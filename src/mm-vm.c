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
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;
  if (mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  /* Kiểm tra đối số cơ bản: caller, kernel và mm phải tồn tại */
  if (!caller || !caller->krnl || !caller->krnl->mm)
    return NULL;
  /* Lấy vm area hiện tại dựa trên vmaid */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL)
    return NULL;
  /* 
   * Xác định kích thước cần cấp:
   * - Nếu caller truyền sẵn 'alignedsz' thì dùng nó (đã căn chỉnh ngoài),
   * - Ngược lại căn 'size' lên theo kích thước trang (PAGING_PAGE_ALIGNSZ).
   */
  addr_t alloc_sz = alignedsz ? alignedsz : PAGING_PAGE_ALIGNSZ(size);
  /* Kiểm tra còn đủ không gian trong vùng ảo (không vượt quá vm_end) */
  if (cur_vma->sbrk + alloc_sz > cur_vma->vm_end)
    return NULL; /* Không đủ chỗ để cấp */
  /* Tạo một vùng nhớ mới cho vm region */  
  newrg = malloc(sizeof(struct vm_rg_struct));
  /* TODO: update the newrg boundary
  */
  /* Thiết lập biên của region: bắt đầu từ sbrk hiện tại, dài = alloc_sz */
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + alloc_sz;
  newrg->rg_next = NULL;

  /* Cập nhật sbrk của vma để "đặt trước" vùng vừa cấp (reserve) */
  cur_vma->sbrk = newrg->rg_end;
  /* END TODO */

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  /* TODO validate the planned memory area is not overlapped */
  if (!caller || !caller->krnl || !caller->krnl->mm)
    return -1;

  if (vmastart >= vmaend)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->krnl->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  /* TODO validate the planned memory area is not overlapped */

  struct vm_area_struct *cur_area = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_area == NULL)
  {
    return -1;
  }

  /* Nếu yêu cầu: vùng đề xuất phải nằm trong cur_area */
  if (vmastart < cur_area->vm_start || vmaend > cur_area->vm_end)
    return -1;

  /* Duyệt các VM area khác và kiểm tra overlap với khoảng đề xuất [vmastart, vmaend) */
  while (vma != NULL)
  {
    if (vma != cur_area && OVERLAP(cur_area->vm_start, cur_area->vm_end, vma->vm_start, vma->vm_end))
    {
      return -1;
    }
    vma = vma->vm_next;
  }
  /* End TODO*/

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  /* Kiểm tra đầu vào */
  if (!caller || !caller->krnl || !caller->krnl->mm || inc_sz == 0)
    return -1;

  /* Căn chỉnh kích thước theo kích thước trang */
  addr_t aligned_sz = PAGING_PAGE_ALIGNSZ(inc_sz);

  /* Lấy vm area cần tăng giới hạn */
  struct vm_area_struct *area = get_vma_by_num(caller->krnl->mm, vmaid);
  if (area == NULL)
    return -1;  
  /* Lưu giới hạn cũ để phục hồi nếu thất bại */
  addr_t old_vm_end = area->vm_end;

  /* Tính vùng mới: từ vm_end hiện tại đến vm_end + aligned_sz */
  addr_t new_vm_end = area->vm_end + aligned_sz;

  /* Kiểm tra overlap của vùng mở rộng với các VMA khác */
  if (validate_overlap_vm_area(caller, vmaid, area->vm_end, new_vm_end) < 0)
    return -1;

  /* Mở rộng vm_end */
  area->vm_end = new_vm_end;

  /* Tính số trang cần cấp */
  int incnumpage = aligned_sz / PAGING_PAGESZ;

  /* Tạo region mới để theo dõi vùng vừa cấp */
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  if (!newrg) {
    area->vm_end = old_vm_end; /* Rollback */
    return -1;
  }

  /* Cố gắng ánh xạ vùng mở rộng vào RAM */
  if (vm_map_ram(caller, area->vm_start, area->vm_end, 
                   old_vm_end, incnumpage, newrg) < 0) {
    area->vm_end = old_vm_end; /* Rollback nếu map thất bại */
    free(newrg);
    return -1;
  }

  free(newrg); /* newrg đã được xử lý bởi vm_map_ram */
  return 0;
}

//#endif
