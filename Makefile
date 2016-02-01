all: thread
thread:
	g++ -o thread thread.cpp -pthread
server:
	g++ -o server server.cpp -pthread
clean:
	rm thread
