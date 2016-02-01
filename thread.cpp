#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <iostream>

class ThreadQueue
{
	std:queue<int> stlqueue;
	public:
	void push(int sock)
	{
		stlqueue.push(sock);
	}
	int pop()
	{
		int rval = stlqueue.front();
		stlqueue.pop();
		return rval;
	}
} sockqueue;

void *howdy(void *arg)
{
	sock = dequeue();
	Read Request
	write response
	close

	int threadId;
	threadId = (long) arg;
	printf("Hi %d\n", threadId);
}
int main()
{
#define NTHREADS 10
#define NQUEUE	 20
	long threadid;
	pthread_t threads[NTHREADS];
	
	for (int i = 0; i < NTHREADS; i++)
	{
		sockqueue.push(i);
	}
	for(int i = 0; i < NTHREADS; i++)
	{
		std::cout << "GOT" << sockqueue.pop() << std::endl;
	}
	for(threadid = 0; threadid < NTHREADS; threadid++)
	{
		pthread_create(&threads[threadid], NULL, howdy, (void *) threadid);
	}
	for(;;)
	{
		fd = accept
		enqueue(fd)
	}	
	pthread_exit(NULL);
}
