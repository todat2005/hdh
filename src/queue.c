#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        // Kiểm tra nếu hàng đợi rỗng
        if(empty(q)) {
            return NULL;
        }
        // Tìm phần tử có độ ưu tiên cao nhất
        int highest_priority_index = 0;
        for(int i = 1; i < q->size; i++) {
            if(q->proc[i]->priority < q->proc[highest_priority_index]->priority) {
                highest_priority_index = i;
            }
        }
        // Xóa phần tử có độ ưu tiên cao nhất khỏi hàng đợi
        struct pcb_t *highest_priority_proc = q->proc[highest_priority_index];
        for(int i = highest_priority_index; i < q->size - 1; i++) {
            q->proc[i] = q->proc[i + 1];
        }
        q->size--;
        return highest_priority_proc;
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