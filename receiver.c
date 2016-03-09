#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "sll.h"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char* argv[]) {
	// $ receiver < sender hostname >< sender portnumber >< filename > PL PC	
	// argc = 6
	srand(time(NULL));
	int clientsocket; //Socket descriptor
    int portno, n;
    socklen_t len;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    char buffer[PACKET_SIZE];

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

	char* filename = argv[3];
	printf("Requesting the file: %s\n", filename);
	sendto(clientsocket, filename, strlen(filename) * sizeof(char), 
					0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));


	// variables to keep track of packet information
	packet* file_packets = NULL;
	unsigned int next_packet = 0;
	unsigned long file_size;
	unsigned int received_packets = 0;
	unsigned int total_num_packets;
	bool firstPacketReceived = false;
	time_t now = time(NULL);
	float pL = 0.2;
	float pC = 0.2;

	// Keeps attempting to send file request until it gets a response. 
	while (firstPacketReceived == false) {
		// Attempts to send a request 
		if (time(NULL) - now > 1) {
				now = time(NULL);
				printf("Requesting the file: %s\n", filename);
				sendto(clientsocket, filename, strlen(filename) * sizeof(char), 
							0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
		}

		if (recvfrom(clientsocket, buffer, sizeof(buffer), MSG_DONTWAIT,(struct sockaddr*) &serv_addr, &len) != -1 
			&& shouldReceive(pL, pC)) 
		{
			firstPacketReceived = true;
			packet* content_packet = (packet *) buffer;
			packet ACK_packet;

			// Allocates space for file in a file_packet buffer.
			file_size = content_packet->total_size;
			total_num_packets = (file_size / PACKET_CONTENT_SIZE);
			file_packets =  (packet *) calloc(total_num_packets, sizeof(packet));
			if (file_packets == NULL) {
				error("ERROR allocating for receiving file packets");
			}

			// Places the first packet received in the correct position of the file_packets buffer.
			int sequenceNum = content_packet->seq_num;
			char packetType = content_packet->type;
			file_packets[sequenceNum] = *content_packet;
			printf("Got packet number %i. \n", sequenceNum);
			if (packetType == SENDPACKET) {
				printf("Packet type: data packet.\n");
			}
			if (packetType == RETRANSMITPACKET) {
				printf("Packet type: retransmitted data packet.\n");
			}
			if (packetType == FILENOTFOUNDPACKET) {
				printf("Packet type: FILE NOT FOUND. Exiting.\n");
				exit(1);
			}
			next_packet = sequenceNum + 1;
			ACK_packet.type = ACKPACKET;
			ACK_packet.seq_num = sequenceNum;
			ACK_packet.total_size = file_size;

			sendto(clientsocket, (char *) &ACK_packet, PACKET_SIZE, 
				0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
			printf("Sent ACK seqnum %i\n\n", ACK_packet.seq_num);

			if (next_packet > total_num_packets) {
				break;
			}
			
		}
	}


	while (1) {
		// keep looping to receive file packets
		if (recvfrom(clientsocket, buffer, sizeof(buffer), 0,(struct sockaddr*) &serv_addr, &len) != -1 
			&& shouldReceive(pL, pC)) 
		{
			packet* content_packet = (packet *) buffer;
			packet ACK_packet;

			int sequenceNum = content_packet->seq_num;
			char packetType = content_packet->type;
			file_packets[sequenceNum] = *content_packet;
			printf("Got packet number %i.\n", sequenceNum);
			if (packetType == SENDPACKET) {
				printf("Packet type: data packet.\n");
			}
			if (packetType == RETRANSMITPACKET) {
				printf("Packet type: retransmitted data packet.\n");
			}
			next_packet = sequenceNum + 1;
			ACK_packet.type = ACKPACKET;
			ACK_packet.seq_num = sequenceNum;
			ACK_packet.total_size = file_size;

			/*
				Received file packet, so send an ACK correspondingly
			*/

			sendto(clientsocket, (char *) &ACK_packet, PACKET_SIZE, 
				0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
			printf("Sent ACK seqnum %i\n\n", ACK_packet.seq_num);

			if (next_packet > total_num_packets) {
				break;
			}
		}
	} // End of receiving file packets while loop

	printf("Done receiving file packets and sending ACKs back\n");

	/*
		Write the received packets into file
	*/
	char fileContent[file_size + 1];
	memset(fileContent, 0, file_size + 1);	

	int i;
	for (i = 0; i <= total_num_packets; i++) {
		strcat(fileContent, file_packets[i].buffer);
	}

	//fileContent[file_size] = '\0';

	FILE* f = fopen("test.txt", "wb");
	if (f == NULL) {
		error("ERROR with opening file");
	}

	fwrite(fileContent, sizeof(char), file_size, f);

	fclose(f);


	close(clientsocket);

	return 0;
}


