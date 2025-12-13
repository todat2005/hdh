/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}


/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Extract page direactories */
	*pgd = (addr&PAGING64_ADDR_PGD_MASK)>>PAGING64_ADDR_PGD_LOBIT;
	*p4d = (addr&PAGING64_ADDR_P4D_MASK)>>PAGING64_ADDR_P4D_LOBIT;
	*pud = (addr&PAGING64_ADDR_PUD_MASK)>>PAGING64_ADDR_PUD_LOBIT;
	*pmd = (addr&PAGING64_ADDR_PMD_MASK)>>PAGING64_ADDR_PMD_LOBIT;
	*pt = (addr&PAGING64_ADDR_PT_MASK)>>PAGING64_ADDR_PT_LOBIT;

	/* TODO: implement the page direactories mapping */

	return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table 
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)
{
	/* Shift the address to get page num and perform the mapping*/
	return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                         pgd,p4d,pud,pmd,pt);
}


/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)
{
  struct krnl_t *krnl = caller->krnl;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  // dummy pte alloc to avoid runtime error
  pte = malloc(sizeof(addr_t));
#ifdef MM64	
  /* Get value from the system */
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);

  //Use tree walk to get the pte
  struct mm_struct *mm = krnl->mm;

  uint64_t *p4d_table = (uint64_t *)mm->pgd[pgd];
  uint64_t *pud_table = (uint64_t *)p4d_table[p4d];
  uint64_t *pmd_table = (uint64_t *)pud_table[pud];
  uint64_t *pt_table = (uint64_t *)pmd_table[pmd];
  pte = (addr_t *)&pt_table[pt];
  
#else
  pte = &krnl->mm->pgd[pgn];
#endif
	
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)
{
  struct krnl_t *krnl = caller->krnl;

  addr_t *pte;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
	
  // dummy pte alloc to avoid runtime error
  pte = malloc(sizeof(addr_t));
#ifdef MM64	
  /* Get value from the system */
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);

  //Use tree walk to get the pte
  struct mm_struct *mm = krnl->mm;

  uint64_t *p4d_table = (uint64_t *)mm->pgd[pgd];
  uint64_t *pud_table = (uint64_t *)p4d_table[p4d];
  uint64_t *pmd_table = (uint64_t *)pud_table[pud];
  uint64_t *pt_table  = (uint64_t *)pmd_table[pmd];

  pte = (addr_t *)&pt_table[pt];

#else
  pte = &krnl->mm->pgd[pgn];
#endif

  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}


