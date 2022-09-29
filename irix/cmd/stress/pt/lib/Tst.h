#ifndef __TST_H__
#define __TST_H__


/* Test Harness Primitives
 */

/* Pull in the checking and benchmark macros.
 * Override output formats with T versions.
 */
#define ChkPrError	TstError
#define ChkPrInfo	TstInfo
#include <Chk.h>
#define BnchPrInfo	TstInfo
#include <Bnch.h>

/*
 * TST_PROTO(testf);
 * TST_PROTO(...);
 *
 * Tst_t TstTable[] = {
 *		{ "test-list-id", 0,
 *			"Purpose of this test table",
 *			"Details and scope." },
 *		{ "testid", testf,
 *			"Thsi test entry",
 *			"Implementation scheme." },
 *		...
 *
 *		{ 0 }
 * };
 */

typedef struct Tst {
	char	*t_name;		/* test id */
	int	(*t_func)(void);	/* entry pt, return 0 for success */
	char	*t_synopsis;		/* short description */
	char	*t_details;		/* method, notes etc. */
	/* per-test volatile data */
} Tst_t;


/* User test table.
 */
extern Tst_t	TstTable[];

#define TST_START(syn) 		Tst_t TstTable[] = { { __FILE__, 0, syn, 0 },
#define TST(nm, syn, det)	{ #nm, nm, syn, det }
#define TST_FINISH		{ 0 } };

#define TST_BEGIN(t)	int t()
#define TST_RETURN(r)	return (r)


/* Test output control
 *
 * Default is terse.
 * Output goes to fd 2.
 */
#define TST_INFO	0x01	/* 'I' */
#define TST_ADMIN	0x02	/* 'A' */
#define TST_TRACE	0x04	/* 'T' */

extern int TstOutput;

extern void	TstSetOutput(char *);
extern void	TstExit(const char *, ...);
extern void	TstError(const char *, ...);
extern void	TstInfo(const char *, ...);
extern void	TstAdmin(const char *, ...);
extern void	TstTrace(const char *, ...);

#define TST_PAUSE	50
#define TST_SNOOZE	100
#define TST_DOZE	200

#define TST_NAP(dly)	sginap(dly);
#define TST_SPIN(dly)	{ volatile int i = dly * 10000; while (i--) ; }

#define TST_TRIAL_THREADS	10
#define TST_TRIAL_LOOP		50

#undef TRUE
#define TRUE	1
#undef FALSE
#define FALSE	0

#endif	/* __TST_H__ */

/*
	Fix the TST_TRIAL counts to be small, medium and large.
	Rename/ cleanup the timeout/delay calls.
 */
