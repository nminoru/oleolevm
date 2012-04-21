#include <linux/mm.h>

#include <linux/oleole.h>
#include <linux/oleoletlb.h>

#include <asm/tlb.h>

#include "oleole_internal.h"
#include "oleole_pgtable.h"


static void reactivate_pmd_table(pud_t *pud);
static void reactivate_pte_table(pmd_t *pmd);




/****************************************************************************/
/*                                                                          */
/****************************************************************************/

int oleole_get_gPTEInfo_offset(struct mm_struct *mm, pte_t **result, unsigned long address)
{
	pgd_t *pgd, pgd_v;
	pud_t *pud, pud_v;
	pmd_t *pmd, pmd_v;
	pte_t *pte;

	pgd   = pgd_offset(mm, address);
	pgd_v = *pgd;
	if (pgd_none(pgd_v))
		if (pud_alloc(mm, pgd, address) == NULL)
			return -ENOMEM;

	pud   = pud_offset(pgd, address);
	pud_v = *pud;

	if (oleole_pud_none(pud_v))
		if (oleole_pmd_alloc(pud))
			return -ENOMEM;

	pmd   = pmd_offset(pud, address);
	pmd_v = *pmd;

	if (oleole_pmd_none(pmd_v))
		if (oleole_pte_alloc(pmd))
			return -ENOMEM;

	pte  = pte_offset_map(pmd, address);

	*result = pte;

	return 0;
}


int oleole_get_gPTE_offset_without_alloc(struct mm_struct *mm, pte_t **result, unsigned long address)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd   = pgd_offset(mm, address);
	if (unlikely(pgd_none(*pgd)))
		return -1;

	pud = pud_offset(pgd, address);

	if (unlikely(oleole_pud_none(*pud)))
		return -1;

	pmd = oleole_pmd_offset(pud, address);

	if (unlikely(oleole_pmd_none(*pmd)))
		return -1;

	pte  = pte_offset_map(pmd, address);

	*result = pte;

	return 0;
}


int oleole_get_gPTE_offset_with_alloc(struct mm_struct *mm, pte_t **result, unsigned long address)
{
	pgd_t *pgd, pgd_v;
	pud_t *pud, pud_v;
	pmd_t *pmd, pmd_v;
	pte_t *pte;

	pgd   = pgd_offset(mm, address);
	pgd_v = *pgd;
	if (pgd_none(pgd_v))
		if (pud_alloc(mm, pgd, address) == NULL)
			return -ENOMEM;

	pud   = pud_offset(pgd, address);
	pud_v = *pud;

	if (oleole_pud_none(pud_v))
		if (oleole_pmd_alloc(pud))
			return -ENOMEM;

	if (unlikely((pud_val(pud_v) & _PAGE_DEACTIVATED)))
		reactivate_pmd_table(pud);

	pmd   = pmd_offset(pud, address);
	pmd_v = *pmd;

	if (oleole_pmd_none(pmd_v))
		if (oleole_pte_alloc(pmd))
			return -ENOMEM;

	if (unlikely((pmd_val(pmd_v) & _PAGE_DEACTIVATED)))
		reactivate_pte_table(pmd);

	pte  = pte_offset_map(pmd, address);

	*result = pte;

	return 0;
}


static void reactivate_pmd_table(pud_t *pud)
{
	int i;
	pmd_t *pmd;
	unsigned long val;

	val  = pud_val(*pud);

	if (likely(val & ~(_PAGE_DEACTIVATED|_PAGE_PRESENT)))
		val &= ~_PAGE_DEACTIVATED;
	else
		val &= ~(_PAGE_DEACTIVATED|_PAGE_PRESENT);

	*pud = __pud(val);

	pmd = pmd_offset(pud, 0);
	for (i=0 ; i<PTRS_PER_PMD ; i++, pmd++) {
		val  = pmd_val(*pmd);
		val |= (_PAGE_DEACTIVATED|_PAGE_PRESENT);
		*pmd = __pmd(val);
	}
}


static void reactivate_pte_table(pmd_t *pmd)
{
	int i;
	pte_t *pte;
	unsigned long val;

	val  = pmd_val(*pmd);

	if (likely(val & ~(_PAGE_DEACTIVATED|_PAGE_PRESENT)))
		val &= ~_PAGE_DEACTIVATED;
	else
		val &= ~(_PAGE_DEACTIVATED|_PAGE_PRESENT);

	*pmd = __pmd(val);
	
	/* PTEs */
	pte = pte_offset_map(pmd, 0);
	for (i=0 ; i<PTRS_PER_PTE ; i++, pte++) {
		val  = pte_val(*pte);
		val |= (_PAGE_DEACTIVATED|_PAGE_PRESENT);
		*pte = __pte(val);
	}
}


