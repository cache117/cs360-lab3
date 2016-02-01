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
#include <pthread.h>


#define SOCKET_ERROR        -1
#define BUFFER_SIZE         1000
#define QUEUE_SIZE          5
#define NAME_SIZE           255
#define CONTENT_TYPE_SIZE   10

int main(int argc, char *argv[])
{
    int hSocket, hServerSocket;  /* handle to socket */
    struct hostent *pHostInfo;   /* holds info about a machine */
    struct sockaddr_in Address; /* Internet socket address stuct */
    int nAddressSize = sizeof(struct sockaddr_in);
    char pBuffer[BUFFER_SIZE];
    int nHostPort;
    char startingDirectory[NAME_SIZE];
    char filePath[NAME_SIZE];

    if (argc < 3)
    {
        printf("\nUsage: server host-port startingDirectory\n");
        return 0;
    }
    else
    {
        nHostPort = atoi(argv[1]);
        strcpy(startingDirectory, argv[2]);
        memset(filePath, 0, sizeof(filePath));
        strcpy(filePath, startingDirectory);
    }

    printf("\nStarting server on port %d in startingDirectory %s", nHostPort, startingDirectory);

    printf("\nMaking socket");
    /* make a socket */
    hServerSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (hServerSocket == SOCKET_ERROR)
    {
        printf("\nCould not make a socket\n");
        return 0;
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
        return 0;
    }
    /*  get port number */
    getsockname(hServerSocket, (struct sockaddr *) &Address, (socklen_t * ) & nAddressSize);
    printf("opened socket as fd (%d) on port (%d) for stream i/o\n", hServerSocket, ntohs(Address.sin_port));

    printf("Server\n\
              sin_family        = %d\n\
              sin_addr.s_addr   = %d\n\
              sin_port          = %d\n", Address.sin_family, (int) Address.sin_addr.s_addr, ntohs(Address.sin_port)
    );

    printf("\nMaking a listen queue of %d elements", QUEUE_SIZE);
    /* establish listen queue */
    if (listen(hServerSocket, QUEUE_SIZE) == SOCKET_ERROR)
    {
        printf("\nCould not listen\n");
        return 0;
    }
    int optval = 1;
    setsockopt(hSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    for (; ;)
    {
        printf("\nWaiting for a connection\n");
        /* get the connected socket */
        hSocket = accept(hServerSocket, (struct sockaddr *) &Address, (socklen_t * ) & nAddressSize);

        printf("\nGot a connection from %X (%d)\n",
               Address.sin_addr.s_addr,
               ntohs(Address.sin_port));
        memset(pBuffer, 0, sizeof(pBuffer));
        int bytesRead = read(hSocket, pBuffer, BUFFER_SIZE);
        printf("Got from browser %d\n%s\n", bytesRead, pBuffer);
        char requestedFile[NAME_SIZE];
        sscanf(pBuffer, "GET %s HTTP/1.1", requestedFile);
        memset(filePath, 0, sizeof(filePath));
        strcat(filePath, startingDirectory);
        strcat(filePath, requestedFile);
        printf("Requested file: %s\n", requestedFile);
        memset(requestedFile, 0, strlen(requestedFile));

        /* analyse given directory */
        struct stat fileStat;
        char contentType[CONTENT_TYPE_SIZE];
        if (stat(filePath, &fileStat))
        {
            printf("ERROR with file: %s\n", filePath);
            memset(pBuffer, 0, sizeof(pBuffer));
            sprintf(pBuffer, "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html>"
                    "<h1>404 Not Found</h1>"
                    "The page '%s' could not be found on this server.\n</html>", filePath);
            write(hSocket, pBuffer, strlen(pBuffer));
        }
        else if (S_ISREG(fileStat.st_mode))
        {
            printf("%s is a regular file \n", filePath);
            printf("File size: %d\n", (int) fileStat.st_size);
            FILE *file = fopen(filePath, "r");
            char *fileBuffer = (char *) malloc((size_t) fileStat.st_size);
            char *headers;
            fread(fileBuffer, (size_t) fileStat.st_size, 1, file);


            sprintf(pBuffer, "HTTP/1.1 200 OK\r\n");
            if (strstr(filePath, ".html"))
            {
                strcpy(contentType, "text/html");
                asprintf(&headers, "%sContent-Type:%s", pBuffer, contentType);
            }
            else if (strstr(filePath, ".gif"))
            {
                strcpy(contentType, "image/gif");
                asprintf(&headers, "%sAccept-Ranges: bytes\r\n", pBuffer);
                asprintf(&headers, "%sContent-length: %d\r\n", pBuffer, (int) fileStat.st_size);
                asprintf(&headers, "%sContent-Type: %s", pBuffer, contentType);
            }
            else if (strstr(filePath, ".jpg"))
            {
                strcpy(contentType, "image/jpg");
                asprintf(&headers, "%sAccept-Ranges: bytes\r\n", pBuffer);
                asprintf(&headers, "%sKeep-Alive: timeout=2, max=100\r\n", pBuffer);
                asprintf(&headers, "%sContent-length: %d\r\n", pBuffer, (int) fileStat.st_size);
                asprintf(&headers, "%sConnection: keep-alive\r\n", pBuffer);
                asprintf(&headers, "%sContent-Type: %s", pBuffer, contentType);
            }
            else
            {
                strcpy(contentType, "text/plain");
                asprintf(&headers, "%sAccept-Ranges: bytes\r\n", pBuffer);
                asprintf(&headers, "%sContent-length:%d\r\n", pBuffer, (int) fileStat.st_size);
                asprintf(&headers, "%sContent-Type: %s", pBuffer, contentType);
            }
            printf("Content-Type: %s", contentType);
            char *preBody;
            asprintf(&preBody, "HTTP/1.1 200 OK\r\n%s\r\n\r\n", headers);
            write(hSocket, preBody, strlen(preBody));
            printf("Writing to socket: \n\n%s", fileBuffer);
            write(hSocket, fileBuffer, strlen(fileBuffer));
            // Free memory, close files
            free(fileBuffer);
            fclose(file);
        }
        else if (S_ISDIR(fileStat.st_mode))
        {
            printf("%s is a directory \n", filePath);
            DIR *dirp;
            struct dirent *dp;
            char *directoryListing;
            dirp = opendir(filePath);
            asprintf(&directoryListing, "<html><h1>File listing:</h1><ul>");
            while ((dp = readdir(dirp)) != NULL)
            {
                asprintf(&directoryListing, "%s\n<li><a href=\"%s\">%s</a></li>", directoryListing, dp->d_name,
                        dp->d_name);
            }
            asprintf(&directoryListing, "%s</ul></html>", directoryListing);
            memset(pBuffer, 0, sizeof(pBuffer));
            sprintf(pBuffer, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n%s", directoryListing);

            write(hSocket, pBuffer, strlen(pBuffer));
            // Free memory, close directory
            free(directoryListing);
            (void) closedir(dirp);
        }
        else
        {
            printf("ERROR with file: %s\n", filePath);
            memset(pBuffer, 0, sizeof(pBuffer));
            sprintf(pBuffer, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n\r\n<html>"
                    "<h1>400 Bad request</h1>"
                    "The page '%s' could not be found on this server.\n</html>", filePath);
        }

        linger lin;
        unsigned int y = sizeof(lin);
        lin.l_onoff = 1;
        lin.l_linger = 10;
        setsockopt(hSocket, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));
        shutdown(hSocket, SHUT_RDWR);
        printf("\nClosing the socket");
        /* close socket */
        if (close(hSocket) == SOCKET_ERROR)
        {
            printf("\nCould not close socket\n");
            return 0;
        }
    }
}
