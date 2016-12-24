/*
Name: Andrew Bagwell
Date: 05/30/2016
Assignment: Project2
File: ftserver.c
Language: C
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h> //http://pubs.opengroup.org/onlinepubs/009695399/functions/inet_addr.html
#include <dirent.h>

//constants 

#define BACKLOG  20
#define MESS_LGTH 500

char hostName[1024]; //buffer to hold hostname
char service[20]; //needed for getnameinfo, may be optional

//function prototypes

int buildServer(char*);
int doFTP(int);
int controlConnection(int);
int setUpCommands(int, char*, char*, char*);
int dataConnection(char*, char*);
void listDir(int);
void getFile(int, char*);
int sendAll(int, char*, int);
void signalIntHandle(int);

int main(int argc, char *argv[]) {

    //command line constraints

    if (argc != 2) {
            fprintf(stderr,"usage: ftserver [port number]\n");
            exit(1);
        }

    //handle the possibility of signal interruption..

    signal(SIGINT, signalIntHandle);

    //build server

    int serverSoc = buildServer(argv[1]);

    //do the FTP 

    int status;

    do {
    	status = doFTP(serverSoc);
    } 
    while (status = 1);

    close(serverSoc);
    return 0;

}

//----------------------BUILD SERVER FUNCTION----------------
//This is the function builds and returns the server socket that is used in the doFTP functions loop 
//-----------------------------------------------------------

int buildServer(char* portPassed) {

//create structs for storing information about server

    struct addrinfo hints;
    struct addrinfo* servInfo;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //check for errors in getaddrinfo function

    int errCheck;
    if ((errCheck = getaddrinfo(NULL, portPassed, &hints, &servInfo)) != 0) {

        fprintf(stderr, "Error -getaddinfo failure: %s\n", gai_strerror(errCheck));
        exit(1);
    }

//This is where the server socket for the control connection is created...

    int serverSocket = socket(servInfo->ai_family, servInfo->ai_socktype, servInfo->ai_protocol);

    //check for errors in socket function

    if (serverSocket == -1) {
        exit(1);
    }

    //bind the socket with bind function

    errCheck = bind(serverSocket, servInfo->ai_addr, servInfo->ai_addrlen);
    if (errCheck == -1) {
        fprintf(stderr, "Error bind() failure");
        exit(1);
    }

    //begin listening for connections

    errCheck = listen(serverSocket, BACKLOG);

    //check for errors in the listen() function

    if (errCheck == -1) {
        fprintf(stderr, "Error listen() failure");
        exit(1);
    }

    printf("Server open: %s\n", portPassed);

    //server begins listening and continues to do so until status changes

    freeaddrinfo(servInfo);

    return serverSocket;
}

//----------------------DOFTP FUNCTION----------------
//This is the main driver function of the program. it takes a server socket file descriptor as a parameter
//It then make functions calls that comprise the FTP program and returns 1 upon success in order to help run the program
//loop
//-----------------------------------------------------------

int doFTP(int serverSocket) {
    
	char userCommand[5];
	char userFile[20];
	char userPort[10];

	printf("Waiting for connection...\n");

    //create control connection

	int controlSocket = controlConnection(serverSocket);

    // parse commands

   int flag = setUpCommands(controlSocket, userCommand, userFile, userPort);

    if (flag == 0) {

    	char invalidCmd[] = "Invalid Command";
		send(controlSocket, invalidCmd, sizeof(invalidCmd), 0);
    }
    
	else if (flag == 1) {

		printf("List directory requested on port %s\n", userPort);
		//create data socket
		int dataSocket = dataConnection(userPort, hostName);
		//send the directory
		printf("Sending directory contents to %s:%s\n", hostName, userPort);
		listDir(dataSocket);
		close(dataSocket);

	}

	else if (flag == 2) {

		printf("File %s requested on port %s\n", userFile, userPort);

		//search for file - if not found, send message through control connection

		FILE* fp = fopen(userFile, "r");

		if (fp == NULL) {

			char noFileErr[] = "File does not exist";
			printf("SENDING file not found error\n");
			send(controlSocket, noFileErr, sizeof(noFileErr), 0);
		}

		else {  //file found
			fclose(fp);
			char fileFound[] = "OK";
			//send confirmation
			send(controlSocket, fileFound, sizeof(fileFound), 0);
			printf("Sending %s to %s:%s\n", userFile, hostName, userPort);
			//I was having issues with my data connection not being open in time so it needed to sleep
			sleep(2);
			int dataSocket = dataConnection(userPort, hostName);
			//send the file
			getFile(dataSocket, userFile);
			close(dataSocket);
		}

		close(controlSocket);
    	
}

    return 1;

}


//------------------control Connection function--------------------------
//This function establishes a TCP socket that is used as the control connection between server and clientt
//for the FTP program. It takes a socket file descriptor and will accept a connection from the outside and return
//the accepted connection's socket file descriptor
//---------------------------------------------------------


int controlConnection (int socketPassed){

	struct sockaddr_storage clientAddress;
    socklen_t addressSize = sizeof(clientAddress);

    int controlFD = accept(socketPassed, (struct sockaddr*)&clientAddress, &addressSize);

    if (controlFD == -1) {
          printf("Connection Failed..\n");
          exit(1);
      }

    getnameinfo((struct sockaddr*)&clientAddress, addressSize, hostName, sizeof(hostName), service, sizeof(service), 0);
    printf("Connection from %s\n", hostName);

    return controlFD;
}

//------------------setUpCommands function--------------------------
//This function parses the commands from the client into seperate commands and then fill the respective
//function variables for further use by the program. Depending on the command received, it also sets a 
//cmd flag as well. 
//---------------------------------------------------------


int setUpCommands(int socketPassed, char* clientCommand, char* clientFile, char* clientPort) {

	int cmdFlag = 0; 
	char cmdsRcvd[MESS_LGTH];
	memset(cmdsRcvd, 0, MESS_LGTH);

	char* parsedArg; 
	int argCount = 0;

	//get the commands from client

    recv(socketPassed, cmdsRcvd, MESS_LGTH, 0);

    parsedArg = strtok(cmdsRcvd, " \n\r\t\0");
    strcpy(clientCommand, parsedArg);

    if (strcmp(clientCommand, "-l") == 0) {

    	parsedArg = strtok(NULL, " \n\r\t\0");
    	strcpy(clientPort, parsedArg);
    	cmdFlag = 1; 
    }

    else if (strcmp(clientCommand, "-g") == 0) {

    	parsedArg = strtok(NULL, " \n\r\t\0");
    	strcpy(clientFile, parsedArg);
    	parsedArg = strtok(NULL, " \n\r\t\0");
    	strcpy(clientPort, parsedArg);
    	cmdFlag = 2; 
    }

	return cmdFlag;

}

//------------------dataConnection function--------------------------
//This function creates a TCP socket and connects to the client's connection in order to server
//as the data connection for the FTP program. It takes a portNum and hostname - represented as char arrays
//as parameters and returns the data connection's socket descriptor
//---------------------------------------------------------

int dataConnection (char* portPassed, char* hostPassed) {

	//create structs to get info about server...

    struct addrinfo hints;
    struct addrinfo* servInfo;
    int dataSocket;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    //check for errors

    int errCheck;
    if ((errCheck = getaddrinfo(hostPassed, portPassed, &hints, &servInfo)) != 0) {

        fprintf(stderr, "Error -getaddinfo failure: %s\n", gai_strerror(errCheck));
        exit(1);
    }

    //create socket with call to socket()

    dataSocket = socket(servInfo->ai_family, servInfo->ai_socktype, servInfo->ai_protocol);

    //error check

     if (dataSocket == -1) {
        exit(1);
    }

    //connect to the other socket on the client

    int status;
    status = connect(dataSocket, servInfo->ai_addr, servInfo->ai_addrlen);

    if (status == -1) {
        exit(1);
    }

    printf("Connected...to %s\n", hostName); //testing 

    freeaddrinfo(servInfo); //no longer needed..

    return dataSocket;

}

//------------------listDir function--------------------------
//This function is passed a socket descriptor and reads through the directory, sending the contents 
//over the socket to the other side, filename by filename. It takes a socket descriptor as a parameter
//and returns nothing
//---------------------------------------------------------

void listDir(int socketPassed) {

	//https://www.informit.com/guides/content.aspx?g=cplusplus&seqNum=245
	//http://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c

	char buffer[1];
	memset(buffer, 0, sizeof(buffer));
	int bc;
	int i; 

	DIR* dir;
	struct dirent* dirContent;

	if ((dir = opendir(".")) != NULL) {

		while ((dirContent = readdir(dir)) != NULL) {

			if (strcmp(dirContent->d_name, ".") == 0) {
				continue;
			}

			else if (strcmp(dirContent->d_name, "..") == 0) {
				continue;
			}

			else {

				sendAll(socketPassed, dirContent->d_name, strlen(dirContent->d_name));
				send(socketPassed, "\n", 1, 0); //send whitespace for formatting
			}		
		}
	}

	else {
		perror("Error: ");
	}
}

//------------------getFile function--------------------------
//This function is passed a socket descriptor and a file name, it opens the file and sends the contents 
//over the socket to the other side, byte by byte. 
//---------------------------------------------------------

void getFile (int socketPassed, char* fileName) {

	//http://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c

	char buffer[1];
	memset(buffer, 0, sizeof(buffer));
	int i; 

	FILE* fp = fopen(fileName, "rb");
	fseek(fp, 0L, SEEK_END);
	int size = ftell(fp);
	rewind(fp);

	for (i = 0; i < size; i++) {

		fread(buffer, sizeof(char), sizeof(buffer), fp);
		sendAll(socketPassed, buffer, sizeof(buffer)); //technically, sendAll is not really needed here, because send() would be fine with such a small buffer but I included for good measure
	}

	fclose(fp);
}

//------------------sendAll function--------------------------
//http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#sendall
//This function takes a socket descriptor, a buffer, and the buffer size. It continually
//sends the contents of the buffer until there is confirmation that the number of bytes 
//sent matched the original buffer size. 
//---------------------------------------------------------

int sendAll (int socketPassed, char* buffer, int buffSize) {

	int bytesSent = 0;
	int bytesRemain = buffSize;
	int byteCount; 

	while (bytesSent < buffSize) {

		byteCount = send(socketPassed, buffer + bytesSent, bytesRemain, 0);

		if (byteCount == -1)
			return 1; 
		bytesSent += byteCount; 
		bytesRemain = bytesRemain - byteCount; 
	}

	return 0; 
}


void signalIntHandle(int sigReceived) {

    printf("\nConnection lost....signal interrupted.\n");
    exit(0);
}



