/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "os-mm.h"
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

//typedef char BYTE;

int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   BYTE value;
   
   /* Find caller PCB by PID in kernel queues instead of allocating a dummy */
   struct pcb_t *caller = NULL;

   if (krnl == NULL) {
       printf("__sys_memmap: NULL krnl\n");
       return -1;
   }

   /* Search running list */
   if (krnl->running_list) {
       for (int i = 0; i < krnl->running_list->size; i++) {
           struct pcb_t *p = krnl->running_list->proc[i];
           if (p && p->pid == pid) { caller = p; break; }
       }
   }

   /* Search ready queue if not found */
   if (caller == NULL && krnl->ready_queue) {
       for (int i = 0; i < krnl->ready_queue->size; i++) {
           struct pcb_t *p = krnl->ready_queue->proc[i];
           if (p && p->pid == pid) { caller = p; break; }
       }
   }

#ifdef MLQ_SCHED
   /* Search MLQ ready queues if still not found */
   if (caller == NULL && krnl->mlq_ready_queue) {
       for (int pr = 0; pr < MAX_PRIO && caller == NULL; pr++) {
           struct queue_t *mq = &krnl->mlq_ready_queue[pr];
           for (int i = 0; i < mq->size; i++) {
               struct pcb_t *p = mq->proc[i];
               if (p && p->pid == pid) { caller = p; break; }
           }
       }
   }
#endif

   if (caller == NULL) {
       /* PID not found in kernel queues; return error */
       printf("__sys_memmap: PID %u not found\n", pid);
       return -1;
   }

   /*
    * @bksysnet: Please note in the dual spacing design
    *            syscall implementations are in kernel space.
    */

   /* TODO: Traverse proclist to terminate the proc
    *       stcmp to check the process match proc_name
    */
//	struct queue_t *running_list = krnl->running_list;

    /* TODO Maching and marking the process */
    /* user process are not allowed to access directly pcb in kernel space of syscall */
    //....
	
   switch (memop) {
   case SYSMEM_MAP_OP:
            /* Reserved process case*/
			vmap_pgd_memset(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_INC_OP:
            inc_vma_limit(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_SWP_OP:
            __mm_swap_page(caller, regs->a2, regs->a3);
            break;
   case SYSMEM_IO_READ:
            MEMPHY_read(caller->krnl->mram, regs->a2, &value);
            regs->a3 = value;
            break;
   case SYSMEM_IO_WRITE:
            MEMPHY_write(caller->krnl->mram, regs->a2, regs->a3);
            break;
   default:
            printf("Memop code: %d\n", memop);
            break;
   }
   
   return 0;
}


