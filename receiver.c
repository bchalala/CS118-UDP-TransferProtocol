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
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address

    portno = argv[2];

	int windowSize;
	int numPackets;
	int fileSize;

	int numPacketsReceived = 0;

	clientsocket = socket(AF_INET,SOCK_DGRAM,0);
	if (clientsocket < 0)
		error("ERROR opening client socket");


 	bzero((char*)&serveraddr, sizeof(serveraddr));
 	serveraddr.sin_family = AF_INET;
 	serveraddr.sin_port = htons(portno);

 	server = gethostbyname(argv[1]);
 	if (server = NULL) {
 		error("ERROR getting host");
 	}

	bcopy((char*) server->h_addr, (char*) &serveraddr.sin_addr.s_addr, server->h_length); 	
	
	printf("Sending request to the client");

	// int sendto(int sockfd, const void *msg, int len, unsigned int flags, const struct sockaddr *to, socklen_t tolen);
	//int recvfrom(int sockfd, void *buf, int len, unsigned int flags, struct sockaddr *from, int *fromlen); 

	char* connMsg = "Let's establish a connection";
	sendto(clientsocket, "Let's establish a connection", sizeof("msg"), 0, (sockaddr*)&serv_addr, sizeof(serv_addr));


}