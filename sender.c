#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */

#include <sys/stat.h> // for file stat
#include <time.h>


#include "sll.h"

//  USAGE: sender < portnumber > CWnd PL PC

void error(char *msg) 
{
	perror(msg);
	exit(1);
}

int main(int argc, char *argv[])
{
	srand(time(NULL));
	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	char buffer[PACKET_SIZE];

	if (argc < 2) {
		 fprintf(stderr,"ERROR, no port provided\n");
		 exit(1);
	}

	float pL,pC;
	if (argv[3] == NULL)
		pL = 0;
	else
		pL = atof(argv[3]);

	if (argv[4] == NULL)
		pC = 0;
	else
		pC = atof(argv[4]);

	printf("PL: %f, PC: %f\n", pL, pC);

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

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	    error("ERROR on binding");

	bzero((char*) &cli_addr, sizeof(cli_addr));
 	clilen = sizeof(cli_addr);		

 	// wait for connection
 	printf("Waiting for receiver\n\n");

 	while (1) {
 		// once we receive file request from receiver, we go in. Otherwise, loop to keep listening
	 	if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, &clilen) != -1) {

			if (shouldReceive(pL, pC) == false) {

				continue;
			}
		 	int namelen = strlen(buffer);
		 	char filename[namelen + 1];
		 	filename[namelen] = '\0';
		 	strncpy(filename, buffer, namelen);

		 	bzero((char*) buffer, sizeof(char) * PACKET_SIZE);

		 	printf("Receiver wants the file: %s\n\n", filename);

		 	/*

				Breaking down file down to packets

		 	*/

			/*	GETTING FILE CONTENT */
			FILE* f =  fopen(filename, "r");
			if (f == NULL) {
				packet notfound;
				notfound.type = FILENOTFOUNDPACKET;
				printf("ERROR opening requested file\n");
				sendto(sockfd, (char *) &notfound, sizeof(char) * PACKET_SIZE, 
										0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));
				continue;
				
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

			int num_packets = fileBytes / PACKET_CONTENT_SIZE;
			int remainderBytes = fileBytes % PACKET_CONTENT_SIZE;
			if (remainderBytes)
				num_packets++;

			printf("Number of packets for the file %s is: %i\n", filename, num_packets);

			/*

				PACKET ARRAY CONSTRUCTION

			*/
			packet file_packets[num_packets];

			int i;
			for (i = 0; i < num_packets; i++) {

				if ((i == num_packets - 1) && remainderBytes) { 
				// the last packet with remainder bytes doesn't take full space
					strncpy(file_packets[i].buffer, fileContent + i * PACKET_CONTENT_SIZE, remainderBytes);
					file_packets[i].buffer[remainderBytes] = '\0';
				}
				else { // normal cases
					strncpy(file_packets[i].buffer, fileContent + i * PACKET_CONTENT_SIZE, PACKET_CONTENT_SIZE);
					file_packets[i].buffer[PACKET_CONTENT_SIZE]	= '\0';
				}

				file_packets[i].total_size = fileBytes;
				file_packets[i].seq_num = i % MAX_SEQ_NUM;
				file_packets[i].type = SENDPACKET;
				// TODO: how should seq_num be numbered - packet number?
			}

			printf("Created array of packets with %i packets\n\n", num_packets);


			/*
		
				START SENDING PACKETS

			*/

			unsigned int window_size;
			if (argv[2] == NULL)
				window_size = 6;
			else
				window_size = atoi(argv[2]) / PACKET_SIZE;
			printf("Window Size if %i packets\n", window_size);
			unsigned int curr_window_elem = 0;

			//int latest_ACK_received = -1;
			int latest_packet = -1;
			int latest_ACKd_packet = -1;

			time_t time_to_wait = 1;


			// keep sending and receiving ACK until we get ACK for last packet
			//while (latest_ACKd_packet != num_packets - 1) {

				/*
					Send packets within the window size					
				*/

				window w = generateWindow(window_size, num_packets);

				while (addWindowElement(&w, (file_packets + curr_window_elem))) {
					curr_window_elem++;
				}

					// here, packet_to_send will be one greater than last_window_packet

				/*
					Loop to receive ACKs within the window
				*/
				while (1) {

					window_element* we = getElementFromWindow(w);
					while (we != NULL) {
						int cs_num = we->packet->seq_num;

						sendto(sockfd, (char *) (file_packets + cs_num), sizeof(char) * PACKET_SIZE, 
										0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));	
						if (we->status == WE_RESEND) {
							printf("Retransmitting packet number %d\n", cs_num);
						}
						if (we->status == WE_NOT_SENT) {
							printf("Transmitting packet number %d\n", cs_num);
						}
						time_t t = time(NULL) + time_to_wait;
						we->timer = t;
						we->status = WE_SENT;
						we = getElementFromWindow(w);
					}

					// Again, loop to listen for ACK msg
					bool didreceive = true;
					while (didreceive) {

						didreceive = false;
						if (recvfrom(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr*) &cli_addr, &clilen) != -1) {
							didreceive = true;
							if (shouldReceive(pL, pC) == false) {

								continue;
							}
							// TODO: handle packet corruption & loss

							packet* ACK_msg = (packet *) buffer;

							if (ACK_msg == NULL) {
								error("ERROR Nothing in ACK msg buffer");
							}

							int latest_ACK_received = -1;
							if (ACK_msg->type == ACKPACKET)
								latest_ACK_received = ACK_msg->seq_num;
							
							if (ackWindowElement(&w, latest_ACK_received)) {

								printf("ACK for the packet %i received\n", latest_ACK_received);

								// if the first window element is ACK'd, we can slide window
								if (num_packets == curr_window_elem + 1 && w.length == 0) {
									printf("ACK for the last packet received\n");
									break;
								}

							}

							

						}
					} // End of if recv ACK

					cleanWindow(&w);
					while (curr_window_elem != num_packets && addWindowElement(&w, (file_packets + curr_window_elem)))
						curr_window_elem++;

					if (num_packets == curr_window_elem && w.length == 0) {
						printf("ACK for the last packet received\n");
						break;
					}

				} // End of ACK while loop
				printf("Done with file transfer\n\n");
				bzero((char*) buffer, sizeof(char) * PACKET_SIZE);
		} // end of if(recvfrom)
	}

 	close(sockfd);
 	return 0;

}
