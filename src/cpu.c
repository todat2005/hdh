#include "cpu.h"
#include "mem.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"

/*
 * calc(): mô phỏng lệnh tính toán (không làm gì thật),
 *         trả về 0 để biểu thị thành công.
 */
int calc(struct pcb_t *proc)
{
    return ((unsigned long)proc & 0UL);
}

/*
 * alloc(): cấp phát bộ nhớ cho process.
 * @size: số byte cần cấp phát
 * @reg_index: ghi địa chỉ trả về vào thanh ghi reg[reg_index]
 */
int alloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    addr_t addr = alloc_mem(size, proc);  // gọi bộ cấp phát cấp thấp

    if (addr == 0)         // 0 = thất bại
        return 1;

    proc->regs[reg_index] = addr;  // lưu địa chỉ vào thanh ghi
    return 0;
}

/*
 * free_data(): giải phóng bộ nhớ đã cấp phát.
 * @reg_index: chứa địa chỉ đã cấp phát
 */
int free_data(struct pcb_t *proc, uint32_t reg_index)
{
    return free_mem(proc->regs[reg_index], proc);
}

/*
 * read(): đọc 1 byte từ bộ nhớ ảo vào thanh ghi.
 *
 * source register + offset → địa chỉ cần đọc
 * sau đó gán vào thanh ghi destination
 */
int read(
    struct pcb_t *proc,
    uint32_t source,       // thanh ghi chứa base address
    uint32_t offset,       // offset cộng thêm vào base
    uint32_t destination   // thanh ghi nhận dữ liệu
){
    BYTE data;

    // đọc byte từ địa chỉ: regs[source] + offset
    if (read_mem(proc->regs[source] + offset, proc, &data))
    {
        proc->regs[destination] = data;  // ghi giá trị byte vào thanh ghi
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
 * write(): ghi 1 byte vào bộ nhớ ảo.
 *
 * địa chỉ đích = regs[destination] + offset
 */
int write(
    struct pcb_t *proc,
    BYTE data,             // dữ liệu cần ghi
    uint32_t destination,  // thanh ghi chứa base address
    uint32_t offset
){
    return write_mem(proc->regs[destination] + offset, proc, data);
}

/*
 * run(): thực thi 1 lệnh của tiến trình.
 *
 * - Lấy instruction tại PC (program counter)
 * - Tăng PC
 * - Xử lý lệnh dựa vào opcode
 * - Trả về 0 nếu thành công, 1 nếu lỗi
 */
int run(struct pcb_t *proc)
{
    // nếu PC vượt quá số lệnh → process kết thúc
    if (proc->pc >= proc->code->size)
    {
        return 1;
    }

    struct inst_t ins = proc->code->text[proc->pc];
    proc->pc++;    // move PC to next instruction

    int stat = 1;
    
    switch (ins.opcode)
    {
    case CALC:
        stat = calc(proc);
        break;

    case ALLOC:
#ifdef MM_PAGING
        stat = liballoc(proc, ins.arg_0, ins.arg_1);
#else
        stat = alloc(proc, ins.arg_0, ins.arg_1);
#endif
        break;

    case FREE:
#ifdef MM_PAGING
        stat = libfree(proc, ins.arg_0);
#else
        stat = free_data(proc, ins.arg_0);
#endif
        break;

    case READ:
#ifdef MM_PAGING
        stat = libread(proc, ins.arg_0, ins.arg_1, (uint32_t*)&ins.arg_2);
#else
        stat = read(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
        break;

    case WRITE:
#ifdef MM_PAGING
        stat = libwrite(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#else
        stat = write(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
        break;

    case SYSCALL:
        stat = libsyscall(proc, ins.arg_0, ins.arg_1, ins.arg_2, ins.arg_3);
        break;

    default:
        stat = 1;
    }

    return stat;
}