/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  struct krnl_t *krnl = caller->krnl;
  uint32_t pte = 0;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t	pt=0;
	
  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
	
  //Use tree walk to get the pte
  struct mm_struct *mm = krnl->mm;

  if (mm->pgd[pgd]) {
      uint64_t *p4d_table = (uint64_t *)mm->pgd[pgd];
      if (p4d_table[p4d]) {
          uint64_t *pud_table = (uint64_t *)p4d_table[p4d];
          if (pud_table[pud]) {
              uint64_t *pmd_table = (uint64_t *)pud_table[pud];
              if (pmd_table[pmd]) {
                  uint64_t *pt_table = (uint64_t *)pmd_table[pmd];
                  // Đích đến: Lấy giá trị chứa trong ô nhớ (không lấy địa chỉ & nữa)
                  // Ép kiểu về uint32_t vì hàm yêu cầu trả về uint32_t
                  pte = (uint32_t)pt_table[pt];
              }
          }
      }
  }

  return pte;
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
	struct krnl_t *krnl = caller->krnl;
	uint64_t pte = 0;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t	pt=0;

  /* TODO Perform multi-level page mapping */
  get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);
  //Use tree walk to get the pte
  struct mm_struct *mm = krnl->mm;
  if (mm->pgd[pgd]) {
      uint64_t *p4d_table = (uint64_t *)mm->pgd[pgd];
      if (p4d_table[p4d]) {
          uint64_t *pud_table = (uint64_t *)p4d_table[p4d];
          if (pud_table[pud]) {
              uint64_t *pmd_table = (uint64_t *)pud_table[pud];
              if (pmd_table[pmd]) {
                  uint64_t *pt_table = (uint64_t *)pmd_table[pmd];
                  // Đích đến: Ghi giá trị vào ô nhớ
                  pt_table[pt] = pte_val;
              }
          }
      }
  }
	
	return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  int pgit = 0;
  uint64_t pattern = 0xdeadbeef;

  /* TODO memset the page table with given pattern
   */

  struct mm_struct *mm = caller->krnl->mm;
  addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;

  for (pgit = 0; pgit < pgnum; pgit++) 
  {
      // Tính địa chỉ ảo của trang hiện tại. Mỗi trang cách nhau 4096 bytes (tương ứng 12 bit offset)
      addr_t current_addr = addr + (pgit * PAGING64_PAGESZ); 

      // Lấy các chỉ số index
      get_pd_from_address(current_addr, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

      // Leo qua các cấp độ của bảng trang và tạo bảng con nếu chưa tồn tại
      
      if (mm->pgd[pgd_idx] == 0) {
          mm->pgd[pgd_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
          memset((void*)mm->pgd[pgd_idx], 0, 512 * sizeof(uint64_t));
      }
      uint64_t *p4d_table = (uint64_t *)mm->pgd[pgd_idx];

      if (p4d_table[p4d_idx] == 0) {
          p4d_table[p4d_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
          memset((void*)p4d_table[p4d_idx], 0, 512 * sizeof(uint64_t));
      }
      uint64_t *pud_table = (uint64_t *)p4d_table[p4d_idx];

      if (pud_table[pud_idx] == 0) {
          pud_table[pud_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
          memset((void*)pud_table[pud_idx], 0, 512 * sizeof(uint64_t));
      }
      uint64_t *pmd_table = (uint64_t *)pud_table[pud_idx];

      if (pmd_table[pmd_idx] == 0) {
          pmd_table[pmd_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
          memset((void*)pmd_table[pmd_idx], 0, 512 * sizeof(uint64_t));
      }
      uint64_t *pt_table = (uint64_t *)pmd_table[pmd_idx];

      // Ghi Pattern vào Entry cuối cùng. Đánh dấu vùng nhớ này bằng giá trị 0xdeadbeef
      pt_table[pt_idx] = pattern;
  }

  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                   // no guarantee all given pages are mapped
  struct framephy_struct *fpit = frames;
  int pgit = 0;
  addr_t pgn;
  struct mm_struct *mm = caller->krnl->mm;

  /* TODO: update the rg_end and rg_start of ret_rg 
  //ret_rg->rg_end =  ....
  //ret_rg->rg_start = ...
  //ret_rg->vmaid = ... 
  */

  // ret_rg là struct trả về để báo cho user biết vùng nhớ nằm ở đâu
  ret_rg->rg_start = addr;
  // Giả sử PAGING_PAGESZ là kích thước trang (4096 hoặc 256 tùy config, nhưng thường có macro này)
  ret_rg->rg_end = addr + (pgnum * PAGING_PAGESZ);

  /* TODO map range of frame to address space
   *      [addr to addr + pgnum*PAGING_PAGESZ
   *      in page table caller->krnl->mm->pgd,
   *                    caller->krnl->mm->pud...
   *                    ...
   */

  
  //enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn64 + pgit);

  // Vòng lặp duyệt qua số lượng trang cần map (pgnum)
  for(pgit = 0; pgit < pgnum; pgit++) 
  {
     if (fpit == NULL) break; // Hết frame vật lý để map (Safety check)

     // Tính Page Number (PGN) cho địa chỉ hiện tại. Địa chỉ hiện tại = Địa chỉ gốc + (thứ tự trang * kích thước trang)
     addr_t current_addr = addr + (pgit * PAGING_PAGESZ);
     pgn = PAGING_PGN(current_addr); //Hoặc: current_addr >> 12;

     // Đảm bảo cây phân trang tồn tại (Logic Allocate), vì pte_set_fpn chỉ điền dữ liệu, không tạo bảng mới.
     addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
     get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

     // Kiểm tra và malloc từng cấp nếu thiếu (giống vmap_pgd_memset)
     if (mm->pgd[pgd_idx] == 0) {
         mm->pgd[pgd_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
         memset((void*)mm->pgd[pgd_idx], 0, 512 * sizeof(uint64_t));
     }
     uint64_t *p4d_tbl = (uint64_t *)mm->pgd[pgd_idx];

     if (p4d_tbl[p4d_idx] == 0) {
         p4d_tbl[p4d_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
         memset((void*)p4d_tbl[p4d_idx], 0, 512 * sizeof(uint64_t));
     }
     uint64_t *pud_tbl = (uint64_t *)p4d_tbl[p4d_idx];

     if (pud_tbl[pud_idx] == 0) {
         pud_tbl[pud_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
         memset((void*)pud_tbl[pud_idx], 0, 512 * sizeof(uint64_t));
     }
     uint64_t *pmd_tbl = (uint64_t *)pud_tbl[pud_idx];

     if (pmd_tbl[pmd_idx] == 0) {
         pmd_tbl[pmd_idx] = (addr_t)malloc(512 * sizeof(uint64_t));
         memset((void*)pmd_tbl[pmd_idx], 0, 512 * sizeof(uint64_t));
     }

     // Map Frame vật lý vào bảng phân trang
     pte_set_fpn(caller, pgn, fpit->fpn);

    /* Tracking for later page replacement activities (if needed)
    * Enqueue new usage page */
     // Thêm vào danh sách quản lý trang (FIFO) của tiến trình
     enlist_pgn_node(&mm->fifo_pgn, pgn);
     
     // Chuyển sang frame tiếp theo trong danh sách liên kết
     fpit = fpit->fp_next;
  }

  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
  addr_t fpn;
  int pgit;
  struct framephy_struct *newfp_str = NULL;

  /* TODO: allocate the page 
  //caller-> ...
  //frm_lst-> ...
  */
  for (pgit = 0; pgit < req_pgnum; pgit++)
  {
    // TODO: allocate the page 
    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0)
    {
      newfp_str = (struct framephy_struct*)malloc(sizeof(struct framephy_struct));
      newfp_str->fpn = fpn;

      newfp_str->fp_next = *frm_lst; // Trỏ node mới vào đầu danh sách hiện tại
      *frm_lst = newfp_str;          // Cập nhật đầu danh sách là node mới
    }
    else
    { // TODO: ERROR CODE of obtaining somes but not enough frames
      //Thu hồi lại các frame đã lỡ cấp phát (Rollback)
       struct framephy_struct *p = *frm_lst;
       while(p != NULL) {
           struct framephy_struct *temp = p;
           MEMPHY_put_freefp(caller->krnl->mram, temp->fpn);
           p = p->fp_next;
           free(temp);
       }
       
       // Reset lại list trả về là NULL
       *frm_lst = NULL;
      return -1;
    }
  }
  /* End TODO */

  return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
  struct framephy_struct *frm_lst = NULL;
  addr_t ret_alloc = 0;
//  int pgnum = incpgnum;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide
   *duplicate control mechanism, keep it simple
   */
  // ret_alloc = alloc_pages_range(caller, pgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000)
  {
    return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
   vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  return 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));

  /* TODO init page table directory */
   //mm->pgd = ...
   //mm->p4d = ...
   //mm->pud = ...
   //mm->pmd = ...
   //mm->pt = ...

  mm->pgd = malloc(512 * sizeof(uint64_t));
  // Quan trọng: Xóa sạch rác trong PGD vừa cấp phát để tránh trỏ lung tung
  memset(mm->pgd, 0, 512 * sizeof(uint64_t));
  mm->p4d = NULL;
  mm->pud = NULL;
  mm->pmd = NULL;
  mm->pt  = NULL;

  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* TODO update VMA0 next */
  // vma0->next = ...
  // Đây là VMA đầu tiên và duy nhất, nên next là NULL
  vma0->vm_next = NULL;

  /* Point vma owner backward */
  //vma0->vm_mm = mm; 
  // Link ngược từ VMA về struct quản lý bộ nhớ (Uncomment dòng này)
  vma0->vm_mm = mm;

  /* TODO: update mmap */
  //mm->mmap = ...
  //mm->symrgtbl = ...
  // Gán vma0 làm node đầu tiên của danh sách các vùng nhớ (mmap)
  mm->mmap = vma0;
  //Khởi tạo symrgtbl
  memset(mm->symrgtbl, 0, PAGING_MAX_SYMTBL_SZ * sizeof(struct vm_rg_struct));

  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn)
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->"  FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
//  addr_t pgn_start;//, pgn_end;
//  addr_t pgit;
//  struct krnl_t *krnl = caller->krnl;

  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;

  get_pd_from_address(start, &pgd, &p4d, &pud, &pmd, &pt);

  /* TODO traverse the page map and dump the page directory entries */
  struct mm_struct *mm = caller->krnl->mm;
  addr_t cur;
  for (cur = start; cur < end; cur += PAGING_PAGESZ) 
  {
      // 1. Cập nhật lại các chỉ số index cho địa chỉ hiện tại (cur)
      get_pd_from_address(cur, &pgd, &p4d, &pud, &pmd, &pt);

      if (mm->pgd[pgd] == 0) continue; // Nhánh này chưa có, bỏ qua
      uint64_t *p4d_tbl = (uint64_t *)mm->pgd[pgd];
      if (p4d_tbl[p4d] == 0) continue;
      uint64_t *pud_tbl = (uint64_t *)p4d_tbl[p4d];
      if (pud_tbl[pud] == 0) continue;
      uint64_t *pmd_tbl = (uint64_t *)pud_tbl[pud];
      if (pmd_tbl[pmd] == 0) continue;
      uint64_t *pt_tbl = (uint64_t *)pmd_tbl[pmd];

      uint64_t pte_val = pt_tbl[pt];

      //In ra màn hình nếu Entry đó có dữ liệu
      if (pte_val != 0) {
          printf("VA: %08lx -> [Index: %ld|%ld|%ld|%ld|%ld] -> PTE: %08lx\n", 
                 cur, pgd, p4d, pud, pmd, pt, pte_val);
      }
  }

  return 0;
}

#endif  //def MM64
