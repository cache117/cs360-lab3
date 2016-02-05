#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <queue>
#include <semaphore.h>;
#include <sys/types.h>
#include <sys/signal.h>

#define SOCKET_ERROR        -1
#define BUFFER_SIZE         1000
#define QUEUE_SIZE          5
#define NAME_SIZE           255
#define NQUEUE 20

int hSocket, hServerSocket;
/* handle to socket */
struct hostent *pHostInfo;
/* holds info about a machine */
struct sockaddr_in Address;
/* Internet socket address stuct */
int nAddressSize = sizeof(struct sockaddr_in);
char pBuffer[BUFFER_SIZE];
int nHostPort, nThreads;
char startingDirectory[NAME_SIZE];
char filePath[NAME_SIZE];

void parseArguments(int argc, char *argv[]);

void setupServer();

void *parseRequest();

void parseImageFile(int hSocket, char *filePath, size_t size);

void parseTextFile(int hSocket, char *filePath, size_t size);

void sendBadRequest();

void closeSocket(int hSocket);

void signalHandler(int status);

void handleSignals();

void initializeThreadedQueue();

sem_t work_to_do, space_on_q, mutex;

class ThreadedQueue
{
    std::queue<int> threadedQueue;
public:
    void push(int sock)
    {
        sem_wait(&space_on_q);
        sem_wait(&mutex);
        threadedQueue.push(sock);
        sem_post(&mutex);
        sem_post(&work_to_do);
    }

    int pop()
    {
        sem_wait(&work_to_do);
        sem_wait(&mutex);
        int front = threadedQueue.front();
        threadedQueue.pop();
        sem_post(&mutex);
        sem_post(&space_on_q);

        return front;
    }

} socketQueue;

int main(int argc, char *argv[])
{
#define DEBUG
    printf("Starting Server");
    handleSignals();
    parseArguments(argc, argv);
    initializeThreadedQueue();
    pthread_t thread[nThreads];
    for (long x = 0; x < nThreads; ++x)
    {
        printf("Creating thread %ld", x);
        pthread_create(&thread[x], NULL, parseRequest, (void *) x);
    }
    setupServer();
    parseRequest();
}

void initializeThreadedQueue()
{
    sem_init(&full, PTHREAD_PROCESS_PRIVATE, 0);
    sem_init(&empty, PTHREAD_PROCESS_PRIVATE, NQUEUE);
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
    printf("\nStarting server on port %d in startingDirectory %s", nHostPort, startingDirectory);

    printf("\nMaking socket");
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

    printf("\nBinding to port %d\n", nHostPort);

    /* bind to a port */
    if (bind(hServerSocket, (struct sockaddr *) &Address, sizeof(Address))
        == SOCKET_ERROR)
    {
        printf("\nCould not connect to host\n");
        exit(1);
    }
    /*  get port number */
    getsockname(hServerSocket, (struct sockaddr *) &Address, (socklen_t * ) & nAddressSize);
    printf("opened socket as fd (%d) on port (%d) for stream i/o\n", hServerSocket, ntohs(Address.sin_port));
#ifdef DEBUG
    printf("Server\n\
              sin_family        = %d\n\
              sin_addr.s_addr   = %d\n\
              sin_port          = %d\n", Address.sin_family, (int) Address.sin_addr.s_addr, ntohs(Address.sin_port)
    );

    printf("\nMaking a listen queue of %d elements", QUEUE_SIZE);
#endif
    /* establish listen queue */
    if (listen(hServerSocket, QUEUE_SIZE) == SOCKET_ERROR)
    {
        printf("\nCould not listen\n");
        exit(1);
    }
    int optval = 1;
    setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void listen()
{
    for (; ;)
    {
        int hSocket;
        printf("\nWaiting for a connection\n");
        /* get the connected socket */
        hSocket = accept(hServerSocket, (struct sockaddr *) &Address, (socklen_t * ) & nAddressSize);
        socketQueue.push(hSocket);
    }
}

void *parseRequest()
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
        printf("Got from browser %d\n%s\n", bytesRead, pBuffer);
        char requestedFile[NAME_SIZE];
        sscanf(pBuffer, "GET %s HTTP/1.1", requestedFile);
        if (requestedFile != NULL)
        {
            char *filePath = (char *) malloc(1 + strlen(startingDirectory) + strlen(requestedFile));
            memset(filePath, 0, sizeof(filePath));
            strcpy(filePath, startingDirectory);
            strcat(filePath, requestedFile);

            printf("Requested file: %s\n", requestedFile);

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
    printf("%s is a directory \n", filePath);
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
    closedir(directory);
    if (indexFileFound)
    {
        printf("Printing index.html file");
        char *indexFilePath;
        asprintf(&indexFilePath, "%s/%s", filePath, "index.html");
        struct stat fileStat;

        stat(filePath, &fileStat);
        parseTextFile(hSocket, indexFilePath, (size_t) fileStat.st_size);
    }
    else
    {
        asprintf(pBuffer, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n%s", directoryListing);
    }
    char *response;


    write(hSocket, pBuffer, strlen(pBuffer));
    // Free memory, close directory
    free(directoryListing);
    (void) closedir(directory);
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
    printf("File body: %s", fileBody);
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
    write(hSocket, pBuffer, strlen(pBuffer));
    closeSocket(hSocket);
}

void sendBadRequest(int hSocket)
{
    printf("ERROR with file: %s\n", filePath);
    char *response;
    asprintf(&response, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html>"
            "<h1>400 Bad request</h1>"
            "The page '%s' could not be found on this server.\n</html>", filePath);
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
    printf("received signal %d\n", status);
}
