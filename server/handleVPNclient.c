/*
Code take from Minimalistic HTTP server in C
needs to still be modified
*/

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for recv() and send() */
#include <unistd.h>     /* for close() */
#include <stdlib.h>
#include <string.h>
#include <time.h>

// server settings
#define DEBUG 1 // breaks with images because no newline
#define MAX_LEN 255
#define WEBFILES "./website"
#define ERROR404 "./website/404.html"
#define DEFAULTPAGE "./website/main.html"

// --------- http header structure ----------
typedef struct httpHeader
{
	char * method;
	char * filename;
	char * protocol;
	char * host;
	char * cookies;
	int contentLength;
	char * messageBody;
} HTTP_HEADER;

void freeHeader(HTTP_HEADER header)
{
	free(header.method);
	free(header.filename);
	free(header.protocol);
	free(header.host);
	free(header.cookies);
	free(header.messageBody);
}

// ------------- basic network IO ------------------
void sendMessage(int clntSock, char * buffer)
{
	send(clntSock, buffer, strlen(buffer), 0);
	if(DEBUG)
		printf("%s", buffer);
}

void recieveLine(int clntSock, char * buffer, int maxLength)
{
	while(maxLength > 0 && recv(clntSock, buffer, 1, 0) > 0 && *buffer != '\n')
	{
		maxLength--;
		buffer++;
	}
	*--buffer='\0'; // idk why but "--" makes it work and it doesn't work without it even when *buffer is '\n' (-_-)
}
// --------------------------------------

long findSize(FILE * fp) 
{
    fseek(fp, 0L, SEEK_END); 
  
    // calculating the size of the file 
    long res = ftell(fp); 
  
	//resetting to the start
	fseek(fp, 0L, SEEK_SET);
	
    return res; 
}

void sendResponseBody(long fileSize, FILE * fp, int clntSock)
{
	sendMessage(clntSock, "\n");
	//Send file to client
	while (fileSize > 0)
	{

		char fileBuffer[1025];
		memset(fileBuffer, 0, 1025);
		if(fileSize > 1024)
		{
			fread(fileBuffer, 1024, 1, fp); // so basically reading from file pointer and writing to file descriptor - there has to be a better way to do this
			fileBuffer[1024] = '\0';
			sendMessage(clntSock, fileBuffer);
			fileSize -= 1024;
		}
		else
		{
			fread(fileBuffer, fileSize, 1, fp);
			fileBuffer[fileSize] = '\0';
			sendMessage(clntSock, fileBuffer);
			fileSize = 0;			
		}
	}
}

// only text types work but the infastructure to support images is there
int sendFileType(int clntSock, char * filename)
{
	char * ext = strrchr(filename, '.') + 1;
	
	char * imageTypes[6] = {"apng", "avif", "gif", "jpeg", "png", "webp"};
	int simpleImage = 1;
	for(int i = 0; i < 6; i++)
	{
		simpleImage *= strcmp(ext, imageTypes[i]);
	}
	
	if (strcmp(ext, "html") == 0)
	{
		sendMessage(clntSock, "Content-Type: text/html\n");
	}
	else if (strcmp(ext, "txt") == 0)
	{
		sendMessage(clntSock, "Content-Type: text/plain\n");
	}
	else if (strcmp(ext, "js") == 0)
	{
		sendMessage(clntSock, "Content-Type: text/javascript\n");
	}
	else if (simpleImage == 0)
	{
		sendMessage(clntSock, "Content-Type: image/");
		sendMessage(clntSock, ext);
		sendMessage(clntSock, "\n");
	}
	else if (strcmp(ext, "jpg")*strcmp(ext, "jfif")*strcmp(ext, "pjpeg")*strcmp(ext, "pjp") == 0)
	{
		sendMessage(clntSock, "Content-Type: image/jpeg\n");
	}
	else if (strcmp(ext, "svg") == 0)
	{
		sendMessage(clntSock, "Content-Type: image/svg+xml\n");
	}
	else
	{
		sendMessage(clntSock, "Content-Type: application/octet-stream\n");
	}
}

