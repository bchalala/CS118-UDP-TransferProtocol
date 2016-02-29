#ifndef SLL
#define SLL

#define WE_NOT_SENT 0
#define WE_RESEND 1
#define WE_SENT 2
#define WE_ACK  3

typedef struct {
    window_element* next;
    byte* buffer;
    int status; 
    int seq_num;
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
bool addWindowElement(window* w, byte* buf, int sequenceNum);

// Get first window element that needs to be sent.
window_element getElementFromWindow(window* w, window_element* we);

#endif
