#include "sgidefs.h"


/*
 * cxfs stubs
 */
void	cxfs_init(){}

void	*cxfs_dcxvn_make() { return((void *)0); }
void    cxfs_dcxvn_obtain() {}

/*
 * cxfs_dcxvn_recall should really never be called. But, let's be nice
 * and return the tkset passed in. But, we can't include tkm.h because
 * non-CELL can't include tkm.h so let's copy the typedefs here.
 */

typedef __uint32_t tk_set_t;
typedef __uint32_t tk_disp_t;

/* ARGSUSED */
tk_set_t cxfs_dcxvn_recall(void *dp, tk_set_t tk, tk_disp_t td) { return(tk); }

void    cxfs_dcxvn_return() {}
void 	cxfs_dcxvn_destroy() {}

void	*cxfs_dsxvn_make() { return((void *)0); }
int	cxfs_dsxvn_obtain() { return(0); }
void	cxfs_dsxvn_obtain_done() {}
void	cxfs_dsxvn_return() {}
void	cxfs_dsxvn_destroy() {}
