#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <iostream>
#include "utils.cpp"
#include <pthread.h>
#include <queue>
#include <semaphore.h>
//#include <utils.h>
using namespace std;

#define NTHREADS 10
#define NQUEUE 20
sem_t full, empty, mutex;

class Myqueue{
    std::queue<int> queue;
public:
    void push(int sock){
        sem_wait(&space_on_q);
        sem_wait(&mutex);
        queue.push(sock);

        sem_post(&mutex);
        sem_post(&work_to_do);
    }
    int pop(){
        sem_wait(&work_to_do);
        sem_wait(&mutex);
        int rval = queue.front();
        queue.pop();

        sem_post(&mutex);
        sem_post(&space_on_q);

        return rval;
    }

} socketQueue;




void signalHandler (int status);   /* definition of signal handler */
void handleSignals();

#define SOCKET_ERROR        -1
#define BUFFER_SIZE         100000000
#define QUEUE_SIZE          5
//#define DEBUG             FALSE
char *substring(char *string, int position, int length);
char** str_split(char* a_str, const char a_delim);
void parseGetRequest(int hSocket,struct stat filestat, char* context, char** request);
void getHtml(int hSocket,char* context, size_t size);
void getImage(int hSocket,char* context, size_t size);
void getDirectory(int hSocket,char* context, char** request);
void sendFileNotFound(int hSocket);
void sendBadRequest(int hSocket);
void parseArgs(int argc, char* argv[]);
void listen();
void *parseRequest(void * arg);
void initializeSockQueue();
void configureServer();
int hServerSocket;  /* handle to socket */
int nHostPort;
char *directory;
struct sockaddr_in Address; /* Internet socket address stuct */
int nAddressSize=sizeof(struct sockaddr_in);



