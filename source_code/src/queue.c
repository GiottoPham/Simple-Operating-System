#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
	/* TODO: put a new process to queue [q] */
	if (q->size == MAX_QUEUE_SIZE) return;
	if (empty(q)) {
		q->proc[0] = proc;
	}
	else {
		int i;
		for (i = q->size; i > 0; --i) {
			if (q->proc[i - 1]->priority >= proc->priority) {
				q->proc[i] = q->proc[i - 1];
			}
			else break;
		}
		q->proc[i] = proc;
	}
	q->size++;
}

struct pcb_t * dequeue(struct queue_t * q) {
	/* TODO: return a pcb whose prioprity is the highest
	 * in the queue [q] and remember to remove it from q
	 * */
	if (empty(q)) return NULL;
	struct pcb_t * rtn_qroc = q->proc[--q->size];
	q->proc[q->size] = NULL;
	return rtn_qroc;
}
