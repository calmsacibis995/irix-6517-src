#ifndef __T6REFCNT_H__
#define __T6REFCNT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.1 $"

/*
 * Reference counts are kept as the lower 28 bits of a 32-bit word.
 * The upper 4 bits are made available for state information.
 *
 * Hash entries are kept in 3 states:
 *
 * INVALID
 *	REF_CNT_INVALID is set, other state bits ignored
 * WAIT
 *	REF_CNT_INVALID is not set, REF_CNT_WAIT is set
 * VALID
 *	REF_CNT_INVALID is not set, REF_CNT_WAIT is not set
 */
#define REF_CNT_INVALID		0x80000000
#define REF_CNT_WAIT		0x40000000
#define REF_CNT_CMASK		0x0fffffff	/* leave unused bits */
#define REF_CNT_SMASK		(REF_CNT_INVALID | REF_CNT_WAIT)

#define REF_CNT(x)		((x) & REF_CNT_CMASK)
#define REF_WAIT(x)		(((x) & REF_CNT_SMASK) == REF_CNT_WAIT)
#define REF_VALID(x)		(((x) & REF_CNT_SMASK) == 0)
#define REF_INVALID(x)		(((x) & REF_CNT_INVALID) != 0)

#ifdef __cplusplus
}
#endif

#endif /* __T6REFCNT_H__ */
