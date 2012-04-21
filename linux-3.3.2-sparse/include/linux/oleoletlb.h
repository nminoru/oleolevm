#ifndef _LINUX_OLEOLETLB_H
#define _LINUX_OLEOLETLB_H

#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/fs.h>

/* 1TB */
#define OLEOLETLB_PAGE_SHIFT      (PGDIR_SHIFT)
#define OLEOLETLB_PAGE_SIZE       (1UL << OLEOLETLB_PAGE_SHIFT)
#define OLEOLETLB_PAGE_MASK       (~(OLEOLETLB_PAGE_SIZE - 1))

#ifdef CONFIG_OLEOLE

static inline int is_vm_oleoletlb_page(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & VM_OLEOLETLB);
}

extern void oleolevm_handle_mm_fault(struct mm_struct *mm, struct vm_area_struct *vma,
				     unsigned long address, unsigned int flags,
				     struct pt_regs *regs, unsigned long error_code);

extern void oleolevm_free_pgd_range(struct mmu_gather *tlb,
				    struct vm_area_struct *vma,
				    unsigned long addr, unsigned long end,
				    unsigned long floor,
				    unsigned long ceiling);

#else

static inline int is_vm_oleoletlb_page(struct vm_area_struct *vma)
{
	return 0;
}

static inline void
oleolevm_handle_mm_fault(struct mm_struct *mm, struct vm_area_struct *vma,
			 unsigned long address, unsigned int flags,
			 struct pt_regs *regs, unsigned long error_code)
{
}

static inline void
oleolevm_free_pgd_range(struct mmu_gather *tlb, struct vm_area_struct *vma,
			unsigned long addr, unsigned long end,
			unsigned long floor,
			unsigned long ceiling)
{
}

#endif

#endif /* _LINUX_OLEOLETLB_H */