/****************************************************************************/
/* Deallocate Shadow Page Table                                             */
/****************************************************************************/
static void free_pmd_range(pud_t *pud)
{
	int i;
	pmd_t *pmd;
	pmd = pmd_offset(pud, 0);

	for (i=0 ; i<PTRS_PER_PMD ; i++, pmd++) {
		struct page *page;

		if (oleole_pmd_none_or_clear_bad(pmd))
			continue;

		page = pmd_page(*pmd);
		__free_page(page);
		pmd_clear(pmd);		
	}
}


static void free_pud_range(pgd_t *pgd)
{
	int i;
	pud_t *pud;
	pud = pud_offset(pgd, 0);

	for (i=0 ; i<PTRS_PER_PUD ; i++, pud++) {
		pmd_t *pmd;
		struct page *page;

		if (oleole_pud_none_or_clear_bad(pud))
			continue;

		free_pmd_range(pud);

		pmd = pmd_offset(pud, 0);
		page = virt_to_page(pmd);
		__free_page(page);
		pud_clear(pud);
	}
}


void oleolevm_free_pgd_range(struct mmu_gather *tlb,
			     struct vm_area_struct *vma,
			     unsigned long addr, unsigned long end,
			     unsigned long floor,
			      unsigned long ceiling)
{
	unsigned long flags;
	oleole_guest_system_t *gsys;

	addr &= PMD_MASK;

	for ( ; addr < end ; addr += PMD_SIZE) {
		pgd_t *pgd;
		pud_t *pud;
		struct page *page;

		pgd = pgd_offset(tlb->mm, addr);

		if (oleole_pgd_none_or_clear_bad(pgd))
			continue;

		free_pud_range(pgd);

		pud = pud_offset(pgd, 0);
		page = virt_to_page(pud);
		__free_page(page);
		pgd_clear(pgd);
	}

	/* */
	gsys = vma->vm_private_data;
	if (gsys) {
		spin_lock_irqsave(&gsys->lock, flags);
		vma->vm_private_data = NULL;
		gsys->vma = NULL;
		spin_unlock_irqrestore(&gsys->lock, flags);
	}

	tlb->need_flush = 1;
}


/****************************************************************************/
/*                                                                          */
/****************************************************************************/

int oleole_flush_guest_virt_memory(oleole_guest_system_t *gsys)
{
	unsigned long flags;
	unsigned long start, end;
	struct vm_area_struct	*vma;
	struct mm_struct *mm;
	pgd_t *pgd;

	spin_lock_irqsave(&gsys->lock, flags);
	vma = gsys->vma;
	spin_unlock_irqrestore(&gsys->lock, flags);

	if (!vma)
		return -1;

	mm = vma->vm_mm;

	if (!mm)
		return -1;

	start = vma->vm_start + OLEOLE_GUSET_VIRT_SPACE_OFFSET;
	end   = start         + 0x100000000UL;

	down_write(&mm->mmap_sem);

	pgd = pgd_offset(mm, start);
	if (!pgd_present(*pgd))
		goto miss;

	for (; start < end ; start += PUD_SIZE) {
		pud_t *pud;
		pmd_t *pmd;
		struct page *page;

		pud = pud_offset(pgd, start);
		if (!pud_present(*pud))
			goto miss;

		free_pmd_range(pud);

		pmd = pmd_offset(pud, 0);
		page = virt_to_page(pmd);
		__free_page(page);
		pud_clear(pud);
	}
miss:

	up_write(&mm->mmap_sem);

	__flush_tlb();

	return 0;
}


/****************************************************************************/
/*                                                                          */
/****************************************************************************/
int oleole_read_guest_phy_word(oleole_guest_system_t *gsys, uint32_t addr, uint32_t *result)
{
	int page_index;
	unsigned long flags;
	struct page *page;
	uint8_t *p;

	if (gsys->guest_phy_mem_size < addr + sizeof(uint32_t))
		return -1;

	page_index = addr >> PAGE_SHIFT;

	spin_lock_irqsave(&guest_phy_page_table[page_index].lock, flags);
	page = guest_phy_page_table[page_index].page;
	spin_unlock_irqrestore(&guest_phy_page_table[page_index].lock, flags);

	if (!page)
		return -1;
	
	p = (uint8_t *)page_address(page);

	*result = *(uint32_t*)(p + (addr & ~PAGE_MASK));

	return 0;
}
