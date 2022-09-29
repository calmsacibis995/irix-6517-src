#ifndef __MEM_H__
#define	__MEM_H__

#ident	"IP22diags/mem/mem.h:  $Revision: 1.3 $"

/*
 * IDE memory tests header
 */

#include "uif.h"

#define FREE_MEM_LO	(K1_RAMBASE + 0x01400000) 
#define MAX_LOW_MEM             (256 * 1024 * 1024)
#define MAX_LOW_MEM_ADDR        (256 * 1024 * 1024)
/* #define FREE_MEM_LO	PROM_STACK	/*
/*
 * All memory tests have the following interface: the arguments are
 *	struct range	*ra;		// range of memory to test
 *	enum bitsense	sense;		// whether to invert bit-sense of test
 *	enum runflag	till;		// whether to continue after an error
 *	void		(*errfun)();	// error status notifier
 * Return code is TRUE for success, FALSE otherwise; the user is notified of
 * failure via (*errfun)(address, expected, actual, status) where the bad
 * address, expected value, actual value, and status code are passed.
 */

/* external entry points to memory tests */
extern bool_t	
	memaddruniq(struct range *, enum bitsense, enum runflag, void (*)());
extern bool_t  
	memwalkingbit(struct range *, enum bitsense, enum runflag, void (*)());
extern bool_t  
	memparity(struct range *, enum bitsense, enum runflag, void (*)());

/* parity checking utility functions */
void	pardisable(void);	/* disable parity checking */
void	parenable(void);	/* enable parity checking */
void	parclrerr(void);	/* clear outstanding parity error */
void	parwbad(void *, unsigned int, int);	/* write bad byte parity */

#endif	/* __MEM_H__ */
