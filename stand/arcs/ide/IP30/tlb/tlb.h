#ifndef __TLB_H__
#define	__TLB_H__

#ident	"IP30/tlb/tlb.h:  $Revision: 1.13 $"

#include "uif.h"

#define PAGESIZE	(1 << TLBHI_VPNSHIFT)

/*
 * Primitive TLB operations: get and set key/value, lookup, and invalidate.
 *	unsigned int	index;
 *	unsigned int	pid, vpn;
 *	unsigned int	key;
 *	unsigned int	value;
 * NB: tlbmatch() returns -1 if the key formed from pid and vpn isn't found.
 */

extern machreg_t	get_tlblo0(int);
#if R4000
extern machreg_t	get_tlblo1(int);
#endif
extern machreg_t	get_tlbpid(void);
extern void		set_tlbpid(unsigned char);
extern long		probe_tlb(char *, unsigned int);

extern void tlbpurge(void);
extern void invaltlb(int);

/*
 * Test functions.  They share a common interface consisting of the following
 * arguments (some are unused by certain tests) and a return value
 * which signals success (0) or failure (!0).
 *	__psunsigned_t	pt[NTLBENTRIES];	// test page table
 *	unsigned int	*uncachedp;		// K1seg ptr to mapped mem
 *	unsigned int	*cachedp;		// K0seg ptr to mapped mem
 */
int	tlbglobal(__psunsigned_t *, u_int *, u_int *);
int	tlbmod(__psunsigned_t *, u_int *, u_int *);
int	tlbnocache(__psunsigned_t *, u_int *, u_int *);
int	tlbvalid(__psunsigned_t *, u_int *, u_int *);
int	tlbmem(__psunsigned_t *, u_int *, u_int *);
int	tlbprobe(__psunsigned_t *, u_int *, u_int *);
int	tlbpid(__psunsigned_t *, u_int *, u_int *);
int	tlbtranslate(__psunsigned_t *, u_int *, u_int *);

#endif	/* __TLB_H__ */
