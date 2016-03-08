#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>

#include "sll.h"

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


	while (1) {
		// keep looping to receive file packets
		if (recvfrom(clientsocket, buffer, sizeof(buffer), 0,(struct sockaddr*) &serv_addr, &len) != -1) {

			packet* content_packet = (packet *) buffer;
			if (content_packet == NULL) {
				printf("File Packet #%i was NULL\n", next_packet);
				exit(1);
			}

			if (file_packets == NULL) {
				// very first packet
				file_size = content_packet->total_size;

				total_num_packets = (file_size / PACKET_CONTENT_SIZE);

				file_packets =  (packet *) calloc(total_num_packets, sizeof(packet));

				if (file_packets == NULL) {
					error("ERROR allocating for receiving file packets");
				}
			}

			packet ACK_packet;

			// TODO: handle packet corruption or loss

			if (content_packet->seq_num == next_packet) {
				// received the packet we supposed to be getting

				printf("Got packet number %i, next packet should be %i\n", next_packet, next_packet+1);
				next_packet++;

				file_packets[received_packets] = *content_packet;
				received_packets++;

				ACK_packet.seq_num = next_packet - 1;
			}
			else {
				// got a different packet than expected
				printf("Should get packet %i, but got %i\n\n", next_packet, content_packet->seq_num);

				ACK_packet.seq_num = content_packet->seq_num;
			}	

			/*
				Received file packet, so send an ACK correspondingly
			*/

			ACK_packet.total_size = file_size;

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


