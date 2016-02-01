all: thread
thread:
	g++ -o thread thread.cpp -pthread
clean:
	rm thread
