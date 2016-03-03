#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */

#include <sys/stat.h> // for file stat
//#include "sll.h"
#define MAX_WE_SIZE 1024

//  USAGE: sender < portnumber > CWnd PL PC

void error(char *msg) 
{
	perror(msg);
	exit(1);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	char buffer[MAX_WE_SIZE];

	if (argc < 2) {
		 fprintf(stderr,"ERROR, no port provided\n");
		 exit(1);
	}

	//reset memory
	memset((char *) &serv_addr, 0, sizeof(serv_addr));	

	//fill in address info
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	//create UDP socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);	
	if (sockfd < 0) 
		error("ERROR opening socket");

	bzero((char*) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family=AF_INET;
	serv_addr.sin_port=htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
	    sizeof(serv_addr)) < 0) 
	    error("ERROR on binding");

	bzero((char*) &cli_addr, sizeof(cli_addr));
 	clilen = sizeof(cli_addr);		

 	// wait for connection
 	printf("Waiting for receiver\n\n");
 	recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, &clilen);
 	// if we get here, receiver talked to us
 	printf("Receiver Said: %s\n\n", buffer);
 	bzero((char*) buffer, sizeof(char) * 256);

 	char* heyMsg = "What's up receiver, I'm here\n";
 	sendto(sockfd, heyMsg, strlen(heyMsg) * sizeof(char), 
 			0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));

 	recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, &clilen);
 	printf("Receiver wants the file: %s\n\n", buffer);

 	int namelen = strlen(buffer);
 	char filename[namelen + 1];
 	filename[namelen] = '\0';
 	strncpy(filename, &buffer, namelen);

 	bzero((char*) buffer, sizeof(char) * 256);

 	heyMsg = "File is on its way\n";
 	sendto(sockfd, heyMsg, strlen(heyMsg) * sizeof(char), 
 		0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));


 	/*
	
		Breaking down file down to packets
	
 	*/

	/*	GETTING FILE CONTENT */
	FILE* f =  fopen(filename, "r");
	if (f == NULL) {
		error("ERROR opening requested file");
	}

	// number of bytes in the file
	struct stat st;
	stat(filename, &st);
	int fileBytes = st.st_size;

	char* fileContent = (char*) calloc(fileBytes, sizeof(char));
	if (fileContent == NULL) {
		error("ERROR allocating space for file");
	}

	fread(fileContent, sizeof(char), fileBytes, f);
	fclose(f);

	/* 	BREAK DOWN FILE */

	int packet_size; // TODO - 1024 - size of other components in window element?
	int num_packets = fileBytes / packet_size;
	int remainderBytes = fileBytes % packet_size;
	if (remainderBytes)
		num_packets++;

	// TODO: figure out how we want to structure packets



 	close(sockfd);

}
