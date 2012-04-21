#ifndef _ARCH_X86_OLEOLE_OLEOLE_PGTABLE_H
#define _ARCH_X86_OLEOLE_OLEOLE_PGTABLE_H

#include <linux/mm.h>		/* for struct page */

#include <asm/pgtable.h>
#include <asm/pgtable_types.h>


#define _PAGE_BIT_DEACTIVATED (50)
#define _PAGE_DEACTIVATED     (_AC(1,UL) << _PAGE_BIT_DEACTIVATED)

#define _KERNPG_TABLE	(_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED |	\
			 _PAGE_DIRTY)


static inline void oleole_pgd_populate(pgd_t *pgd, pud_t *pud)
{
	*pgd = __pgd(_PAGE_TABLE | __pa(pud));
}

static inline void oleole_pud_populate(pud_t *pud, pmd_t *pmd)
{
	*pud = __pud(_PAGE_TABLE | __pa(pmd));
}

static inline void oleole_pmd_populate(pmd_t *pmd, pte_t *pte)
{
	*pmd = __pmd(_PAGE_TABLE | __pa(pte));
}


/**/
static inline int oleole_pgd_bad(pgd_t pgd)
{
	pgd_t temp = pgd;
	temp.pgd &= ~_PAGE_DEACTIVATED;
	return pgd_bad(temp);
}

static inline int oleole_pud_bad(pud_t pud)
{
	pud_t temp = pud;
	temp.pud &= ~_PAGE_DEACTIVATED;
	return pud_bad(temp);
}

static inline int oleole_pmd_bad(pmd_t pmd)
{
	pmd_t temp = pmd;
	temp.pmd &= ~_PAGE_DEACTIVATED;
	return pmd_bad(temp);
}


/**/
static inline int oleole_pgd_none(pgd_t pgd)
{
	pgd_t temp = pgd;
	temp.pgd &= ~_PAGE_DEACTIVATED;
	return pgd_none(temp);
}

static inline int oleole_pud_none(pud_t pud)
{
	pud_t temp = pud;
	temp.pud &= ~_PAGE_DEACTIVATED;
	return pud_none(temp);
}

static inline int oleole_pmd_none(pmd_t pmd)
{
	pmd_t temp = pmd;
	temp.pmd &= ~_PAGE_DEACTIVATED;
	return pmd_none(temp);
}

static inline int oleole_pte_none(pte_t pte)
{
	pte_t temp = pte;
	temp.pte &= ~_PAGE_DEACTIVATED;
	return pte_none(temp);
}


#define oleole_pmd_offset(dir, address) \
	((pmd_t*)((unsigned long) __va(pud_val(*(dir)) & PHYSICAL_PAGE_MASK & ~_PAGE_DEACTIVATED)) + pmd_index(address))

#define oleole_pte_offset(dir, address) \
	((pte_t*)((unsigned long) __va(pmd_val(*(dir)) & PTE_MASK & ~_PAGE_DEACTIVATED)) + pte_index(address))


static inline int oleole_pgd_none_or_clear_bad(pgd_t *pgd)
{
	if (oleole_pgd_none(*pgd))
		return 1;
	if (unlikely(oleole_pgd_bad(*pgd))) {
		pgd_clear_bad(pgd);
		return 1;
	}
	return 0;
}


static inline int oleole_pud_none_or_clear_bad(pud_t *pud)
{
	if (oleole_pud_none(*pud))
		return 1;
	if (unlikely(oleole_pud_bad(*pud))) {
		pud_clear_bad(pud);
		return 1;
	}
	return 0;
}


static inline int oleole_pmd_none_or_clear_bad(pmd_t *pmd)
{
	if (oleole_pmd_none(*pmd))
		return 1;
	if (unlikely(oleole_pmd_bad(*pmd))) {
		pmd_clear_bad(pmd);
		return 1;
	}
	return 0;
}


static inline int oleole_pud_alloc(pgd_t *pgd)
{
	struct page *page;
	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (unlikely(page == NULL)) {
		return -ENOMEM;
	}
	oleole_pgd_populate(pgd, (pud_t*)page_address(page));
	return 0;
}


static inline int oleole_pmd_alloc(pud_t *pud)
{
	struct page *page;
	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (unlikely(page == NULL)) {
		return -ENOMEM;
	}
	oleole_pud_populate(pud, (pmd_t*)page_address(page));
	return 0;
}


static inline int oleole_pte_alloc(pmd_t *pmd)
{
	struct page *page;
	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (unlikely(page == NULL)) {
		return -ENOMEM;
	}
	oleole_pmd_populate(pmd, (pte_t*)page_address(page));
	return 0;
}


#endif  /* _ARCH_X86_OLEOLE_OLEOLE_PGTABLE_H */
