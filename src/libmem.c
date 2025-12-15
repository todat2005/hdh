/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
//#ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
// Thêm một region mới vào danh sách region rãnh của VMA
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
// Lấy region theo ID (dùng làm chỉ số trong bảng symbol của biến)
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region (out)
 *
 * Mô tả (Tiếng Việt):
 *  - Hàm này cấp phát một vùng nhớ ảo cho tiến trình `caller` trong VMA
 *    chỉ định bởi `vmaid` và ghi thông tin vùng vào `symrgtbl[rgid]`.
 *  - Thứ tự xử lý:
 *    1) Khóa mutex để bảo toàn thao tác quản lý vùng ảo.
 *    2) Tìm một region rỗng phù hợp trong `cur_vma->vm_freerg_list` bằng
 *       `get_free_vmrg_area`. Nếu tìm được thì ghi vào `symrgtbl` và trả về.
 *    3) Nếu không tìm được, dò lại `vm_freerg_list` thủ công (reclaim).
 *    4) Nếu vẫn không có chỗ, gọi syscall `sys_memmap` (SYSCALL 17 với
 *       `SYSMEM_INC_OP`) để tăng limit của VMA (sbrk-like), rồi cấp vùng
 *       bắt đầu từ `old_sbrk`.
 *  - Trả địa chỉ đầu vùng qua `*alloc_addr` và trả 0 khi thành công.
 *
 * Lưu ý / Hạn chế:
 *  - Hàm giả định syscall tăng limit thành công; nên kiểm tra kết quả syscall
 *    và rollback nếu thất bại (hiện chưa có).
 *  - Nên kiểm tra `rgid` hợp lệ trước khi ghi vào `symrgtbl`.
 *  - Hàm khóa toàn cục `mmvm_lock` trong suốt quá trình; có thể tối ưu
 *    bằng khóa theo-VMA nếu cần song song cao.
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /*Allocate at the toproof */
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int inc_sz=0;
  // cấp phát vùng nhớ ảo trong VMA chỉ định thông qua rgid
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    *alloc_addr = rgnode.rg_start;
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }
    /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  /*Attempt to increate limit to get space */
#ifdef MM64
  inc_sz = (uint32_t)(size/(int)PAGING64_PAGESZ);
  inc_sz = inc_sz + 1;
#else
  inc_sz = PAGING_PAGE_ALIGNSZ(size);
#endif
  int old_sbrk;

  old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * SYSCALL 1 sys_memmap
   */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
#ifdef MM64
  regs.a3 = inc_sz;
#else
  regs.a3 = PAGING_PAGE_ALIGNSZ(size);
#endif  
  syscall(caller->krnl, caller->pid, 17, &regs); /* SYSCALL 17 sys_memmap */

  /*Successful increase limit */
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  *alloc_addr = old_sbrk;

  /* Advance program break so subsequent allocations do not overlap */
  cur_vma->sbrk = old_sbrk + size;
  pthread_mutex_unlock(&mmvm_lock);
  return 0;

}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
// hàm giải phóng một vùng nhớ ảo của tiến trình
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);

  if (rgnode->rg_start >= rgnode->rg_end)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
// hàm thư viện cấp phát vùng nhớ ảo cho tiến trình
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t  addr;

  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }

  /* Mirror legacy semantics: store base address into the register slot */
  if (reg_index < sizeof(proc->regs) / sizeof(proc->regs[0])) {
    proc->regs[reg_index] = addr;
  }
  printf("liballoc:%d, PID: %i\n", __LINE__, proc->pid);
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  pthread_mutex_lock(&mmvm_lock);
  print_pgtbl(proc, 0, -1); // print max TBL
  pthread_mutex_unlock(&mmvm_lock);
#endif
#endif

  /* By default using vmaid = 0 */
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }
  
  /* Clear stale base address in register slot */
  if (reg_index < sizeof(proc->regs) / sizeof(proc->regs[0])) {
    proc->regs[reg_index] = 0;
  }
  printf("libfree:%d, PID: %i\n", __LINE__, proc->pid);
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  pthread_mutex_lock(&mmvm_lock);
  print_pgtbl(proc, 0, -1); // print max TBL
  pthread_mutex_unlock(&mmvm_lock);
