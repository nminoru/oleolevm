#ifndef _ARCH_X86_OLEOLE_OLEOLE_INTERNAL_H
#define _ARCH_X86_OLEOLE_OLEOLE_INTERNAL_H


/* 
 *  Oleole VM mapping image
 *
 *  PGD Table -> PUD Table -> PMD Table -> PTE Table
 *  (System)     (Ours)       (Ours)       (Ours)
 *    256TB      512GB         1GB           2MB
 *
 *  
 */


typedef struct {
	spinlock_t		lock;
	unsigned int		initilized;
	unsigned long		guest_phy_mem_size;
	uint32_t                cr3;
	struct vm_area_struct	*vma;
} oleole_guest_system_t;


typedef struct {
	spinlock_t		lock;
	struct page		*page;
} oleole_guest_phy_page_t;


extern oleole_guest_phy_page_t *guest_phy_page_table;


extern oleole_guest_system_t *oleole_guest_system_alloc(void);
extern void oleole_guest_system_dealloc(oleole_guest_system_t *gsys);

extern int oleole_create_procfile(void);

extern int oleole_get_gPTEInfo_offset(struct mm_struct *mm, pte_t **result, unsigned long address);
extern int oleole_get_gPTE_offset_without_alloc(struct mm_struct *mm, pte_t **result, unsigned long address);
extern int oleole_get_gPTE_offset_with_alloc(struct mm_struct *mm, pte_t **result, unsigned long address);
extern unsigned long oleole_map_guest_phy_memory(unsigned long old_size, unsigned long new_size);
extern int oleole_flush_guest_virt_memory(oleole_guest_system_t *gsys);
extern int oleole_read_guest_phy_word(oleole_guest_system_t *gsys,uint32_t addr, uint32_t *result);


#endif  /* _ARCH_X86_OLEOLE_OLEOLE_INTERNAL_H */
