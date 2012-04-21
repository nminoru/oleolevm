#ifndef _LINUX_OLEOLE_IOCTL_H
#define _LINUX_OLEOLE_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define OLEOLE_IOC_MAGIC 'o'

#define OLEOLE_IOC_START		_IOW(OLEOLE_IOC_MAGIC, 1, __u64)
#define OLEOLE_IOC_SETCR3		_IOW(OLEOLE_IOC_MAGIC, 2, __u32)

#endif /* _LINUX_OLEOLE_IOCTL_H */

