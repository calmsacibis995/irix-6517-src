#ifndef _SPYIO_H
#define _SPYIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "spyBase.h"
#include <sys/types.h>
#include <sys/ucontext.h>

extern int	ioXJBGregs(spyProc_t*, int, caddr_t, void*);
extern int	ioXJBFPregs(spyProc_t*, int, caddr_t, void*);

extern int	ioPlaceGregsJB(spyProc_t*, int, void*, caddr_t, off64_t);
extern int	ioPlaceFPregsJB(spyProc_t*, int, void*, caddr_t, off64_t);

extern void	ioXPointer(spyProc_t*, caddr_t, off64_t*);
extern void	ioXInt(spyProc_t*, caddr_t, uint_t*);
extern void	ioXShort(spyProc_t*, caddr_t, ushort_t*);

extern int	ioFetchPointer(spyProc_t*, off64_t, off64_t*);
extern int	ioFetchInt(spyProc_t*, off64_t, uint_t*);
extern int	ioFetchShort(spyProc_t*, off64_t, ushort_t*);
extern int	ioFetchBytes(spyProc_t*, off64_t, caddr_t, size_t);
extern int	iotFetchBytes(spyProc_t*, tid_t, off64_t, caddr_t, size_t);
extern int	iotFetchGregs(spyProc_t*, tid_t, caddr_t);
extern int	iotFetchFPregs(spyProc_t*, tid_t, caddr_t);

extern int	ioPlaceBytes(spyProc_t*, off64_t, caddr_t, size_t);

extern char*	ioToken(char*, char**);

extern int	ioListScan(spyProc_t*, off64_t,
			   int (*)(spyProc_t*, off64_t, void*), void*);

extern void	ioSetTrace(char*);
extern void	ioTrace(char*, ...);

/* Discover what type of abi we are.
 * This is useful when are asked to do a procfs command without
 * any modifiers, i.e. in the caller format.
 */
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
#	define IONATIVE	SP_O32
#elif (_MIPS_SIM == _MIPS_SIM_NABI32)
#	define IONATIVE	SP_N32
#elif (_MIPS_SIM == _MIPS_SIM_ABI64)
#	define IONATIVE	SP_N64
#endif

/* Strip off the modifiers */
#define IOPIOCCMD(op)	((op) & ~(PIOC_IRIX5_N32|PIOC_IRIX5_64))

/* Translate modifier into abi type */
#define IOPIOCFMT(op)	\
	(((op) & PIOC_IRIX5_N32) ? SP_N32		\
	 : (((op) & PIOC_IRIX5_64) ? SP_N64 		\
				   : IONATIVE))

#define IOPTRSIZE(p)	((p->sp_abi == SP_N64) ? 8 : 4)

#ifdef __cplusplus
}
#endif

#endif /* _SPYIO_H */
