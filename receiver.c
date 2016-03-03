#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char* argv[]) {
	// $ receiver < sender hostname >< sender portnumber >< filename > PL PC	
	// argc = 6

	int clientsocket; //Socket descriptor
    int portno, n;
    socklen_t len;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    char buffer[256];


    // pre-PLPC testing - too lazy to write all the parameters for testing
	if (argc < 3) {
		 fprintf(stderr,"ERROR, no host or port provided\n");
		 exit(1);
	}

	/* 
	if (argc < 6) {
		fprintf(stderr, "USAGE: receiver <sender hostname> <sender portnumber> <filename> <PL> <PC>");
		exit(1);
	}
	*/

    portno = atoi(argv[2]);

	int windowSize;
	int numPackets;
	int fileSize;

	int numPacketsReceived = 0;

	clientsocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (clientsocket < 0)
		error("ERROR opening client socket");


 	bzero((char*) &serv_addr, sizeof(serv_addr));
 	serv_addr.sin_family = AF_INET;
 	serv_addr.sin_port = htons(portno);

 	server = gethostbyname(argv[1]);
 	if (server == NULL) {
 		error("ERROR getting host");
 	}

	bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length); 	
	
	printf("Sending request to the sender\n\n");

	// int sendto(int sockfd, const void *msg, int len, unsigned int flags, const struct sockaddr *to, socklen_t tolen);
	// int recvfrom(int sockfd, void *buf, int len, unsigned int flags, struct sockaddr *from, int *fromlen); 

	char* connMsg = "Let's establish a connection";
	sendto(clientsocket, connMsg, strlen(connMsg) * sizeof(char), 
					0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	printf("Waiting for sender response\n\n");
	recvfrom(clientsocket, buffer, sizeof(buffer), 
					0,(struct sockaddr*) &serv_addr, &len);

	printf("Sender Responded: %s\n", buffer);
	bzero((char*) buffer, sizeof(char) * 256);

	char* filename = argv[3];
	sendto(clientsocket, filename, strlen(filename) * sizeof(char), 
					0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));	
	printf("Requesting the file: %s\n", filename);

	recvfrom(clientsocket, buffer, sizeof(buffer), 
					0,(struct sockaddr*) &serv_addr, &len);
	printf("Is the file coming?: %s\n", buffer);
	bzero((char*) buffer, sizeof(char) * 256);


	close(clientsocket);

}


