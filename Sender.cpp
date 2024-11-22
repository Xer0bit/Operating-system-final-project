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
#endif

#include <fstream>
#include <iostream>

using std::cout;
using std::cin;

#ifdef _WIN32
    typedef HANDLE sync_handle_t;
#else
    typedef sem_t* sync_handle_t;
#endif

void processMessages(const std::string& file_name, sync_handle_t hStartEvent, sync_handle_t hInputReadySemaphore, sync_handle_t hOutputReadySemaphore, sync_handle_t hMutex);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <file_name>\n";
        return 1;
    }

    std::string file_name = argv[1];
    std::fstream file;
    sync_handle_t hStartEvent;

    #ifdef _WIN32
        hStartEvent = OpenEvent(EVENT_MODIFY_STATE, FALSE, "StartProcess");
    #else
        // Open existing semaphore, don't create
        hStartEvent = sem_open("/start_process", 0);
    #endif

    if (hStartEvent == NULL || hStartEvent == SEM_FAILED) {
        cout << "Open event failed. \nPress any key to exit.\n";
        cin.get();
        return 1;
    }

    // Wait for Receiver to signal semaphores are ready
    #ifdef _WIN32
        WaitForSingleObject(hStartEvent, INFINITE);
    #else
        sem_wait(hStartEvent);
    #endif

    sync_handle_t hInputReadySemaphore;
    #ifdef _WIN32
        hInputReadySemaphore = OpenSemaphore(EVENT_ALL_ACCESS, FALSE, "EnterSemaphoreStarted");
    #else
        hInputReadySemaphore = sem_open("/input_sem", 0);
    #endif

    if (hInputReadySemaphore == NULL)
        return 1;

    sync_handle_t hOutputReadySemaphore;
    #ifdef _WIN32
        hOutputReadySemaphore = OpenSemaphore(EVENT_ALL_ACCESS, FALSE, "OutputSemaphoreStarted");
    #else
        hOutputReadySemaphore = sem_open("/output_sem", O_CREAT, 0644, 0);
    #endif

    if (hOutputReadySemaphore == NULL)
        return 1;

    sync_handle_t hMutex;
    #ifdef _WIN32
        hMutex = OpenMutex(SYNCHRONIZE, FALSE, "FileAccessMutex");
    #else
        hMutex = sem_open("/mutex_sem", O_CREAT, 0644, 1);
    #endif

    #ifdef _WIN32
        SetEvent(hStartEvent);
    #else
        sem_post(hStartEvent);
    #endif

    cout << "Event was started\n";

    processMessages(file_name, hStartEvent, hInputReadySemaphore, hOutputReadySemaphore, hMutex);

    #ifdef _WIN32
        CloseHandle(hInputReadySemaphore);
        CloseHandle(hOutputReadySemaphore);
        CloseHandle(hMutex);
    #else
        sem_close(hInputReadySemaphore);
        sem_close(hOutputReadySemaphore);
        sem_close(hMutex);
    #endif

    return 0;
}

void processMessages(const std::string& file_name, sync_handle_t hStartEvent, sync_handle_t hInputReadySemaphore, sync_handle_t hOutputReadySemaphore, sync_handle_t hMutex) {
    std::fstream file;
    cout << "Print 1 to write message;\nInput 0 to exit process\n";
    std::string key;
    cin >> key;

    while (true) {
        if (key == "1") {
            #ifdef _WIN32
                WaitForSingleObject(hMutex, INFINITE);
            #else
                sem_wait(hMutex);
            #endif
            file.open(file_name, std::ios::out | std::ios::app);

            std::string msg;
            cout << "Type in message: ";
            cin >> msg;

            char message[21];
            for (size_t i = 0; i < msg.length(); i++) {
                message[i] = msg[i];
            }

            for (size_t i = msg.length(); i < 21; i++) {
                message[i] = '\0';
            }

            message[20] = '\n';
            #ifdef _WIN32
                ReleaseMutex(hMutex);
                ReleaseSemaphore(hOutputReadySemaphore, 1, NULL);
            #else
                sem_post(hMutex);
                sem_post(hOutputReadySemaphore);
            #endif

            #ifdef _WIN32
                if (!ReleaseSemaphore(hInputReadySemaphore, 1, NULL)) {
                    cout << "You have exceeded the number of messages!\n";
                    ReleaseSemaphore(hOutputReadySemaphore, 1, NULL);
                    WaitForSingleObject(hOutputReadySemaphore, INFINITE);
                }
            #else
                if (sem_post(hInputReadySemaphore) == -1) {
                    cout << "You have exceeded the number of messages!\n";
                    sem_post(hOutputReadySemaphore);
                    sem_wait(hOutputReadySemaphore);
                }
            #endif

            for (int i = 0; i < 21; i++) {
                file << message[i];
            }

            file.close();
            cout << "\nInput 1 to write message;\nInput 0 to exit process\n";
            cin >> key;
        }

        if (key != "0" && key != "1") {
            cout << "\nIncorrect value!\nInput 1 to write message;\nInput 0 to exit process\n";
            cin >> key;
        }

        if (key == "0") {
            cout << "Process ended.";
            break;
        }
    }
}
