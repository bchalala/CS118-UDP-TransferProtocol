/* Implements sll.h header. For server side of UDP FTP 
 * Brett Chalabian and Jason Woo */

#include "sll.h"
#include <stdio.h>
#include <stdlib.h>

window generateWindow(int window_size, int num_packets) {
    window w;
    w.length = 0;
    w.max_size = window_size;
    w.head = NULL;
    w.tail = NULL;
    w.total_packets = num_packets;
    w.current_packets = 0;
    return w;
}

bool setWindowElementStatus(window* w, int sequenceNum, int status) 
{
    window_element* cur = w->head;
    while (cur != NULL) {
        if (cur->packet->seq_num == sequenceNum)
        {
            cur->status = status;
            return true;

        }
        cur = cur->next;
    }
    // Element not found
    return false;
}


bool ackWindowElement(window* w, int sequenceNum) 
{
    return setWindowElementStatus(w, sequenceNum, WE_ACK);
}

void cleanWindow(window* w) 
{
    // If the head of the window can be recycled, it recycles the head and sets the window
    while (w->head != NULL && w->head->status == WE_ACK) {
        w->length--;
        window_element* temp = w->head;

        if (w->head != w->tail) 
            w->head = w->head->next;
        else {
            w->head = NULL;
            w->tail = NULL;
        }
        free(temp);
    }
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    window_element* cur = w->head;

    while (cur != NULL) {
        if (cur->status != WE_ACK && cur->status != WE_NOT_SENT) {
            if (tv.tv_sec > cur->tv.tv_sec || (tv.tv_sec == cur->tv.tv_sec && tv.tv_usec > cur->tv.tv_usec)) {
                cur->status = WE_RESEND;
                cur->packet->type = RETRANSMITPACKET;
                printf("Packet number %d has timed out.\n", cur->packet->seq_num);
            }
        }
        cur = cur->next;
    }

}

bool addWindowElement(window* w, packet* packet)
{
    if (w->length == w->max_size)
        return false;

    if (w->current_packets >= w->total_packets)
        return false;

    w->length++;
    w->current_packets++;
    window_element* nelement = malloc(sizeof(window_element));
    nelement->status = WE_NOT_SENT;
    nelement->packet = packet;
    nelement->next = NULL;
    
    if (w->head == NULL && w->tail == NULL) {
        w->head = nelement;
        w->tail = nelement;
    }
    else {
        w->tail->next = nelement;
        w->tail = nelement;
    } 
   
    return true; 
}

window_element* getElementFromWindow(window w)
{
    window_element* cur = w.head;
    while (cur != NULL)
    {
        if (cur->status == WE_NOT_SENT || cur->status == WE_RESEND)
        {
            return cur;
        }

        cur = cur->next;
    }
    return NULL;
}


bool shouldReceive(float pL, float pC) {
    int trypl = rand() % 1000;
    float normalizedtrypl = trypl/1000.0;
    int trypc = rand() % 1000;
    float normalizedtrypc = trypc/1000.0;
    if (normalizedtrypl >= pL) {
        if (normalizedtrypc >= pC)
            return true;
        printf("Received corrupted packet.\n\n");
    }

    return false;
}

















