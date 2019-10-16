#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include "boundedqueue.h"

/**
 * @file boundedqueue.c
 * @brief File di implementazione dell'interfaccia per la coda di dimensione finita
 */

/* ------------------- interfaccia della coda ------------------ */

BQueue_t *initBQueue(size_t n) {
   BQueue_t *q = (BQueue_t*)calloc(sizeof(BQueue_t), 1);
   if (!q) { 
   	perror("malloc"); 
		return NULL;
	}
   
	q->buf = calloc(sizeof(void*), n);
   if (!q->buf) {
		perror("malloc buf");
		goto error;
   }
   
	q->head  = q->tail = 0;
   q->qlen  = 0;
   q->qsize = n;
   
	return q;
 	
	error:
	{
   	if (!q) return NULL; 
		else if (q->buf) free(q->buf);
   	free(q);
   	return NULL;
	}
}

void destroyBQueue(BQueue_t *q, void (*F)(void*)) {
   if (!q) {
		errno = EINVAL;
		return;
   }

   if (F) {
		while (q -> qlen != 0) {
			if (q -> buf[q -> head]) {
         		F(q -> buf[q -> head]);
				q -> qlen--;
			}
         	q -> head = (q -> head + 1) % q -> qsize;  
     	}
   }
   
	if (q -> buf) free(q -> buf);
	free(q);
}

int pushBQueue(BQueue_t *q, void *data) {
   if (!q || !data) {
		errno = EINVAL;
		return -1;
   }
   
	if (q -> qsize == q -> qlen) 
		return -1;

	q->buf[q->tail] = data;
   q->tail = (q->tail + 1) % q->qsize;  
   q->qlen += 1;
   
	return 0;
}

void* popBQueue(BQueue_t *q) {
	if (!q || q -> qlen == 0) {
		errno = EINVAL;
		return NULL;
   }

	void* data = q -> buf[q -> head];
	q -> head = (q -> head + 1) % q -> qsize;
	q -> qlen -= 1;

	return data;
}

size_t getBQueueLen(BQueue_t *q) {
	if (!q) {
		errno = EINVAL;
		return -1;
	}
	
	size_t len = q->qlen;

	return len;
}

BQueue_iterator_t* BQueue_iterator_init(BQueue_t* q) {
   if (!q) {
      errno = EINVAL;
      return NULL;
   }

	BQueue_iterator_t* iterator = (BQueue_iterator_t*) malloc(sizeof(BQueue_iterator_t));
	if (!iterator) 
		return NULL;
	
	iterator -> queue = q;
	iterator -> next = iterator -> queue -> head;

	return iterator;
}

void* BQueue_iterator_next(BQueue_iterator_t* iterator) {
	if (!iterator) {
		errno = EINVAL;
		return NULL;
	}

	void* data = iterator -> queue -> buf[iterator -> next];
	iterator -> next = ((iterator -> next) + 1) % (iterator -> queue -> qsize);

	return data;
}

void BQueue_iterator_destroy(BQueue_iterator_t* iterator) {
	if (iterator) free(iterator);
}

