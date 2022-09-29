/**************************************************************************
 *									  *
 * 		 Copyright (C) 1987-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_MMAN_H__
#define __SYS_MMAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.48 $"

#include <standards.h>
/*	mmap flags and protections
*/
#define	MAP_SHARED	0x1	/* share changes */
#define	MAP_PRIVATE	0x2	/* changes are private */
#define	MAP_TYPE	0xf	/* mask for mapping type */

#define	MAP_FIXED	0x010	/* interpret addr exactly */
#define MAP_RENAME	0x020	/* assign page to file */
#define MAP_AUTOGROW	0x040	/* file grows with store access */
#define	MAP_LOCAL	0x080	/* separate copies made on fork/sproc */
#define MAP_AUTORESRV	0x100	/* logical swap reserved on demand */
#define MAP_TEXT	0x200	/* chg SHARED -> PRIVATE on write */
#define MAP_BRK		0x400	/* potentially make the area following this
				 * mapping the heap (brk(2))
				 */
#define MAP_PRIMARY	0x800	/* this mapping should be considered the
				 * 'primary' mapping
				 */

#define PROT_NONE	0x0	/* page can not be accessed */
#define PROT_READ	0x1	/* page can be read */
#define PROT_WRITE	0x2	/* page can be written */
#define PROT_EXECUTE	0x4	/* page can be executed */
#define PROT_EXEC	0x4	/* SVR4 SVID name for PROT_EXECUTE */
#define PROT_EXEC_NOFLUSH 0x8	/* see mprotect(2) */

#if _SGIAPI
/* these flags are used by memcntl */
#define PROC_TEXT	(PROT_EXEC | PROT_READ)
#define PROC_DATA	(PROT_READ | PROT_WRITE)
#endif	/* _SGIAPI */

/* The PRIVATE define clashes with a define in sys/dsreq.h.
 * So for the kernel, provide a different name.
 * memcntl is currently not implemented anyway.
 */
#ifndef _KERNEL

#if _SGIAPI
#define SHARED          0x10
#define PRIVATE         0x20
#endif	/* _SGIAPI */

#else	/* defined _KERNEL follows */

#define MEMCNTL_SHARED          0x10
#define MEMCNTL_PRIVATE         0x20

#endif /* _KERNEL */

#define MAP_FAILED	((void *)-1L)

#ifdef _KERNEL
#define	PROT_ALL	(PROT_READ | PROT_WRITE | PROT_EXEC)
#endif /* _KERNEL */

/*	msync flags
*/
#define	MS_ASYNC	0x1	/* return immediately */
#define	MS_INVALIDATE	0x2	/* invalidate mappings */
#define	MS_SYNC		0x4	/* wait for msync */

#if _POSIX93 || _SGIAPI || _XOPEN5
/* flags to mlockall */
#define MCL_CURRENT     0x1             /* lock current mappings */
#define MCL_FUTURE      0x2             /* lock future mappings */
#endif

#if _SGIAPI
/*	advice to madvise
*/
#define	MADV_NORMAL	0		/* no further special treatment */
#define	MADV_RANDOM	1		/* expect random page references */
#define	MADV_SEQUENTIAL	2		/* expect sequential page references */
#define	MADV_WILLNEED	3		/* will need these pages */
#define	MADV_DONTNEED	4		/* don't need these pages */

/*
 * Synchronize the pages in an address range 
 * to follow the memory management policy 
 */
#define	MADV_SYNC_POLICY	7	

/* flags to memcntl */
#define MC_SYNC         1               /* sync with backing store */
#define MC_LOCK         2               /* lock pages in memory */
#define MC_UNLOCK       3               /* unlock pages from memory */
#define MC_ADVISE       4               /* give advice to management */
#define MC_LOCKAS       5               /* lock address space in memory */
#define MC_UNLOCKAS     6               /* unlock address space from memory */

#endif	/* _SGIAPI */

#ifndef _KERNEL
#include <sys/types.h>

#if _XOPEN4UX || _POSIX93 || _XOPEN5
extern void	*mmap(void *, size_t, int, int, int, off_t);
extern int	mprotect(void *, size_t, int);
extern int	msync(void *, size_t, int);
extern int	munmap(void *, size_t);
#endif /* _XOPEN4UX || _POSIX93 || _XOPEN5 */

#if _POSIX93 || _ABIAPI || _XOPEN5
extern int	mlockall(int);
extern int	munlockall(void);
extern int	mlock(const void *, size_t);
extern int	munlock(const void *, size_t);
extern int	shm_open(const char *, int, mode_t);
extern int	shm_unlink(const char *);
#endif /* _POSIX93 || _ABIAPI || _XOPEN5 */

#if _LFAPI
extern void	*mmap64(void *, size_t, int, int, int, off64_t);
#endif

#if _SGIAPI || _ABIAPI
extern int	madvise(void *, size_t, int);
extern int	memcntl(const void *, size_t, int, void *, int, int);
#endif	/* _SGIAPI || _ABIAPI */
#endif /* !_KERNEL */

#ifdef _KERNEL
#ifdef R10000_SPECULATION_WAR
extern void	*mpin_lockdown(caddr_t, size_t);
extern void	mpin_unlockdown(void *, caddr_t, size_t);
#endif	/* R10000_SPECULATION_WAR */

#define RTC_MMAP_ADDR	0xaddac000L
#endif

#ifdef __cplusplus
}
#endif

#endif /* !__SYS_MMAN_H__ */
