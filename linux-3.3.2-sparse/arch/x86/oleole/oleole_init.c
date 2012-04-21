#include <linux/init.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/oleole.h>
#include <linux/oleoletlb.h>
#include <linux/oleole_ioctl.h>

#include "oleole_internal.h"


oleole_guest_phy_page_t *guest_phy_page_table;


oleole_guest_system_t *oleole_guest_system_alloc(void)
{
	oleole_guest_system_t *gsys;

	gsys = kzalloc(sizeof(oleole_guest_system_t), GFP_KERNEL);
	spin_lock_init(&gsys->lock);

	return gsys;
}


void oleole_guest_system_dealloc(oleole_guest_system_t *gsys)
{
	kfree(gsys);
}


unsigned long oleole_map_guest_phy_memory(unsigned long old_size, unsigned long new_size)
{
	unsigned long ret;
	unsigned long s;

	if (old_size == new_size)
		return new_size;

	if (new_size < old_size)
		goto shrink;

	for (s = old_size ; s < new_size ; s += PAGE_SIZE) {
		int index;
		unsigned long flags;
		struct page *page;
		void* p;

		page = alloc_page(GFP_KERNEL);
		if (!page)
			break;

		p = page_address(page);
		memset(p, 0, PAGE_SIZE);

		index = s / PAGE_SIZE;

		spin_lock_irqsave(&guest_phy_page_table[index].lock, flags);
		guest_phy_page_table[index].page = page;
		spin_unlock_irqrestore(&guest_phy_page_table[index].lock, flags);

		ret = s + PAGE_SIZE;
	}

	return ret;

shrink:
	for (s = new_size ; s < old_size ; s += PAGE_SIZE) {
		int index;
		unsigned long flags;
		struct page *page;

		index = s / PAGE_SIZE;

		spin_lock_irqsave(&guest_phy_page_table[index].lock, flags);
		page = guest_phy_page_table[index].page;
		guest_phy_page_table[index].page = NULL;
		spin_unlock_irqrestore(&guest_phy_page_table[index].lock, flags);

		if (page)
			__free_page(page);
	}
	
	return new_size;
}


static int construct_system(void)
{
	int i;
	const unsigned long size = sizeof(oleole_guest_phy_page_t) * OLEOLE_GUEST_PHY_MEMORY_PAGES;

	guest_phy_page_table = vmalloc(size);
	if (!guest_phy_page_table) {
		printk("oleole: Can't allocate guset_phy_page_table.\n");
		return -ENOMEM;		
	}

	memset(guest_phy_page_table, 0, size);

	for (i=0 ; i<OLEOLE_GUEST_PHY_MEMORY_PAGES ; i++)
		spin_lock_init(&guest_phy_page_table[i].lock);

	return 0;
}


static int __init oleole_init(void)
{
	int ret;

	printk("oleole virtual memory installed\n");

	ret = oleole_create_procfile();
	if (ret < 0)
		return 0;

	ret = construct_system();
	if (ret < 0)
		return 0;

	return 0;
}
__initcall(oleole_init);
