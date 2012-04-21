#ifndef _LINUX_OLEOLE_H
#define _LINUX_OLEOLE_H

#include <linux/mm_types.h>

#define OLEOLE_GUEST_PHY_MEMORY_PAGES (1048576UL)
#define OLEOLE_GUEST_PHY_PAGE_SIZE    (4096)

#define OLEOLE_GUSET_PHY_SPACE_OFFSET  (0UL)
#define OLEOLE_GUSET_VIRT_SPACE_OFFSET (0x200000000UL)

#define OLEOLE_PTE_PRESENT_BIT (0)
#define OLEOLE_PTE_WP_BIT      (1)

#define OLEOLE_PTE_PRESENT     (1U << OLEOLE_PTE_PRESENT_BIT)
#define OLEOLE_PTE_WP          (1U << OLEOLE_PTE_WP_BIT)

#endif /* _LINUX_OLEOLE_H */
