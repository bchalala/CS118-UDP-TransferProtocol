#ifndef SLL
#define SLL

#include <stdbool.h>
#include <time.h>

#define WE_NOT_SENT 0
#define WE_RESEND 1
#define WE_SENT 2
#define WE_ACK  3

#define MAX_SEQ_NUM

#define PACKET_SIZE 1024
#define PACKET_CONTENT_SIZE (PACKET_SIZE - sizeof(long) - sizeof(int) - 1)
// -1 for \0 at the end

typedef struct _packet {
	unsigned long total_size;
	unsigned int seq_num;
	char buffer[PACKET_CONTENT_SIZE + 1];
	// +1 for \0 at the end
} packet;

typedef struct window_element {
    struct window_element* next;
    packet* packet;
    int status; 
    time_t timer;
} window_element;

typedef struct {
    window_element* head;
    window_element* tail;
    int length;
    int max_size;
} window;

// Generates a new window struct
window generateWindow(int window_size);

// Marks a packet as acknowledged. Should be used when parsing client messages.
bool ackWindowElement(window* w, int sequenceNum);

// Marks packet as needing to be resent. Should be used when parsing cient messages.
bool resendWindowElement(window* w, int sequenceNum);

// Removes elements from the head of the window if they are sent and acknowledged.
void cleanWindow(window* w);

// Add an element to the window. If the window is full, it will return false.
bool addWindowElement(window* w, packet* packet);

// Get first window element that needs to be sent.
window_element* getElementFromWindow(window* w);

/* Other auxiliary functions */
bool shouldReceive(float pL, float pC);


#endif
