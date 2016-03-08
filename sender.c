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
				// TODO: how should seq_num be numbered - packet number?
			}

			printf("Created array of packets with %i packets\n\n", num_packets);


			/*
		
				START SENDING PACKETS

			*/

			// window_size --> atoi(argv[2]) / PACKET_SIZE
			unsigned int window_size = 5; // or atoi(argv[2])
			unsigned int curr_window_elem = 0;

			//int latest_ACK_received = -1;
			int latest_packet = -1;
			int latest_ACKd_packet = -1;
			int expected_ACK = 0;

			time_t timeout;


			// keep sending and receiving ACK until we get ACK for last packet
			while (latest_ACKd_packet != num_packets - 1) {

				/*
					Send packets within the window size					
				*/

				window w = generateWindow(window_size);

				int l = curr_window_elem;
				while (addWindowElement(&w, (file_packets + l))) {
					l++;
				}

				l = curr_window_elem;
				window_element* curr_we = NULL;
				while ((curr_we = getElementFromWindow(&w))) {
					printf("Latest Packet Sent: %i\nCurrent Window Element: %i\n\n", latest_packet, l);

					if (l > latest_ACKd_packet) {
						sendto(sockfd, (char *) (file_packets + l), sizeof(char) * PACKET_SIZE, 
							0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));

						curr_we->status = WE_SENT;
						curr_we->timer = time(NULL);

						printf("Just sent packet %i out of %i\n", l, num_packets);
						latest_packet = l;
					}

					l++;
				}

				/*
				int j;
				for (j = curr_window_elem; j < curr_window_elem + window_size && j < num_packets; j++) {
					printf("Latest Packet Sent: %i\nCurrent Window Element: %i\n\n", latest_packet, j);

					if (j > latest_ACKd_packet) {

						sendto(sockfd, (char *) (file_packets + j), sizeof(char) * PACKET_SIZE, 
							0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));



						printf("Just sent packet %i out of %i\n", j, num_packets);

						latest_packet = j;

						// TODO: set timer

					}

				}
				*/


				int last_window_packet = -1;
				if (curr_window_elem + window_size > num_packets)
					last_window_packet = num_packets-1;
				else
					last_window_packet = window_size + curr_window_elem - 1;	


				/*
					Loop to receive ACKs within the window
				*/
				while (1) {

					// TODO: break out if timeout'd

					// Again, loop to listen for ACK msg
					if (recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &cli_addr, &clilen) != -1) {

						// TODO: handle packet corruption & loss

						packet* ACK_msg = (packet *) buffer;

						if (ACK_msg == NULL) {
							error("ERROR Nothing in ACK msg buffer");
						}

						int latest_ACK_received = ACK_msg->seq_num;

						//if (latest_ACK_received > file_packets[curr_window_elem].seq_num) {
						if (latest_ACK_received == expected_ACK) {

							latest_ACKd_packet = latest_ACK_received;
							expected_ACK++;

							curr_window_elem++;

							if (latest_ACK_received == num_packets - 1) {
								printf("ACK for the last packet received\n");
								printf("Last packet had %i Bytes\n", remainderBytes);
								break;
							}

							latest_packet++;

							/*
							  curr window elem = 0
							_____________________
							| 0 | 1 | 2 | 3 | 4 |
							---------------------

							--> update: curr window elem = 1, send 5
								_____________________
							| 0 | 1 | 2 | 3 | 4 | 5 |
								---------------------
							*/

							printf("ACK for the packet %i received, sending %i\n", expected_ACK - 1, expected_ACK);

							sendto(sockfd, (char *) (file_packets + latest_packet), sizeof(char) * PACKET_SIZE, 
									0, (struct sockaddr*) &cli_addr, sizeof(cli_addr));						
							//TODO: timeout
						}
					}
				} // End of ACK while loop
			} // End of packet sending while loop
		}
	}

 	close(sockfd);
 	return 0;

}
