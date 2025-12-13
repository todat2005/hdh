#include "loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t avail_pid = 1; 
/* Biến PID toàn cục – mỗi lần load process mới thì tăng lên */

#define OPT_CALC    "calc"
#define OPT_ALLOC   "alloc"
#define OPT_FREE    "free"
#define OPT_READ    "read"
#define OPT_WRITE   "write"
#define OPT_SYSCALL "syscall"

/* -------------------------------------------------------
   Chuyển chuỗi opcode (ví dụ: "alloc") thành enum opcode
   ------------------------------------------------------- */
static enum ins_opcode_t get_opcode(char * opt) {
    if (!strcmp(opt, OPT_CALC)) {
        return CALC;
    }else if (!strcmp(opt, OPT_ALLOC)) {
        return ALLOC;
    }else if (!strcmp(opt, OPT_FREE)) {
        return FREE;
    }else if (!strcmp(opt, OPT_READ)) {
        return READ;
    }else if (!strcmp(opt, OPT_WRITE)) {
        return WRITE;
    }else if (!strcmp(opt, OPT_SYSCALL)) {
        return SYSCALL;
    }else{
        // Nếu opcode không hợp lệ → báo lỗi và dừng lại
        printf("get_opcode return Opcode: %s\n", opt);
        exit(1);
    }
}

/* -------------------------------------------------------
   Hàm load(): Đọc file mô tả tiến trình và tạo PCB tương ứng
   ------------------------------------------------------- */
struct pcb_t * load(const char * path) {

    /* Tạo PCB mới cho process */
    struct pcb_t * proc = (struct pcb_t *)malloc(sizeof(struct pcb_t));

    proc->pid = avail_pid;     // Gán PID
    avail_pid++;               // Tăng PID cho process tiếp theo

    proc->page_table =
        (struct page_table_t*)malloc(sizeof(struct page_table_t));

    proc->bp = PAGE_SIZE;      // Base pointer ban đầu
    proc->pc = 0;              // Program counter bắt đầu từ 0

    /* Mở file mô tả tiến trình */
    FILE * file;
    if ((file = fopen(path, "r")) == NULL) {
        printf("Cannot find process description at '%s'\n", path);
        exit(1);
    }

    /* Lưu đường dẫn file vào PCB */
    snprintf(proc->path, 2*sizeof(path)+1, "%s", path);

    /* Chuẩn bị load code segment */
    char opcode[10];
    proc->code = (struct code_seg_t*)malloc(sizeof(struct code_seg_t));

    /* Đọc dòng đầu tiên: priority + số instruction */
    fscanf(file, "%u %u", &proc->priority, &proc->code->size);

    /* Cấp bộ nhớ cho mảng instruction */
    proc->code->text = (struct inst_t*)malloc(
        sizeof(struct inst_t) * proc->code->size
    );

    uint32_t i = 0;
    char buf[200];

    /* --------------------------------------------
       Đọc lần lượt từng instruction trong file
       -------------------------------------------- */
    for (i = 0; i < proc->code->size; i++) {

        fscanf(file, "%s", opcode);                // đọc opcode dạng text
        proc->code->text[i].opcode = get_opcode(opcode);   // chuyển sang enum

        switch(proc->code->text[i].opcode) {

        case CALC:
            // CALC không có tham số nên không đọc gì thêm
            break;

        case ALLOC:
            // ALLOC a b  → cần đọc 2 tham số
            fscanf(
                file,
                "" FORMAT_ARG " " FORMAT_ARG "\n",
                &proc->code->text[i].arg_0,
                &proc->code->text[i].arg_1
            );
            break;

        case FREE:
            // FREE a  → chỉ có 1 tham số
            fscanf(file, "" FORMAT_ARG "\n", &proc->code->text[i].arg_0);
            break;

        case READ:
        case WRITE:
            // READ / WRITE có 3 tham số
            fscanf(
                file,
                "" FORMAT_ARG " " FORMAT_ARG " " FORMAT_ARG "\n",
                &proc->code->text[i].arg_0,
                &proc->code->text[i].arg_1,
                &proc->code->text[i].arg_2
            );
            break;    

        case SYSCALL:
            /* SYSCALL có thể nhiều tham số → dùng fgets + sscanf */
            fgets(buf, sizeof(buf), file);
            sscanf(buf, "" FORMAT_ARG "" FORMAT_ARG "" FORMAT_ARG "" FORMAT_ARG "",
                       &proc->code->text[i].arg_0,
                       &proc->code->text[i].arg_1,
                       &proc->code->text[i].arg_2,
                       &proc->code->text[i].arg_3
            );
            break;

        default:
            printf("Opcode: %s\n", opcode);
            exit(1);
        }
    }

    return proc;
}
