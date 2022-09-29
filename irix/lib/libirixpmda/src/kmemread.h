/*
 * $Id: kmemread.h,v 1.10 1999/04/28 07:02:32 tes Exp $
 */
#include <sgidefs.h>
#include <nlist.h>


/*
 * Warning:	values here have to match order of kernsymb[] initializers in
 *		kmemread.c
 */
#define KS_KERNEL_MAGIC	0
#define KS_END		1
#define KS_IFNET	2
#define KS_DKIPPROBED	3
#define KS_DKIOTIME	4
#define KS_DKSCINFO	5
#define KS_USRAID_SOFTC	6
#define KS_USRAID_IOTIME	7
#define KS_USRAID_INFO	8
#define KS_VN_VNUMBER	9
#define KS_VN_EPOCH	10
#define KS_VN_NFREE	11
#define KS_STR_CURPAGES 12
#define KS_STR_MINPAGES 13
#define KS_STR_MAXPAGES 14

#if _MIPS_SZPTR == 64
#define NLIST	nlist64
#define LSEEK	lseek64
#define KMEM_ADDR_MASK	0x8000000000000000
#elif _MIPS_SZPTR == 32
#define NLIST	nlist
#define LSEEK	lseek
#define KMEM_ADDR_MASK	0x80000000
#else
bozo!  I need to know how to handle your size of kernel addresses
#endif

extern struct NLIST kernsymb[];

extern void kmeminit(void);
extern int kmemread(__psunsigned_t, void *, int);
extern int unaligned_kmemread(__psunsigned_t, void *, int);

#define VALID_KMEM_ADDR(addr)	((addr & KMEM_ADDR_MASK) == KMEM_ADDR_MASK)
#define ALIGNED_KMEM_ADDR(addr)	((addr & 0x3) == 0)