void sendTimeHeader(int clntSock)
{
	time_t t;
	struct tm *tmp;
	char buffer[255];
	char * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	char * weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	
	time(&t);
	tmp = gmtime(&t);
	
	sprintf(buffer, "Date: %s, %d %s %d %d:%d:%d GMT\n", weekdays[tmp->tm_wday], tmp->tm_mday, months[tmp->tm_mon], (tmp->tm_year + 1900), tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
	sendMessage(clntSock, buffer);
}

void readHTTPheader(int clntSock, HTTP_HEADER * header)
{
	char buffer[MAX_LEN + 1];
	char * token;
	char * delim = " ";
	char * delimKey = " :";
	
	// zero values
	header->method = NULL;
	header->filename = NULL;
	header->protocol = NULL;
	header->host = NULL;
	header->cookies = NULL;
	header->messageBody = NULL;
	header->contentLength = 0;
	
	// read first line of the header
	recieveLine(clntSock, buffer, MAX_LEN);
	
	// get method
	token = strtok(buffer, delim);
	header->method = (char *) malloc(sizeof(char) * (strlen(token) + 1));
	strcpy(header->method, token);
	
	// get requested filename
	if(token = strtok(NULL, delim))
	{
		header->filename = (char *) malloc(sizeof(char) * (strlen(WEBFILES) + strlen(token) + 1));
		strcpy(header->filename, WEBFILES);
		strcat(header->filename, token);
	}
	
	// get protocol
	if(token = strtok(NULL, delim))
	{
		header->protocol = (char *) malloc(sizeof(char) * (strlen(token) + 1));
		strcpy(header->protocol, token);
	}
	
	// print the first line
	printf("Method: %s\n", header->method);
	printf("File name: %s\n", header->filename);
	printf("Protocol: %s\n", header->protocol);
	
	// read all the values in the header
	recieveLine(clntSock, buffer, MAX_LEN);
	while(*buffer)
	{
		strtok(buffer, delimKey);
		token = strtok(NULL, delim);
		// boy this is going to be long
		if (strcmp(buffer, "Host") == 0)
		{
			header->host = (char *) malloc(sizeof(char) * (strlen(token) + 1));
			strcpy(header->host, token);
			printf("Host: %s\n", header->host);
		}
		else if (strcmp(buffer, "Content-Length") == 0)
		{
			header->contentLength = atoi(token);
			printf("Content Length: %d\n", header->contentLength);
		}
		else if (strcmp(buffer, "Cookie") == 0)
		{
			header->cookies = (char *) malloc(sizeof(char) * (strlen(token) + 1));
			strcpy(header->cookies, token);
			printf("Cookies: %s\n", header->cookies);
		}
		
		recieveLine(clntSock, buffer, MAX_LEN);
	}
	
	// read body if it exists
	// should work as long as message is not insanely long
	if(header->contentLength > 0)
	{
		header->messageBody = (char *) malloc(sizeof(char) * (header->contentLength + 1));
		recv(clntSock, header->messageBody, header->contentLength, 0);
		header->messageBody[header->contentLength] = '\0'; // I don't think null character is added, whoops
		printf("Message: %s\n", header->messageBody);
	}
	
	printf("\n");
}

void handleHTTPrequest(int clntSock, HTTP_HEADER * header)
{
	char * method = header->method; // for simplicity sake
	
	sendMessage(clntSock, "HTTP/1.1 400 Bad Request\n");
	sendTimeHeader(clntSock);
}

void handleHTTPClient(int clntSock)
{
	HTTP_HEADER header;
	
	readHTTPheader(clntSock, &header);
	
	handleHTTPrequest(clntSock, &header);
	
	freeHeader(header);
	close(clntSock);
}
