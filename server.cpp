#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <dirent.h>
#include <queue>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>

using namespace std;

#define SOCKET_ERROR        -1
#define BUFFER_SIZE         1000
#define QUEUE_SIZE          5
#define NAME_SIZE           255
#define NQUEUE              20

int hServerSocket;
/* handle to socket */
struct sockaddr_in Address;
/* Internet socket address stuct */
int nAddressSize = sizeof(struct sockaddr_in);
int nHostPort, nThreads;
char startingDirectory[NAME_SIZE];
char filePath[NAME_SIZE];

void parseArguments(int argc, char *argv[]);

void setupServer();

void *parseRequest(void *arg);

void parseGetFile(int hSocket, char *filePath, size_t size);

void parseImageFile(int hSocket, char *filePath, size_t size);

void parseTextFile(int hSocket, char *filePath, size_t size);

void parseDirectory(int hSocket, char *filePath);

void sendBadRequest();

void closeSocket(int hSocket);

void signalHandler(int status);

void handleSignals();

void closeSocket(int hSocket);

void sendBadRequest(int hSocket);

void sendFileNotFound(int hSocket);

void initializeThreadQueue();

void listenForConnection();

sem_t workToDo, spaceOnQueue, mutex;

class ThreadedQueue
{
    std::queue<int> threadedQueue;
public:
    void push(int sock)
    {
        sem_wait(&spaceOnQueue);
        sem_wait(&mutex);
        threadedQueue.push(sock);
        sem_post(&mutex);
        sem_post(&workToDo);
    }

    int pop()
    {
        sem_wait(&workToDo);
        sem_wait(&mutex);
        int front = threadedQueue.front();
        threadedQueue.pop();
        sem_post(&mutex);
        sem_post(&spaceOnQueue);

        return front;
    }
} socketQueue;

int main(int argc, char *argv[])
{
#define DEBUG
#ifdef DEBUG
    printf("Starting Server\n");
#endif
    handleSignals();
    parseArguments(argc, argv);
    initializeThreadQueue();
    pthread_t thread[nThreads];
    long threadID;
    for (threadID = 0; threadID < nThreads; ++threadID)
    {
#ifdef DEBUG
        printf("Creating thread: %ld\n", threadID);
#endif
        pthread_create(&thread[threadID], NULL, parseRequest, (void *) threadID);
    }
    setupServer();
    listenForConnection();
}

void initializeThreadQueue()
{
    sem_init(&workToDo, PTHREAD_PROCESS_PRIVATE, 0);
    sem_init(&spaceOnQueue, PTHREAD_PROCESS_PRIVATE, NQUEUE);
    sem_init(&mutex, PTHREAD_PROCESS_PRIVATE, 1);
}

void parseArguments(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("\nUsage: server host-port number-of-threads starting-directory\n");
        exit(1);
    }
    else
    {
        nHostPort = atoi(argv[1]);
        nThreads = atoi(argv[2]);
        strcpy(startingDirectory, argv[3]);
        memset(filePath, 0, sizeof(filePath));
        strcpy(filePath, startingDirectory);
    }
}

