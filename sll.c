/* Implements sll.h header. For server side of UDP FTP 
 * Brett Chalabian and Jason Woo */

#include "sll.h"
#include <stdio.h>

window generateWindow(int window_size) {
    w.length = 0;
    w.max_size = window_size();
    w.head = NULL;
    w.tail = NULL;
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

bool resendWindowElement(window* w, int sequenceNum) 
{
    return setWindowElementStatus(w, sequenceNum, WE_RESEND);
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
        free(head->buffer);
        if (w->head != w->tail) 
            w->head = w->head->next;
        else {
            w->head = NULL;
            w->tail = NULL;
        }
        free(temp);
    }
}

bool addWindowElement(window* w, packet* packet)
{
    if (w->length == w->max_size)
        return false;

    length++;
    window_element* nelement = malloc(sizeof(window_element));
    nelement->status = WE_NOT_SENT;
    nelement->packet = p;
    
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

window_element getElementFromWindow(window* w)
{
    window_element* cur = w->head;
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

