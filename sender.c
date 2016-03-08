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
	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	char buffer[PACKET_SIZE];

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

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	    error("ERROR on binding");

	bzero((char*) &cli_addr, sizeof(cli_addr));
 	clilen = sizeof(cli_addr);		

 	// wait for connection
 	printf("Waiting for receiver\n\n");

 	while (1) {
 		// once we receive file request from receiver, we go in. Otherwise, loop to keep listening
	 	if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, &clilen) != -1) {

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
				file_packets[i].seq_num = i;
				file_packets[i].type = SENDPACKET;
				// TODO: how should seq_num be numbered - packet number?
			}

			printf("Created array of packets with %i packets\n\n", num_packets);


			/*
		
				START SENDING PACKETS

			*/

			// window_size --> atoi(argv[2]) / PACKET_SIZE
			unsigned int window_size = 6; // or atoi(argv[2])
			unsigned int curr_window_elem = 0;

			//int latest_ACK_received = -1;
			int latest_packet = -1;
			int latest_ACKd_packet = -1;
			int expected_ACK = 0;

			unsigned int time_to_wait = 5;


			// keep sending and receiving ACK until we get ACK for last packet
			//while (latest_ACKd_packet != num_packets - 1) {

				/*
					Send packets within the window size					
				*/

				window w = generateWindow(window_size, num_packets);

				int l = curr_window_elem;
				while (addWindowElement(&w, (file_packets + l))) {
					l++;
				}

				int packet_to_send = curr_window_elem;
				window_element* curr_we = NULL;
				while ((curr_we = getElementFromWindow(&w))) {
					printf("Latest Packet Sent: %i\nCurrent Window Element: %i\n\n", latest_packet, packet_to_send);

					if (packet_to_send > latest_ACKd_packet) {
						sendto(sockfd, (char *) (file_packets + packet_to_send), sizeof(char) * PACKET_SIZE, 
							0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));

						curr_we->status = WE_SENT;
						curr_we->timer = time(NULL) + time_to_wait;

						printf("Just sent packet %i out of %i\n", packet_to_send, num_packets);
						latest_packet = packet_to_send;
					}

					packet_to_send++;
				}

				printf("Out of the first window\n\n");


				int last_window_packet = -1;
				if (curr_window_elem + window_size > num_packets)
					last_window_packet = num_packets-1;
				else
					last_window_packet = window_size + curr_window_elem - 1;	
					// here, packet_to_send will be one greater than last_window_packet

				/*
					Loop to receive ACKs within the window
				*/
				while (1) {

					// TODO: break out if timeout'd

					//printf("Check if timeout or new message\n");
					
					if (w.head != NULL && difftime(w.head->timer, time(NULL)) <= 0) {
						// the first window element is timeout'd
						int curr_s = w.head->packet->seq_num;
						w.head->timer = time(NULL) + time_to_wait;
						printf("First window element (%i) had timed out\n", curr_s);

						if (!resendWindowElement(&w, curr_s)) {
							printf("Failed to resend packet (%i)\n", curr_s);
						} 

						sendto(sockfd, (char *) (file_packets + curr_s), sizeof(char) * PACKET_SIZE, 
										0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));	

						printf("Retransmitting packet number %i\n", curr_s);			

					}
					


					// Again, loop to listen for ACK msg
					if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, &clilen) != -1) {

						// TODO: handle packet corruption & loss

						packet* ACK_msg = (packet *) buffer;

						if (ACK_msg == NULL) {
							error("ERROR Nothing in ACK msg buffer");
						}

						int latest_ACK_received = -1;
						if (ACK_msg->type == ACKPACKET)
							latest_ACK_received = ACK_msg->seq_num;


						
						if (ackWindowElement(&w, latest_ACK_received)) {

							if (latest_ACK_received == num_packets - 1) {

								printf("ACK for the last packet received\n");
								break;
							}

							printf("ACK for the packet %i received\n", latest_ACK_received);

							// if the first window element is ACK'd, we can slide window

							while (w.head->status == WE_ACK) {
								cleanWindow(&w);

								if (addWindowElement(&w, (file_packets + packet_to_send))) {
									curr_window_elem++;
									printf("Sliding window, new 1st window index is: %i last: %i\n", curr_window_elem, curr_window_elem + window_size-1);
								}

								if (curr_window_elem <= packet_to_send && packet_to_send < curr_window_elem + window_size) {
								//if (packet_to_send < num_packets) {
									sendto(sockfd, (char *) (file_packets + packet_to_send), sizeof(char) * PACKET_SIZE, 
											0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));
									printf("Just sent packet %i out of %i\n", packet_to_send, num_packets);

									packet_to_send++;
								}		
								//printf("Check if there are more windows %i to clean\n", curr_window_elem);
							}

							/*
							cleanWindow(&w);


							if (addWindowElement(&w, (file_packets + packet_to_send))) {

								curr_window_elem++;
								printf("Sliding window, new 1st window index is: %i last: %i\n", curr_window_elem, curr_window_elem + window_size-1);
							}

							if (curr_window_elem <= packet_to_send && packet_to_send < curr_window_elem + window_size) {
							//if (packet_to_send < num_packets) {
								sendto(sockfd, (char *) (file_packets + packet_to_send), sizeof(char) * PACKET_SIZE, 
										0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));
								printf("Just sent packet %i out of %i\n", packet_to_send, num_packets);

								packet_to_send++;
							}
							*/

						}


					} // End of if recv ACK
					//printf("Next received message ");

				} // End of ACK while loop
			//	break;
			//} // End of packet sending while loop
				printf("Done with file transfer\n\n");
				bzero((char*) buffer, sizeof(char) * PACKET_SIZE);
		} // end of if(recvfrom)
	}

 	close(sockfd);
 	return 0;

}
