CXX = g++
CXXFLAGS = -Wall -pthread

all: Sender Receiver

Sender: Sender.cpp
	$(CXX) $(CXXFLAGS) -o Sender Sender.cpp

Receiver: Receiver.cpp
	$(CXX) $(CXXFLAGS) -o Receiver Receiver.cpp

clean:
	rm -f Sender Receiver