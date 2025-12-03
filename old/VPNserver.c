/*
Code take from Minimalistic HTTP server in C
needs to still be modified
*/


#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>  // for multi-threading

#define MAXPENDING 5

void handleHTTPClient(int clntSock);

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

int setupServerSocket(unsigned short fileServPort)
{
	int servSockAddr;
	struct sockaddr_in localServAddr; // Local address

	/* Create socket for incoming connections */
	// note SOCK_DGRAM for UDP not SOCK_STREAM for TCP
	if ((servSockAddr = socket(PF_INET, SOCK_DGRAM, 0/*IPPROTO_UDP*/)) < 0)
		DieWithError("socket() failed");
  
	/* Construct local address structure */
	memset(&localServAddr, 0, sizeof(localServAddr));   /* Zero out structure */
	localServAddr.sin_family = AF_INET;                /* Internet address family */
	localServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
	localServAddr.sin_port = htons(fileServPort);      /* Local port */

	/* Bind to the local address */
	if (bind(servSockAddr, (struct sockaddr *) &localServAddr, sizeof(localServAddr)) < 0)
		DieWithError("bind() failed");

	/* Mark the socket so it will listen for incoming connections */
	if (listen(servSockAddr, MAXPENDING) < 0)
		DieWithError("listen() failed");
	return servSockAddr;
}

void* spawnHandlerThread(void* arg)
{
	pthread_detach(pthread_self());
	
	printf("start--------------------\n");
	
	int * clntSock = (int *)arg;
	handleHTTPClient(*clntSock);
	
	printf("end--------------------\n");
	
	pthread_exit(NULL);
}

void listenLoop(int servSock)
{
	struct sockaddr_in clientSockAddr; // Client address
	unsigned int clntLen; // Length of client address data structure
	int clntSock; // socket descriptor for client
	pthread_t ptid; // for the thread

	for (;;)
	{
		/* Set the size of the in-out parameter */
		clntLen = sizeof(clientSockAddr);

		/* Wait for a client to connect */
		if ((clntSock = accept(servSock, (struct sockaddr *) &clientSockAddr, &clntLen)) < 0)
		{
			DieWithError("accept() failed");
		}
		    
		/* clntSock is connected to a client! */
		printf("Handling client %s\n", inet_ntoa(clientSockAddr.sin_addr));

		// start thread to handle client
		pthread_create(&ptid, NULL, &spawnHandlerThread, &clntSock);
	}
}

int main(int argc, char *argv[])
{
    if (argc != 2)     /* Test for correct number of arguments */
    {
        fprintf(stderr, "Usage:  %s <Server Port>\n", argv[0]);
        exit(1);
    }

	int servSock = setupServerSocket(atoi(argv[1]));

    listenLoop(servSock);
    /* NOT REACHED */
}
