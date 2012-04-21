#include <linux/init.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/oleole.h>
#include <linux/oleoletlb.h>
#include <linux/oleole_ioctl.h>

#include "oleole_internal.h"


static struct proc_dir_entry *oleolevm_proc_entry;



static int oleolevm_open(struct inode *inode, struct file *file)
{
	file->private_data = oleole_guest_system_alloc();

	return 0;
}


static int oleolevm_release(struct inode *inode, struct file *file)
{
	unsigned long flags;
	unsigned long size;
	oleole_guest_system_t *gsys;

	if (!file->private_data)
		goto end;

	gsys = (oleole_guest_system_t *)file->private_data;

	file->private_data = NULL;

	spin_lock_irqsave(&gsys->lock, flags);
	size = gsys->guest_phy_mem_size;
	gsys->guest_phy_mem_size = 0;
	spin_unlock_irqrestore(&gsys->lock, flags);
	
	oleole_map_guest_phy_memory(size, 0);

	oleole_guest_system_dealloc(gsys);

end:
	return 0;
}


static int oleolevm_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long flags;
	unsigned long vma_len;
	oleole_guest_system_t *vm_info;

	vm_info = file->private_data;

	if (!vm_info)
		return -ENODEV;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (vma->vm_start & ~OLEOLETLB_PAGE_MASK)
		return -EINVAL;

	vma_len = (unsigned long)(vma->vm_end - vma->vm_start);

	if (vma_len != OLEOLETLB_PAGE_SIZE)
		return -EINVAL;
	
	/*
	 *  - VM_DONTCOPY   Don't copy on fork
	 *  - VM_DONTEXPAND Can't expand with mremap.
	 *  - VM_RESERVED   Can't unmap
	 */
	vma->vm_flags |= VM_OLEOLETLB | VM_DONTCOPY | VM_DONTEXPAND| VM_RESERVED;

	spin_lock_irqsave(&vm_info->lock, flags);	
	vm_info->vma = vma;
	if (!vma->vm_private_data)
		vma->vm_private_data = vm_info;
	spin_unlock_irqrestore(&vm_info->lock, flags);

	return 0;
}


static long oleolevm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -EINVAL;
	oleole_guest_system_t *gsys;
	void __user *argp;
	
	argp = (void __user *)arg;
	gsys = (oleole_guest_system_t *)file->private_data;

	if (!gsys)
		return -ENODEV;

	switch(cmd) {

	case OLEOLE_IOC_START: {
		int mapping;
		unsigned long flags;
		unsigned long old_guest_phy_mem_size, cur_phy_mem_size;

		__u64 size = arg;

		if (size & ~PAGE_MASK)
			return -EINVAL; /* missaligment */

		if (OLEOLE_GUEST_PHY_MEMORY_PAGES < (size >> PAGE_SHIFT))
			return -EINVAL; /* too big */

		spin_lock_irqsave(&gsys->lock, flags);
		mapping = (gsys->vma != NULL);
		if (mapping)
			gsys->guest_phy_mem_size = size;
		spin_unlock_irqrestore(&gsys->lock, flags);

		if (mapping)
			return -EBUSY; 

		cur_phy_mem_size = oleole_map_guest_phy_memory(old_guest_phy_mem_size, size);

		spin_lock_irqsave(&gsys->lock, flags);
		gsys->guest_phy_mem_size = cur_phy_mem_size;
		spin_unlock_irqrestore(&gsys->lock, flags);

		if (cur_phy_mem_size != size)
			return -ENOMEM;

		return 0;
	}

	case OLEOLE_IOC_SETCR3: {
		__u32 cr3 = arg;
		unsigned long flags;

		if (cr3 & ~PAGE_MASK)
			return -EINVAL; /* missaligment */

		spin_lock_irqsave(&gsys->lock, flags);
		gsys->cr3 = cr3;
		spin_unlock_irqrestore(&gsys->lock, flags);

		oleole_flush_guest_virt_memory(gsys);

		return 0;
	}

	default:
		ret = -ENOTTY;

	}

	return ret;
}


static loff_t oleolevm_lseek(struct file *file, loff_t offset, int orig)
{
	loff_t retval = -EINVAL;

	switch (orig) {
	case SEEK_CUR: /* 1 */
		offset += file->f_pos;
		/* fallthrough */

	case SEEK_SET: /* 0 */
		if (offset < 0 || offset > MAX_NON_LFS)
			break;

		file->f_pos = retval = offset;
		/* fallthrough */
	}

	return retval;
}


static struct file_operations oleolevm_proc_ops = {
	.open           = oleolevm_open,
	.mmap           = oleolevm_mmap,
	.llseek         = oleolevm_lseek,
	.unlocked_ioctl = oleolevm_ioctl,
	.release        = oleolevm_release,
};


int oleole_create_procfile(void)
{
	struct proc_dir_entry *entry;

	entry = create_proc_entry("oleolevm", S_IWUGO|S_IRUGO, NULL);

	if (!entry) {
		printk("oleole: Can't create \"oleolevm\" proc file.\n");
		return -ENOMEM;
	}

	entry->proc_fops = &oleolevm_proc_ops;
	entry->data      = 0;

	oleolevm_proc_entry = entry;

	return 0;
}