int main(int argc, char* argv[])
{
#define DEBUG
    handleSignals();
    parseArgs(argc, argv);
    initializeSockQueue();
    printf("\nStarting server*******************************************************\n");
    pthread_t thread[NTHREADS];
    long x;
    for (x = 0; x < NTHREADS; x++){
        printf("Creating thread number %ld\n", x);
        pthread_create(&thread[x], NULL, parseRequest, (void*) x);
    }
    configureServer();
    listen();

}
void initializeSockQueue()
{
    sem_init(&full, PTHREAD_PROCESS_PRIVATE, 0);
    sem_init(&empty, PTHREAD_PROCESS_PRIVATE, NQUEUE);
    sem_init(&mutex, PTHREAD_PROCESS_PRIVATE, 1);
}
void configureServer()
{
    struct hostent* pHostInfo;   /* holds info about a machine */
    printf("\nMaking socket");
    /* make a socket */
    hServerSocket=socket(AF_INET,SOCK_STREAM,0);
    if(hServerSocket == SOCKET_ERROR)
    {
        printf("\nCould not make a socket\n");
        exit(1);
    }

    /* fill address struct */
    Address.sin_addr.s_addr=INADDR_ANY;
    Address.sin_port=htons(nHostPort);
    Address.sin_family=AF_INET;

    printf("\nBinding to port %d\n",nHostPort);

    /* bind to a port */
    if(bind(hServerSocket,(struct sockaddr*)&Address,sizeof(Address))
       == SOCKET_ERROR)
    {
        printf("\nCould not connect to host\n");
        exit(1);
    }
    /*  get port number */
    getsockname( hServerSocket, (struct sockaddr *) &Address,(socklen_t *)&nAddressSize);
#ifdef DEBUG
    printf("opened socket as fd (%d) on port (%d) for stream i/o\n",hServerSocket, ntohs(Address.sin_port) );
    printf("Server\n\
          sin_family        = %d\n\
          sin_addr.s_addr   = %d\n\
          sin_port          = %d\n"
            , Address.sin_family
            , Address.sin_addr.s_addr
            , ntohs(Address.sin_port)
    );


    printf("\nMaking a listen queue of %d elements",QUEUE_SIZE);
#endif
    /* establish listen queue */
    if(listen(hServerSocket,QUEUE_SIZE) == SOCKET_ERROR)
    {
        perror("\nCould not listen\n");
        exit(1);
    }
    int optval = 1;
    setsockopt(hServerSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    //start litening to the port
}
void listen()
{
    for(;;)
    {
        int hSocket;
        printf("\nWaiting for a connection\n");
        /* get the connected socket */
        hSocket=accept(hServerSocket,(struct sockaddr*)&Address,(socklen_t *)&nAddressSize);
        socketQueue.push(hSocket);
        //parseRequest(hSocket);
    }
}

void *parseRequest(void * arg){
    for(;;)
    {

        int hSocket = socketQueue.pop();
        linger lin;
        unsigned int y=sizeof(lin);
        lin.l_onoff=1;
        lin.l_linger=10;
        setsockopt(hSocket,SOL_SOCKET, SO_LINGER,&lin,sizeof(lin));
#ifdef DEBUG
        printf("\nGot a connection from %X (%d)\n",
               Address.sin_addr.s_addr,
               ntohs(Address.sin_port));
#endif

        /* read from socket into buffer */
        char pBuffer[BUFFER_SIZE];
        memset(pBuffer,0,sizeof(pBuffer));
        read(hSocket,pBuffer,BUFFER_SIZE);

        char** request;
        request = str_split(pBuffer, ' ');
        if(request[0] != NULL && strstr(request[0], "GET") != NULL)
        {
            //     GET / http/1.1 host: address
            char *context = (char *) malloc(1 + strlen(directory) + strlen(request[1]) );
            //strcpy(context, ".");
            memset(context,0,sizeof(context));
            strcpy(context, directory);
            strcat(context, request[1]);
            struct stat filestat;
            if(stat(context, &filestat))
            {
                sendFileNotFound(hSocket);
            }
            else
            {
                parseGetRequest(hSocket, filestat, context, request);
            }
        }
        else
        {
            sendBadRequest(hSocket);
        }
    }
}

void sendFileNotFound(int hSocket)
{
    perror("ERROR in stat\n");
    char *response;
    asprintf(&response, "HTTP/1.1 404 FILE NOT FOUND\r\nHeaders\r\n\r\n%s\n", "<html>Error 404 File Not Found bud</html>");
    write(hSocket,response,(strlen(response)));
    /* close socket */
    shutdown(hSocket, SHUT_RDWR);
    if(close(hSocket) == SOCKET_ERROR)
    {
        printf("\nCould not close socket\n");
        exit(1);
    }
}

void sendBadRequest(int hSocket)
{
    char *response;
    asprintf(&response, "HTTP/1.1 400 Bad Request\r\nHeaders\r\n\r\n<html>Error 400 Bad Request</html>");
    int written = 0;
    write(hSocket,response,(sizeof(response)));
    /* close socket */
    shutdown(hSocket, SHUT_RDWR);
    if(close(hSocket) == SOCKET_ERROR)
    {
        printf("\nCould not close socket\n");
        exit(1);
    }
}

void parseGetRequest(int hSocket,struct stat filestat, char* context, char** request)
{
    if(S_ISREG(filestat.st_mode))
    {
        if(strstr(context,".gif") || strstr(context,".jpg"))
        {
            getImage(hSocket,context, filestat.st_size);
        }
        else
        {
            getHtml(hSocket, context, filestat.st_size);
        }
    }
    else if(S_ISDIR(filestat.st_mode))
    {
        getDirectory(hSocket, context, request);
    }
}

void getDirectory(int hSocket,char* context, char** request)
{
    DIR *dirp;
    struct dirent *dp;
    dirp = opendir(context);
    char* list;
    asprintf(&list, "<html><ul>");
    char *path;
    char ** splitAddress = str_split(request[3],'\r');
    asprintf(&path, "%s/%s/",splitAddress[0],request[1]+1);
    int index = 0;
    while ((dp = readdir(dirp)) != NULL)
    {
        if(strcmp(dp->d_name,".") != 0 && strcmp(dp->d_name,"..") != 0)
        {
            asprintf(&list,"%s<li><a href=\"http://%s%s\">%s</li>\n",list, path, dp->d_name,dp->d_name);
        }
        if(strcmp(dp->d_name,"index.html") == 0){
            index = 1;
        }
        //printf("path: %s, name: %s \n",path,dp->d_name);
    }
    char *response;
    asprintf(&list,"%s</ul></html>",list);
    (void)closedir(dirp);
    if(index == 1)
    {
        printf("Printing index");
        char *headers;
        asprintf(&headers,"Content-Type: text/html");
        char* filepath;
        asprintf(&filepath,"%s/%s",context,"index.html");
        printf("opening index.html,%s",filepath);


        FILE *file;
        file = fopen(filepath, "r");
        fseek(file, 0, SEEK_END);
        int fileSize = ftell(file);
        rewind(file);
        char body[fileSize];
        memset(body,0,sizeof(body));
        int i = 0;
        int c;
        while ((c = getc(file)) != EOF)
        {
            body[i]=c;
            i++;
        }
        body[fileSize] = '\0';
#ifdef DEBUG
        printf("This is the body: %s\n Yeah", body);
#endif

        asprintf(&response, "HTTP/1.1 200 OK\r\n%s\r\n\r\n%s", headers, body);
        fclose(file);
    }
    else
    {
        asprintf(&response, "HTTP/1.1 200 OK\r\nHeaders\r\n\r\n%s", list);
    }
    write(hSocket,response,(strlen(response)));
    /* close socket */
    shutdown(hSocket, SHUT_RDWR);
    if(close(hSocket) == SOCKET_ERROR)
    {
        printf("\nCould not close socket\n");
        exit(1);
    }
}

void getHtml(int hSocket,char* context, size_t size)
{
    char *headers;
    if(strstr(context,".html"))
    {
        asprintf(&headers,"Content-Type: text/html");
    }
    else
    {
        asprintf(&headers,"Accept-Ranges: bytes\r\n");
        asprintf(&headers,"%sContent-length: %d\r\n",headers,(int)size);
        asprintf(&headers,"%sContent-Type: text/plain",headers);
    }
    FILE *file;
    file = fopen(context, "r");
    char *body;
    body = (char *)malloc(size);
    int i = 0;
    int c;
    while ((c = getc(file)) != EOF)
    {
        body[i]=c;
        i++;
    }
    printf("This is the body: %s\n", body);
    char *response;
    asprintf(&response, "HTTP/1.1 200 OK\r\n%s\r\n\r\n%s", headers, body);
    fclose(file);
    int written = 0;
    write(hSocket,response,(strlen(response)));
    /* close socket */
    shutdown(hSocket, SHUT_RDWR);
    if(close(hSocket) == SOCKET_ERROR)
    {
        printf("\nCould not close socket\n");
        exit(1);
    }
}

void getImage(int hSocket,char* context, size_t size)
{
    char body[size];
    char *headers;
    if(strstr(context,".gif"))
    {
        asprintf(&headers,"Accept-Ranges: bytes\r\n");
        asprintf(&headers,"%sContent-length: %d\r\n",headers,(int)size);
        asprintf(&headers,"%sContent-Type: image/gif",headers);
    }
    else if(strstr(context,".jpg"))
    {
        asprintf(&headers,"Accept-Ranges: bytes\r\n");
        asprintf(&headers,"%sKeep-Alive: timeout=2, max=100\r\n",headers);
        asprintf(&headers,"%sContent-length: %d\r\n",headers,(int)size);
        asprintf(&headers,"%sContent-Type: image/jpeg\r\n",headers);
        asprintf(&headers,"%sConnection: keep-alive",headers);
    }
    FILE *file;

    unsigned long fileLen;

    //Open file
    file = fopen(context, "rb");
    if (!file)
    {
        fprintf(stderr, "Unable to open file %s", context);
        exit(1);
    }
    if (!body)
    {
        fprintf(stderr, "Memory error!");
        fclose(file);
        exit(1);
    }

    //Read file contents into buffer
    int readval = fread(body, 1, size, file);
    fclose(file);
    char* preBody;
    asprintf(&preBody, "HTTP/1.1 200 OK\r\n%s\r\n\r\n", headers);
    write(hSocket,preBody,(strlen(preBody)));
    write(hSocket,body,(sizeof(body)));
    /* close socket */
    shutdown(hSocket, SHUT_RDWR);
    if(close(hSocket) == SOCKET_ERROR)
    {
        printf("\nCould not close socket\n");
        exit(1);
    }
}

void parseArgs(int argc, char* argv[])
{
    /*Parse the process arguments*/
    if(argc < 2)
    {
        printf("\nUsage: server host-port directory\n");
        exit(1);
    }
    else
    {
        nHostPort=atoi(argv[1]);
        directory=(char *) malloc(1 + strlen(argv[2]));
        memset(directory,0,sizeof(directory));
        strcpy(directory,argv[2]);
    }
}
char *substring(char *string, int position, int length)
{
    char *pointer;
    int c;

    pointer = (char *)malloc(length+1);

    if (pointer == NULL)
    {
        printf("Unable to allocate memory.\n");
        exit(1);
    }

    for (c = 0 ; c < length ; c++)
    {
        *(pointer+c) = *(string+position-1);
        string++;
    }

    *(pointer+c) = '\0';

    return pointer;
}

void handleSignals()
{
    int rc1, rc2;

    // First set up the signal handler
    struct sigaction sigold, signew, signew2;

    signew.sa_handler=signalHandler;
    //signew2.sa_handler=intSignalHandler;
    sigemptyset(&signew.sa_mask);
    sigaddset(&signew.sa_mask,SIGINT);
    signew.sa_flags = SA_RESTART;
    sigaction(SIGINT,&signew2,&sigold);
    sigaction(SIGHUP,&signew,&sigold);
    sigaction(SIGPIPE,&signew,&sigold);
}

void signalHandler (int status)
{
    printf("received signal %d\n",status);
}

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = (char**)malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            //assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        //assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}