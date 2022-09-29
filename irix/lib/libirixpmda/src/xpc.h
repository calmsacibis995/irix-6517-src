/*
 * High precision counter support
 *
 * $Id: xpc.h,v 1.6 1997/01/27 06:27:02 chatz Exp $
 */

extern __uint64_t *XPCincr(pmID, int, __uint32_t);
#ifdef PCP_DEBUG
extern void XPCdump(void);
#endif
