#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

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
#define NTHREADS 20
	long threadid;
	pthread_t threads[NTHREADS];

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
