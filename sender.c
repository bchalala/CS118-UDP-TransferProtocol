#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */

//  USAGE: sender < portnumber > CW nd PL PC

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

	char buffer[256];

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
	serv_addr.sin_port=htons(5018); // argv htons(portno)
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
 	printf("Receiver Responded: %s\n\n", buffer);

 	char* heyMsg = "What's up receiver, I'm here\n";
 	sendto(sockfd, heyMsg, strlen(heyMsg) * sizeof(char), 
 			0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));

 	close(sockfd);

}