#endif
#endif
  return 0;//val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, addr_t pgn, addr_t *fpn, struct pcb_t *caller)
{

  uint32_t pte = pte_get_entry(caller, pgn);

  /* Nếu trang chưa hiện diện trong RAM */
  if (!PAGING_PAGE_PRESENT(pte))
  { 
    addr_t vicpgn, swpfpn;

    /* Tìm trang nạn nhân (victim page) để đẩy ra ngoài swap */
    if (find_victim_page(mm, &vicpgn) == -1)
    {
      return -1;   // Không tìm được victim page
    }

    /* Lấy một frame trống trong vùng swap */
    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1)
    {
      return -1;   // Không còn frame trống trong swap
    }

    /* Bắt đầu cơ chế hoán trang (swap):
       - Copy nội dung frame của victim từ RAM → SWAP
       - Đánh dấu victim là 'bị swap'
       - Tái sử dụng frame RAM của victim cho trang đang cần nạp
    */
    {
      uint32_t vic_pte = pte_get_entry(caller, vicpgn);
      addr_t vicfpn = PAGING_FPN(vic_pte);
      struct memphy_struct *mram = caller->krnl->mram;
      struct memphy_struct *mswp = caller->krnl->active_mswp;

      /* Kiểm tra hợp lệ: victim phải đang có mặt trong RAM */
      if (!PAGING_PAGE_PRESENT(vic_pte)) {
        MEMPHY_put_freefp(mswp, swpfpn);
        return -1;
      }

      /* Sao chép dữ liệu frame của victim từ RAM sang SWAP */
      if (__swap_cp_page(mram, vicfpn, mswp, swpfpn) != 0) {
        MEMPHY_put_freefp(mswp, swpfpn);
        return -1;   // Lỗi copy
      }

      /* Cập nhật PTE của victim:
         - đặt trạng thái swapped
         - lưu offset của frame trong swap
      */
      pte_set_swap(caller, vicpgn, 0, swpfpn);

      /* Gán lại frame RAM của victim cho trang pgn đang cần */
      if (pte_set_fpn(caller, pgn, vicfpn) != 0) {
        /* Nếu gán frame lỗi → phục hồi lại PTE của victim */
        pte_set_fpn(caller, vicpgn, vicfpn);
        MEMPHY_put_freefp(mswp, swpfpn);
        return -1;
      }

      /* Ghi nhận trang pgn vừa được đưa vào RAM vào danh sách FIFO */
      enlist_pgn_node(&mm->fifo_pgn, pgn);
    }
  }

  /* Trả về frame number đã map */
  *fpn = PAGING_FPN(pte_get_entry(caller,pgn));

  if (!PAGING_PAGE_PRESENT(pte_get_entry(caller,pgn))) {
    return -1;
  }

  return 0;
}


/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  /* Tách địa chỉ ảo thành:
     - pgn: số trang ảo
     - off: offset trong trang
  */
  addr_t pgn = PAGING_PGN(addr);
  addr_t off = PAGING_OFFST(addr);
  addr_t fpn;

  /* Đảm bảo trang pgn đã nằm trong RAM.
     Nếu trang bị swap-out → pg_getpage sẽ tự swap-in.
  */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* truy cập trang lỗi hoặc không hợp lệ */

  /* Tính địa chỉ vật lý: physical address = frame number + offset */
  addr_t phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  /* Đọc 1 byte từ RAM tại địa chỉ vật lý vừa tính */
  if (MEMPHY_read(caller->krnl->mram, phyaddr, data) != 0) {
    return -1;  // đọc thất bại
  }

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  /* Tách địa chỉ ảo thành số trang và offset */
  addr_t pgn = PAGING_PGN(addr);
  addr_t off = PAGING_OFFST(addr);
  addr_t fpn;

  /* Đảm bảo trang đã được nạp vào RAM (fault nếu cần) */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* không thể truy cập trang */

  /* Tính địa chỉ vật lý trong RAM */
  addr_t phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  /* Ghi 1 byte vào RAM tại địa chỉ tính được */
  if (MEMPHY_write(caller->krnl->mram, phyaddr, value) != 0) {
    return -1;  // ghi thất bại
  }

  return 0;
}
/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* Validate inputs and region bounds under mutex */
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (!currg || !cur_vma) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* Check region initialized and offset in-range (ranges are [start, end) ) */
  if (currg->rg_start >= currg->rg_end) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  addr_t rsize = currg->rg_end - currg->rg_start;
  if (offset < 0 || (addr_t)offset >= rsize) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  addr_t vaddr = currg->rg_start + offset;
  int ret = pg_getval(caller->mm, vaddr, data, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return ret;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    addr_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  *destination = data;
  printf("libread:%d, PID: %i\n", __LINE__, proc->pid);
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  pthread_mutex_lock(&mmvm_lock);
  print_pgtbl(proc, 0, -1); // print max TBL
  pthread_mutex_unlock(&mmvm_lock);
#endif
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* Check region initialized and offset in-range */
  if (currg->rg_start >= currg->rg_end) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  addr_t rsize = currg->rg_end - currg->rg_start;
  if (offset < 0 || (addr_t)offset >= rsize) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  int pg_result = pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return pg_result;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1)
  {
    return -1;
  }
  printf("libwrite:%d, PID: %i\n", __LINE__, proc->pid);
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
#ifdef PAGETBL_DUMP
  pthread_mutex_lock(&mmvm_lock);
  print_pgtbl(proc, 0, -1); // print max TBL
  pthread_mutex_unlock(&mmvm_lock);
#else
  MEMPHY_dump(proc->krnl->mram);
#endif
#endif
  return val;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (!pg)
  {
    return -1;
  }
  struct pgn_t *prev = NULL;
  while (pg->pg_next)
  {
    prev = pg;
    pg = pg->pg_next;
  }
  *retpgn = pg->pgn;
  
  if (prev != NULL) {
    prev->pg_next = NULL;
  } else {
    // Only one node in the list, need to update mm->fifo_pgn
    mm->fifo_pgn = NULL;
  }

  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        {                                /*End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    else
    {
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

//#endif