#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if (q->size < MAX_QUEUE_SIZE){
                q->proc[q->size++] = proc;
        }
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        #ifdef MLQ_SCHED
                int size = q->size;
                if (size == 0) return NULL;
                struct pcb_t*temp = q->proc[0];
                int index = 0;
                for (int i = 1; i < size; i++){
                        if (temp->priority > q->proc[i]->priority){
                                temp = q->proc[i];
                                index = i;
                        }
                }
                for (int i = index; i < size - 1; i++) {
                        q->proc[i] = q->proc[i + 1];
                }
                q->proc[--q->size] = NULL;
                return temp;
        #else
                if (q->size == 0) return NULL;
                struct pcb_t*temp = q->proc[0];
                for (int i = 0; i < q->size - 1; i++){
                        q->proc[i] = q->proc[i+1];
                }
                q->proc[--q->size] = NULL;
                return temp;
        #endif
}
struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
        for(int i = 0; i < q->size; i++) {
            if(q->proc[i] == proc) {
                // Tìm thấy phần tử cần xóa
                for(int j = i; j < q->size - 1; j++) {
                    q->proc[j] = q->proc[j + 1];
                }
                q->size--;
                return proc;
            }
        }
        return NULL;
}