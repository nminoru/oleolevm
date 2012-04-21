#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/signal.h>

#include <asm/tlbflush.h> /* for __flush_tlb() */

#include <linux/oleole.h>
#include <linux/oleoletlb.h>

#include "oleole_internal.h"
#include "oleole_pgtable.h"


enum x86_pf_error_code {
	PF_PROT		=		1 << 0,
	PF_WRITE	=		1 << 1,
	PF_USER		=		1 << 2,
	PF_RSVD		=		1 << 3,
	PF_INSTR	=		1 << 4,
};


typedef struct {
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	unsigned long address;
	unsigned long offset;
	unsigned int flags;
	struct pt_regs *regs;
	unsigned long error_code;
	struct task_struct *task;
} oleole_fault_t;


static int guest_dynamic_address_translation(oleole_guest_system_t *gsys, unsigned long offset, uint32_t *goffset, int *writeprot);
static void map_guest_page(oleole_guest_system_t *gsys, oleole_fault_t *fault, int virt);
static void throw_exception(struct task_struct *tsk, int signo, int code, unsigned long address, unsigned long error_code);


void
oleolevm_handle_mm_fault(struct mm_struct *mm, struct vm_area_struct *vma,
			 unsigned long address, unsigned int flags,
			 struct pt_regs *regs, unsigned long error_code)
{
	oleole_fault_t fault;
	oleole_guest_system_t *gsys;

	fault.mm         = mm;
	fault.vma        = vma;
	fault.address    = address;
	fault.flags      = flags;
	fault.regs       = regs;
	fault.error_code = error_code;
	fault.task       = current;

	__set_current_state(TASK_RUNNING);

	BUG_ON(!user_mode(regs));

	gsys = (oleole_guest_system_t *)vma->vm_private_data;

	fault.offset = address - vma->vm_start;

	switch (fault.offset >> 32) {
		
	case 0:
		/* guest phy memory */
		map_guest_page(gsys, &fault, 0);
		break;

	case 2:
		/* guest virtual memory */
		map_guest_page(gsys, &fault, 1);
		break;

	default:
		throw_exception(fault.task, SIGBUS, BUS_ADRERR, address, error_code);
		break;
	}
}


static int
guest_dynamic_address_translation(oleole_guest_system_t *gsys, unsigned long offset, uint32_t *goffset, int *writeprot)
{
	uint32_t sto, pto; /* segment-table origin, page-table origin */
	uint32_t sti, pti; /* segment-table index, page-table index */
	uint32_t ste, pte; /* segment-table entry, page-table entry */
	uint32_t ste_addr, pte_addr;

	sto = gsys->cr3;

	sti = ((offset >> 22) & 0x3FF);
	pti = ((offset >> 12) & 0x3FF);

	ste_addr = sto + sti * 4;

	if (oleole_read_guest_phy_word(gsys, ste_addr, &ste))
		return -1;

	if (!(ste & OLEOLE_PTE_PRESENT))
		return OLEOLE_PTE_PRESENT;

	pto = ste & 0xFFFFF000;

	pte_addr = pto + sti * 4;

	if (oleole_read_guest_phy_word(gsys, pte_addr, &pte))
		return -1;

	if (!(pte & OLEOLE_PTE_PRESENT))
		return OLEOLE_PTE_PRESENT;

	*writeprot = (pte & OLEOLE_PTE_WP);

	*goffset = pte & 0xFFFFF000;

	return 0;
}


static void
map_guest_page(oleole_guest_system_t *gsys, oleole_fault_t *fault, int virt)
{
	int ret, write, writeprot = 0;
	pte_t *pte;
	struct page *page;
	unsigned long index, offset, address;
	unsigned long flags, error_code;
	uint32_t goffset = 0;
	struct task_struct *task;

	write   = fault->error_code & PF_WRITE;
	error_code = fault-> error_code;

	address = fault->address;
	offset  = fault->offset;

	task    = fault->task;

	ret = oleole_get_gPTE_offset_with_alloc(fault->mm, &pte, address);
	if (unlikely(ret < 0))
		return;

	*pte = __pte(0);

	if (!virt)
		goto abs;

	ret = guest_dynamic_address_translation(gsys, offset, &goffset, &writeprot);
	if (ret == OLEOLE_PTE_PRESENT) {
		throw_exception(task, SIGSEGV, 0x101, address, error_code);
		return;
	} else if (ret < 0) {
		throw_exception(task, SIGBUS, 0x102, address, error_code);
		return;
	} else if (write && writeprot) {
		throw_exception(task, SIGSEGV, 0x103, address, error_code);
		return;
	}
		
	offset = goffset;

abs:

	if (gsys->guest_phy_mem_size <= offset) {
		throw_exception(fault->task, SIGSEGV, 0x103, address, fault->error_code);
		return;
	}

	index = offset / PAGE_SIZE;

	spin_lock_irqsave(&guest_phy_page_table[index].lock, flags);
	page = guest_phy_page_table[index].page;
	spin_unlock_irqrestore(&guest_phy_page_table[index].lock, flags);

	if (writeprot)
		*pte = mk_pte(page, __pgprot(_PAGE_TABLE & ~_PAGE_RW));
	else
		*pte = mk_pte(page, __pgprot(_PAGE_TABLE));

	__flush_tlb_one(address);

	return;
}


static void
throw_exception(struct task_struct *tsk, int signo, int code, unsigned long address, unsigned long error_code)
{
	siginfo_t info;

	tsk->thread.cr2 = address;
	tsk->thread.error_code = error_code | (address >= TASK_SIZE);
	tsk->thread.trap_no = 14;

	info.si_signo	= signo;
	info.si_errno	= 0;
	info.si_code	= code;
	info.si_addr	= (void __user *)address;

	force_sig_info(signo, &info, tsk);	
}
