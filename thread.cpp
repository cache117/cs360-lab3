#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <iostream>

sem_t empty, full, mutex;
class ThreadQueue
{
    std: queue<int>stlqueue;
public:
    void push(int sock)
    {
        sem_wait(&empty);
        sem_wait(&mutex);
        stlqueue.push(sock);
        sem_post(&mutex);
        sem_post(&full);
    }

    int pop()
    {
        sem_wait(&full);
        sem_wait(&mutex);
        int rval = stlqueue.front();
        stlqueue.pop();
        sem_post(&mutex);
        sem_post(&empty);
        return (rval);
    }
} sockqueue;

void *howdy(void *arg)
{
    int threadId;
    threadId = (long) arg;
    printf("Hi %d\n", threadId);
}

int main()
{
#define NTHREADS 10
#define NQUEUE     20
    long threadid;
    pthread_t threads[NTHREADS];
    sem_init(&mutex, PTHREAD_PROCESS_PRIVATE, 1);
    sem_init(&full, PTHREAD_PROCESS_PRIVATE, 0);
    sem_init(&empty, PTHREAD_PROCESS_PRIVATE, NQUEUE);


    for (int i = 0; i < NTHREADS; i++)
    {
        sockqueue.push(i);
    }
    for (int i = 0; i < NTHREADS; i++)
    {
        std::cout << "GOT" << sockqueue.pop() << std::endl;
    }
    for (threadid = 0; threadid < NTHREADS; threadid++)
    {
        pthread_create(&threads[threadid], NULL, howdy, (void *) threadid);
    }/*
	for(;;)
	{
		fd = accept
		enqueue(fd)
	}	*/
    pthread_exit(NULL);
}
