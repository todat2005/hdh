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
//  struct krnl_t *krnl = caller->krnl;

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
  //... krnl->mm->pgd
  //... krnl->mm->pt
  //pte = &krnl->mm->pt;
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
  struct krnl_t *krnl;
  struct mm_struct *mm;
  addr_t pte_val;
  addr_t pgd_idx=0, p4d_idx=0, pud_idx=0, pmd_idx=0, pt_idx=0;

  if (!caller || !caller->krnl) return -1;
  
  krnl = caller->krnl;
  mm = krnl->mm;

#ifdef MM64
  /* Lấy chỉ số 5 cấp từ page number */
  get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

  /* Cấp phát các bảng nếu chưa có */
  if (!mm->pgd) mm->pgd = calloc(512, sizeof(uint64_t));
  if (!mm->p4d) mm->p4d = calloc(512, sizeof(uint64_t));
  if (!mm->pud) mm->pud = calloc(512, sizeof(uint64_t));
  if (!mm->pmd) mm->pmd = calloc(512, sizeof(uint64_t));
  if (!mm->pt)  mm->pt  = calloc(512, sizeof(uint64_t));

  /* Đọc, chỉnh sửa, ghi lại PTE value */
  pte_val = mm->pt[pt_idx];  // ✓ ĐÚNG: pt_idx là chỉ số
  
  SETBIT(pte_val, PAGING_PTE_PRESENT_MASK);
  CLRBIT(pte_val, PAGING_PTE_SWAPPED_MASK);
  SETVAL(pte_val, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  mm->pt[pt_idx] = pte_val;

  return 0;

#else
  /* Chế độ non-MM64: single-level page table */
  if (!krnl->mm) return -1;
  
  pte_val = krnl->mm->pgd[pgn];
  SETBIT(pte_val, PAGING_PTE_PRESENT_MASK);
  CLRBIT(pte_val, PAGING_PTE_SWAPPED_MASK);
  SETVAL(pte_val, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
  
  krnl->mm->pgd[pgn] = pte_val;

  return 0;
#endif
}


/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)
{
  struct mm_struct *mm;
  uint32_t pte_val = 0;
  addr_t pgd_idx=0, p4d_idx=0, pud_idx=0, pmd_idx=0, pt_idx=0;
  
  mm = caller->krnl->mm;

#ifdef MM64
  /* Lấy chỉ số 5 cấp từ page number */
  get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

  /* Kiểm tra: các bảng phải đã được cấp phát (trong init_mm) */

  /* Đọc giá trị PTE từ bảng trang cuối cùng */
  pte_val = mm->pt[pt_idx];

  return pte_val;

#else
  /* Chế độ non-MM64: single-level page table */
  pte_val = krnl->mm->pgd[pgn];

  return pte_val;
#endif
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)
{
	struct krnl_t *krnl = caller->krnl;
	krnl->mm->pgd[pgn]=pte_val;
	
	return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
int vmap_pgd_memset(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum)                      // num of mapping page
{
  struct krnl_t *krnl;
  struct mm_struct *mm;
  addr_t pte_val = 0xdeadbeef;  /* Mẫu khởi tạo (0xdeadbeef) */
  int pgit;
  addr_t cur_addr;
  addr_t pgd_idx=0, p4d_idx=0, pud_idx=0, pmd_idx=0, pt_idx=0;

  if (!caller || !caller->krnl) return -1;
  
  krnl = caller->krnl;
  mm = krnl->mm;

#ifdef MM64
  /* Duyệt qua pgnum trang liên tục */
  for (pgit = 0; pgit < pgnum; pgit++) {
    /* Địa chỉ hiện tại của trang thứ pgit */
    cur_addr = addr + (pgit * PAGING_PAGESZ);
    
    /* Lấy chỉ số 5 cấp từ địa chỉ hiện tại */
    get_pd_from_address(cur_addr, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);
    
    /* Cấp phát các bảng nếu chưa có */
    if (!mm->pgd) mm->pgd = calloc(512, sizeof(uint64_t));
    if (!mm->p4d) mm->p4d = calloc(512, sizeof(uint64_t));
    if (!mm->pud) mm->pud = calloc(512, sizeof(uint64_t));
    if (!mm->pmd) mm->pmd = calloc(512, sizeof(uint64_t));
    if (!mm->pt)  mm->pt  = calloc(512, sizeof(uint64_t));
    
    /* Ghi mẫu vào PTE */
    mm->pt[pt_idx] = pte_val;
  }

  return 0;

#else
  /* Chế độ non-MM64: single-level page table */
  for (pgit = 0; pgit < pgnum; pgit++) {
    addr_t pgn = (addr + pgit * PAGING_PAGESZ) >> PAGING64_ADDR_PT_SHIFT;
    krnl->mm->pgd[pgn] = pte_val;
  }

  return 0;
#endif
}

/*
 * vmap_page_range - map a range of pages to given frames
 * @caller : process calling
 * @addr   : start virtual address (aligned to page size)
 * @pgnum  : number of pages requested to map
 * @frames : linked list of framephy_struct (fpn) to map (may be shorter than pgnum)
 * @ret_rg : returned region info (rg_start/rg_end)
 *
 * Trả về: số trang thực tế đã map (>=0), hoặc -1 nếu có lỗi
 *
 * Lưu ý: hàm này đơn giản hóa: giả sử mỗi bảng cấp đã được tổ chức
 * như mảng 512 entries (như các hàm khác trong dự án).
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                    addr_t addr,                       // start address which is aligned to pagesz
                    int pgnum,                      // num of mapping page
                    struct framephy_struct *frames, // list of the mapped frames
                    struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{
    struct krnl_t *krnl;
    struct mm_struct *mm;
    struct framephy_struct *fp;
    int mapped = 0;
    int i;
    addr_t cur_addr;
    addr_t pgd_idx=0, p4d_idx=0, pud_idx=0, pmd_idx=0, pt_idx=0;
    uint64_t pte_val = 0;

    if (!caller || !caller->krnl || !ret_rg) return -1;
    krnl = caller->krnl;
    mm = krnl->mm;
    if (!mm) return -1;

    /* Nếu frames rỗng thì không có gì để map */
    if (!frames) {
        ret_rg->rg_start = addr;
        ret_rg->rg_end = addr;
        return 0;
    }

#ifdef MM64
    /* Đảm bảo các mảng page-directory đã có (đơn giản: cấp phát flat 512 entries) */
    if (!mm->pgd) mm->pgd = calloc(512, sizeof(uint64_t));
    if (!mm->p4d) mm->p4d = calloc(512, sizeof(uint64_t));
    if (!mm->pud) mm->pud = calloc(512, sizeof(uint64_t));
    if (!mm->pmd) mm->pmd = calloc(512, sizeof(uint64_t));
    if (!mm->pt)  mm->pt  = calloc(512, sizeof(uint64_t));

    fp = frames;
    for (i = 0; i < pgnum && fp != NULL; i++, fp = fp->fp_next) {
        cur_addr = addr + (i * PAGING_PAGESZ);

        /* Lấy chỉ số PT index từ địa chỉ */
        get_pd_from_address(cur_addr, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx);

        /* Tạo PTE: mark present, clear swapped, set FPN */
        pte_val = 0;
        SETBIT(pte_val, PAGING_PTE_PRESENT_MASK);
        CLRBIT(pte_val, PAGING_PTE_SWAPPED_MASK);
        SETVAL(pte_val, fp->fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

        /* Ghi vào page table (PT) tại vị trí pt_idx */
        mm->pt[pt_idx] = pte_val;

        /* Có thể enqueue pgn vào fifo_pgn để tracking (nếu cần) */
        // enlist_pgn_node(&mm->fifo_pgn, /* pgn value */);

        mapped++;
    }

#else
    /* Non-MM64: simple single-level page table */
    fp = frames;
    for (i = 0; i < pgnum && fp != NULL; i++, fp = fp->fp_next) {
        addr_t pgn = (addr + i * PAGING_PAGESZ) >> PAGING_ADDR_PGN_LOBIT;
        pte_val = 0;
        SETBIT(pte_val, PAGING_PTE_PRESENT_MASK);
        CLRBIT(pte_val, PAGING_PTE_SWAPPED_MASK);
        SETVAL(pte_val, fp->fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
        mm->pgd[pgn] = (uint32_t)pte_val;
        mapped++;
    }
#endif

    /* Cập nhật ret_rg thông tin vùng đã map */
    ret_rg->rg_start = addr;
    ret_rg->rg_end = addr + (mapped * PAGING_PAGESZ);

    return mapped;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
    struct memphy_struct *mram;
    struct framephy_struct *head = NULL;
    struct framephy_struct *tail = NULL;
    struct framephy_struct *node, *it;
    addr_t fpn;
    int i;

    if (!frm_lst) return -1;
    *frm_lst = NULL;

    if (!caller || !caller->krnl) return -1;
    mram = caller->krnl->mram;
    if (!mram) return -1;

    if (req_pgnum <= 0) return 0;

    /* Fail-fast safety check:
     * Nếu số frame rảnh trên memphy < req_pgnum thì trả lỗi ngay.
     * Lý do: trong thiết kế đơn giản này chúng ta KHÔNG cố thực hiện
     * swap-off/eviction phức tạp bên trong hàm cấp phát, vì nếu swap-off
     * bản thân nó cần frame thì có thể dẫn tới vòng lặp vô tận / deadlock.
     */
    {
      int avail = 0;
      struct framephy_struct *tmp = mram->free_fp_list;
      while (tmp) { avail++; tmp = tmp->fp_next; }
      if (avail < req_pgnum) {
        return -3000; /* not enough free frames */
      }
    }

  /*
   * Cố gắng cấp phát `req_pgnum` frame từ thiết bị MEMPHY (mram).
   * Thuật toán:
   *  - Gọi `MEMPHY_get_freefp` để lấy một FPN (frame physical number).
   *  - Nếu thành công, tạo một node `framephy_struct` lưu FPN đó và nối vào
   *    danh sách kết quả (head..tail) trả về qua `*frm_lst`.
   *  - Nếu không thể cấp phát node struct (malloc thất bại), ta hoàn nguyên:
   *      + Trả lại FPN vừa lấy vào memphy bằng `MEMPHY_put_freefp`.
   *      + Trả lại tất cả FPN đã lấy trước đó (MEMPHY_put_freefp) và giải phóng node.
   *      + Trả về mã lỗi `-3000` (theo quy ước của project để báo OOM / không đủ tài nguyên).
   *  - Nếu `MEMPHY_get_freefp` báo không còn frame (trả != 0), ta rollback toàn bộ
   *    các frame đã lấy và trả về `-3000`.
   *
   * Giá trị trả về:
   *  - >=0 : số frame đã cấp (ở đây trả về `req_pgnum` khi cấp thành công)
   *  - -3000: không đủ frame / OOM (rollback đã thực hiện)
   *  - -1: tham số không hợp lệ
   */
  for (i = 0; i < req_pgnum; i++) {
    /* Lấy một frame từ memphy */
    if (MEMPHY_get_freefp(mram, &fpn) == 0) {
      /* Cấp phát node theo dõi frame */
      node = malloc(sizeof(struct framephy_struct));
      if (!node) {
        /* Nếu không cấp được node, trả lại frame vừa lấy và rollback */
        MEMPHY_put_freefp(mram, fpn);

        /* Rollback: trả lại mọi frame đã lấy trước đó và giải phóng node list */
        it = head;
        while (it) {
          MEMPHY_put_freefp(mram, it->fpn);
          node = it->fp_next;
          free(it);
          it = node;
        }
        *frm_lst = NULL;
        return -3000;
      }

      /* Gán thông tin cho node và nối vào danh sách kết quả */
      node->fpn = fpn;
      node->fp_next = NULL;
      node->owner = caller->krnl->mm; /* ghi owner để tiện tracking */

      if (!head) {
        head = tail = node;
      } else {
        tail->fp_next = node;
        tail = node;
      }
    } else {
      /* Nếu MEMPHY không còn frame trả về: rollback toàn bộ và báo lỗi */
      it = head;
      while (it) {
        MEMPHY_put_freefp(mram, it->fpn);
        node = it->fp_next;
        free(it);
        it = node;
      }
      *frm_lst = NULL;
      return -3000;
    }
  }

  /* Thành công: trả về danh sách frame và số frame đã cấp */
  *frm_lst = head;
  return (addr_t)req_pgnum;
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
  int pgnum = incpgnum;

  /* LƯU Ý: hàm cấp phát frame ở mức này giữ đơn giản (fail-fast).
   * - Trước khi cố lấy các frame từ memphy, chúng ta kiểm tra số frame
   *   rảnh thực tế và nếu không đủ thì KHÔNG cố thực hiện swap-off/evict.
   * - Lý do: swap-off có thể cần frame/tài nguyên tạm và dễ gây vòng lặp
   *   vô tận hoặc deadlock trong thiết kế đơn giản của dự án này.
   * - Hậu quả: nếu yêu cầu > số frame rảnh, hàm sẽ trả lỗi (-3000) và
   *   caller phải xử lý (ví dụ: cấp từng phần, báo ENOMEM, hoặc retry).
   */
  ret_alloc = alloc_pages_range(caller, pgnum, &frm_lst);

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

  /* Kiểm tra tham số */
  if (!mm || !vma0) {
    if (vma0) free(vma0);
    return -1;
  }

  /*
   * Khởi tạo page table directories bằng eager allocation.
   * Nếu bất kỳ calloc nào thất bại, ta phải giải phóng những vùng
   * đã cấp trước đó để tránh leak và trả lỗi.
   */
  mm->pgd = calloc(512, sizeof(uint64_t)); /* PGD level */
  mm->p4d = calloc(512, sizeof(uint64_t)); /* P4D level */
  mm->pud = calloc(512, sizeof(uint64_t)); /* PUD level */
  mm->pmd = calloc(512, sizeof(uint64_t)); /* PMD level */
  mm->pt  = calloc(512, sizeof(uint64_t)); /* PT level (leaf) */

  if (!mm->pgd || !mm->p4d || !mm->pud || !mm->pmd || !mm->pt) {
    /* Giải phóng những allocation đã thành công trước đó */
    if (mm->pgd) free(mm->pgd);
    if (mm->p4d) free(mm->p4d);
    if (mm->pud) free(mm->pud);
    if (mm->pmd) free(mm->pmd);
    if (mm->pt)  free(mm->pt);
    free(vma0);
    return -1;
  }

  /* Khởi tạo các trường quản lý của mm */
  mm->mmap = NULL;       /* head của danh sách VMA */
  mm->symrgtbl = NULL;   /* bảng symbol/region (nếu dùng) */
  mm->fifo_pgn = NULL;   /* FIFO tracking page numbers (optional) */


  /* By default the owner comes with at least one vma */

  /*
   * Thiết lập VMA mặc định (rỗng) cho tiến trình:
   * - vma0 là VMA đầu tiên dùng để quản lý vùng ảo ban đầu.
   * - vm_freerg_list lưu các region rỗi trong VMA (dùng cho sbrk/mmap sau này).
   */
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  vma0->vm_freerg_list = NULL; /* danh sách region rỗi ban đầu */
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* Liên kết VMA vào mm */
  vma0->vm_next = NULL;
  vma0->vm_mm = mm; /* con trỏ về owner mm */

  /* Đặt vma0 là head của danh sách mmap trong mm */
  mm->mmap = vma0;

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
  addr_t pgn_start, pgn_end;
  addr_t pgit;
  struct krnl_t *krnl = caller->krnl;
  mm_struct *mm = krnl->mm;
  addr_t pgd=0;
  addr_t p4d=0;
  addr_t pud=0;
  addr_t pmd=0;
  addr_t pt=0;
  for(pgit = start; pgit < end; pgit += PAGING_PAGESZ)
  {
    get_pd_from_address(pgit, &pgd, &p4d, &pud, &pmd, &pt);
    printf("print_pgtbl:\nPDG=%lld, P4g=%lld, PUD=%lld, PMD=%lld\n",
           pgit, pgd, p4d, pud, pmd);
  }
  /* TODO traverse the page map and dump the page directory entries */

  return 0;
}

//#endif  //def MM64
