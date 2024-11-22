/*
 * Process Communication Project
 * Operating System Lab Project
 * Presented to: Mr. Muneeb Saleem
 * 
 * This project demonstrates inter-process communication using files and synchronization primitives
 * Compatible with both Windows and Linux platforms
 */

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <semaphore.h>
    #include <unistd.h>
    #include <sys/wait.h>
#endif

#include <fstream>
#include <iostream>
#include <string>

using std::cout;
using std::cin;

#ifdef _WIN32
    typedef HANDLE sync_handle_t;
#else
    typedef sem_t* sync_handle_t;
#endif

// Platform-specific function declarations
#ifdef _WIN32
void CreateSenderProcesses(const std::string& file_name, int number_of_senders, HANDLE* hEventStarted);
void HandleMessages(const std::string& file_name, HANDLE hInputReadySemaphore, HANDLE hOutputReadySemaphore, HANDLE hMutex);
#else
void CreateSenderProcesses(const std::string& file_name, int number_of_senders);
void HandleMessages(const std::string& file_name, sync_handle_t hInputReadySemaphore, sync_handle_t hOutputReadySemaphore, sync_handle_t hMutex);
#endif

int main() {
    std::string file_name;
    int number_of_notes;
    std::fstream file;
    int number_of_senders;

    #ifdef _WIN32
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
    #endif

    cout << "Enter binary file name: ";
    cin >> file_name;
    cout << "Enter number of notes: ";
    cin >> number_of_notes;
    file.open(file_name, std::ios::out);
    file.close();

    cout << "Enter number of Sender Processes: ";
    cin >> number_of_senders;

    #ifdef _WIN32
        HANDLE hInputReadySemaphore = CreateSemaphore(NULL, 0, number_of_notes, "EnterSemaphoreStarted");
        if (hInputReadySemaphore == NULL)
            return GetLastError();
        HANDLE hOutputReadySemaphore = CreateSemaphore(NULL, 0, number_of_notes, "OutputSemaphoreStarted");
        if (hOutputReadySemaphore == NULL)
            return GetLastError();
        HANDLE hMutex = CreateMutex(NULL, FALSE, "FileAccessMutex");
        HANDLE* hEventStarted = new HANDLE[number_of_senders];

        CreateSenderProcesses(file_name, number_of_senders, hEventStarted);

        WaitForMultipleObjects(number_of_senders, hEventStarted, TRUE, INFINITE);
        ReleaseMutex(hMutex);
    #else
        // Cleanup any stale semaphores with more detailed error handling
        sem_unlink("/input_sem");
        sem_unlink("/output_sem");
        sem_unlink("/mutex_sem");
        sem_unlink("/start_process");
        
        // Create semaphores with wider permissions
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
        
        sync_handle_t hInputReadySemaphore = sem_open("/input_sem", O_CREAT, mode, 0);
        sync_handle_t hOutputReadySemaphore = sem_open("/output_sem", O_CREAT, mode, 0);
        sync_handle_t hMutex = sem_open("/mutex_sem", O_CREAT, mode, 1);
        sync_handle_t hStartEvent = sem_open("/start_process", O_CREAT, mode, 0);

        if (hInputReadySemaphore == SEM_FAILED || hOutputReadySemaphore == SEM_FAILED || 
            hMutex == SEM_FAILED || hStartEvent == SEM_FAILED) {
            perror("sem_open failed");
            // Cleanup on error
            sem_unlink("/input_sem");
            sem_unlink("/output_sem");
            sem_unlink("/mutex_sem");
            sem_unlink("/start_process");
            exit(1);
        }

        // Signal that semaphores are ready
        sem_post(hStartEvent);

        CreateSenderProcesses(file_name, number_of_senders);
    #endif

    HandleMessages(file_name, hInputReadySemaphore, hOutputReadySemaphore, hMutex);

    #ifdef _WIN32
        CloseHandle(hInputReadySemaphore);
        CloseHandle(hOutputReadySemaphore);
        for (int i = 0; i < number_of_senders; i++) {
            CloseHandle(hEventStarted[i]);
        }

        delete[] hEventStarted;
    #else
        sem_close(hInputReadySemaphore);
        sem_close(hOutputReadySemaphore);
        sem_close(hMutex);
        sem_unlink("/input_sem");
        sem_unlink("/output_sem");
        sem_unlink("/mutex_sem");
    #endif

    return 0;
}

#ifdef _WIN32
void CreateSenderProcesses(const std::string& file_name, int number_of_senders, HANDLE* hEventStarted) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    for (int i = 0; i < number_of_senders; ++i) {
        std::string sender_cmd = "Sender.exe " + file_name;
        LPSTR lpwstrSenderProcessCommandLine = const_cast<LPSTR>(sender_cmd.c_str());

        ZeroMemory(&si, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        if (!CreateProcess(NULL, lpwstrSenderProcessCommandLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            cout << "The Sender Process is not started.\n";
            exit(GetLastError());
        }
        hEventStarted[i] = CreateEvent(NULL, FALSE, FALSE, "StartProcess");
        if (hEventStarted[i] == NULL)
            exit(GetLastError());
        CloseHandle(pi.hProcess);
    }
}
#else
void CreateSenderProcesses(const std::string& file_name, int number_of_senders) {
    for (int i = 0; i < number_of_senders; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(1);
        }
        if (pid == 0) {  // Child process
            std::string sender_path = "./Sender";
            char* const args[] = {
                const_cast<char*>(sender_path.c_str()),
                const_cast<char*>(file_name.c_str()),
                nullptr
            };
            execv(sender_path.c_str(), args);
            // If execv returns, there was an error
            perror("execv failed");
            exit(1);
        }
    }
    // Parent process continues...
}
#endif

void HandleMessages(const std::string& file_name, sync_handle_t hInputReadySemaphore, sync_handle_t hOutputReadySemaphore, sync_handle_t hMutex) {
    std::fstream file;
    cout << "\nEnter 1 to read message;\nEnter 0 to exit \n";
    int key;
    cin >> key;
    file.open(file_name, std::ios::in);

    while (true) {
        if (key == 1) {
            std::string message;
            #ifdef _WIN32
                WaitForSingleObject(hInputReadySemaphore, INFINITE);
                WaitForSingleObject(hMutex, INFINITE);
            #else
                sem_wait(hInputReadySemaphore);
                sem_wait(hMutex);
            #endif

            std::getline(file, message);
            cout << message;

            #ifdef _WIN32
                ReleaseSemaphore(hOutputReadySemaphore, 1, NULL);
                ReleaseMutex(hMutex);
            #else
                sem_post(hOutputReadySemaphore);
                sem_post(hMutex);
            #endif

            cout << "\nEnter 1 to read message; \nEnter 0 to exit \n";
            cin >> key;
        }
        if (key != 0 && key != 1) {
            cout << "\nValue Error!\nEnter 1 to read message;\nEnter 0 to exit \n";
            cin >> key;
        }
        if (key == 0) {
            cout << "Process ended";
            break;
        }
    }
    file.close();
}