void setupServer()
{
    struct hostent *pHostInfo;   /* holds info about a machine */
#ifdef DEBUG
    printf("\nStarting server on port %d in startingDirectory %s", nHostPort, startingDirectory);

    printf("\nMaking socket");
#endif
    /* make a socket */
    hServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (hServerSocket == SOCKET_ERROR)
    {
        printf("\nCould not make a socket\n");
        exit(1);
    }

    /* fill address struct */
    Address.sin_addr.s_addr = INADDR_ANY;
    Address.sin_port = htons(nHostPort);
    Address.sin_family = AF_INET;
#ifdef DEBUG
    printf("\nBinding to port %d\n", nHostPort);
#endif
    /* bind to a port */
    if (bind(hServerSocket, (struct sockaddr *) &Address, sizeof(Address)) == SOCKET_ERROR)
    {
        printf("\nCould not connect to host\n");
        exit(1);
    }
    /*  get port number */
    getsockname(hServerSocket, (struct sockaddr *) &Address, (socklen_t * ) & nAddressSize);
#ifdef DEBUG
    printf("opened socket as fd (%d) on port (%d) for stream i/o\n", hServerSocket, ntohs(Address.sin_port));
    printf("Server\n\
              sin_family        = %d\n\
              sin_addr.s_addr   = %d\n\
              sin_port          = %d\n", Address.sin_family, (int) Address.sin_addr.s_addr, ntohs(Address.sin_port)
    );

    printf("\nMaking a listenForConnection queue of %d elements", QUEUE_SIZE);
#endif
    /* establish listen queue */
    if (listen(hServerSocket, QUEUE_SIZE) == SOCKET_ERROR)
    {
        printf("\nCould not listenForConnection\n");
        exit(1);
    }
    int optval = 1;
    setsockopt(hServerSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void listenForConnection()
{
    for (; ;)
    {
        int hSocket;
#ifdef DEBUG
        printf("\nWaiting for a connection\n");
#endif
        /* get the connected socket */
        hSocket = accept(hServerSocket, (struct sockaddr *) &Address, (socklen_t * ) & nAddressSize);
        socketQueue.push(hSocket);
    }
}

void *parseRequest(void *arg)
{
    for (; ;)
    {
        int hSocket = socketQueue.pop();
        linger lin;
        unsigned int y = sizeof(lin);
        lin.l_onoff = 1;
        lin.l_linger = 10;
        setsockopt(hSocket, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));
#ifdef DEBUG
        printf("\nGot a connection from %X (%d)\n",
               Address.sin_addr.s_addr,
               ntohs(Address.sin_port));
#endif
        char pBuffer[BUFFER_SIZE];
        int bytesRead = read(hSocket, pBuffer, BUFFER_SIZE);
#ifdef DEBUG
        printf("Got from browser %d\n%s\n", bytesRead, pBuffer);
#endif
        char requestedFile[NAME_SIZE];
        sscanf(pBuffer, "GET %s HTTP/1.1", requestedFile);
        if (requestedFile != NULL)
        {
#ifdef DEBUG
            printf("Handling server GET %s request on thread %ld\n", requestedFile, (long) arg);
#endif
            char *filePath = (char *) malloc(1 + strlen(startingDirectory) + strlen(requestedFile));
            memset(filePath, 0, sizeof(filePath));
            strcpy(filePath, startingDirectory);
            strcat(filePath, requestedFile);
#ifdef DEBUG
            printf("Requested file: %s\n", requestedFile);
#endif
            /* analyse given directory */
            struct stat fileStat;

            if (stat(filePath, &fileStat))
            {
                sendFileNotFound(hSocket);
            }
            else if (S_ISREG(fileStat.st_mode))
            {
                parseGetFile(hSocket, filePath, (size_t) fileStat.st_size);
            }
            else if (S_ISDIR(fileStat.st_mode))
            {
                parseDirectory(hSocket, filePath);
            }
        }
        else
        {
            sendBadRequest();
        }
    }

}

void parseDirectory(int hSocket, char *filePath)
{
#ifdef DEBUG
    printf("%s is a directory \n", filePath);
#endif
    DIR *directory;
    struct dirent *directoryEntry;
    char *directoryListing;
    directory = opendir(filePath);
    asprintf(&directoryListing, "<html><h1>File listing:</h1><ul>");
    int indexFileFound = 0;
    while ((directoryEntry = readdir(directory)) != NULL)
    {
        if (strcmp(directoryEntry->d_name, "index.html") == 0)
        {
            indexFileFound = 1;
            break;
        }
        if (strcmp(directoryEntry->d_name, ".") != 0 && strcmp(directoryEntry->d_name, "..") != 0)
        {
            asprintf(&directoryListing, "%s\n<li><a href=\"%s\">%s</a></li>", directoryListing, directoryEntry->d_name,
                     directoryEntry->d_name);
        }
    }
    asprintf(&directoryListing, "%s</ul></html>", directoryListing);
    (void) closedir(directory);
    if (indexFileFound)
    {
#ifdef DEBUG
        printf("\nPrinting index.html file\n");
#endif
        char *indexFilePath;
        asprintf(&indexFilePath, "%s/%s", filePath, "index.html");

        struct stat fileStat;
        stat(filePath, &fileStat);
        parseTextFile(hSocket, indexFilePath, (size_t) fileStat.st_size);
    }
    else
    {
        char *response;
        asprintf(&response, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n%s", directoryListing);
        write(hSocket, response, strlen(response));

        // Free memory
        free(directoryListing);
    }
}

void parseGetFile(int hSocket, char *filePath, size_t size)
{
#ifdef DEBUG
    printf("%s is a regular file \n", filePath);
    printf("File size: %d\n", (int) size);
#endif
    if (strstr(filePath, ".gif") || strstr(filePath, ".jpg"))
    {
        parseImageFile(hSocket, filePath, size);
    }
    else
    {
        parseTextFile(hSocket, filePath, size);
    }
}

void parseTextFile(int hSocket, char *filePath, size_t size)
{
    char *headers;
    if (strstr(filePath, ".html"))
    {
        asprintf(&headers, "Content-Type: text/html");
    }
    else
    {
        asprintf(&headers, "Accept-Ranges: bytes\r\n");
        asprintf(&headers, "%sContent-length: %d\r\n", headers, (int) size);
        asprintf(&headers, "%sContent-Type: text/plain", headers);
    }

    FILE *file;
    file = fopen(filePath, "r");
    char *fileBody;
    fileBody = (char *) malloc(size);
    int i = 0;
    int nextCharacter;
    while ((nextCharacter = getc(file)) != EOF)
    {
        fileBody[i++] = (char) nextCharacter;
    }
#ifdef HEAVY_DEBUG
    printf("File body: %s", fileBody);
#endif
    char *response;
    asprintf(&response, "HTTP/1.1 200 OK\r\n%s\r\n\r\n%s", headers, fileBody);
    fclose(file);

    write(hSocket, response, strlen(response));
    closeSocket(hSocket);

}

void parseImageFile(int hSocket, char *filePath, size_t size)
{
    char imageBody[size];
    char *headers;
    if (strstr(filePath, ".gif"))
    {
        asprintf(&headers, "Accept-Ranges: bytes\r\n");
        asprintf(&headers, "%sContent-length: %d\r\n", headers, (int) size);
        asprintf(&headers, "%sContent-Type: image/gif", headers);
    }
    else if (strstr(filePath, ".jpg"))
    {
        asprintf(&headers, "Accept-Ranges: bytes\r\n");
        asprintf(&headers, "%sKeep-Alive: timeout=2, max=100\r\n", headers);
        asprintf(&headers, "%sContent-length: %d\r\n", headers, (int) size);
        asprintf(&headers, "%sContent-Type: image/jpeg\r\n", headers);
        asprintf(&headers, "%sConnection: keep-alive", headers);
    }

    FILE *image;

    unsigned long fileLength;

    //rb = read bytes. Needed for images
    image = fopen(filePath, "rb");
    if (!image)
    {
        fprintf(stderr, "Unable to open file %s", filePath);
        exit(1);
    }

    int readValue = fread(imageBody, 1, size, image);
    fclose(image);
    char *preBody;
    asprintf(&preBody, "HTTP/1.1 200 OK\r\n%s\r\n\r\n", headers);
    //The headers are text, but the image body is full of binary, which could be interpreted as EOF commands.
    write(hSocket, preBody, strlen(preBody));
    write(hSocket, imageBody, sizeof(imageBody));
    closeSocket(hSocket);
}

void closeSocket(int hSocket)
{
    /* close socket */
    shutdown(hSocket, SHUT_RDWR);
    if (close(hSocket) == SOCKET_ERROR)
    {
        printf("\nCould not close socket\n");
        exit(1);
    }
}

void sendFileNotFound(int hSocket)
{
    printf("ERROR with file: %s\n", filePath);
    char *response;
    asprintf(&response, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html>"
            "<h1>404 Not Found</h1>"
            "The page '%s' could not be found on this server.\n</html>", filePath);
    write(hSocket, response, strlen(response));
    closeSocket(hSocket);
}

void sendBadRequest(int hSocket)
{
    printf("ERROR with file: %s\n", filePath);
    char *response;
    asprintf(&response, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html>"
            "<h1>400 Bad request</h1></html>");
    write(hSocket, response, sizeof(response));
    closeSocket(hSocket);
}

void handleSignals()
{
    int rc1, rc2;

    // First set up the signal handler
    struct sigaction sigOld, sigNew, sigNew2;

    sigNew.sa_handler = signalHandler;
    //signew2.sa_handler=intSignalHandler;
    sigemptyset(&sigNew.sa_mask);
    sigaddset(&sigNew.sa_mask, SIGINT);
    sigNew.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigNew2, &sigOld);
    sigaction(SIGHUP, &sigNew, &sigOld);
    sigaction(SIGPIPE, &sigNew, &sigOld);
}

void signalHandler(int status)
{
    printf("rReceived signal: %d\n", status);
}
