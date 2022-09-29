#ident "@(#)debug.h	1.14	11/13/92"
/*
 *  SpiderTCP Network Daemon
 *
 *      Debugging definitions
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.debug.h
 *	@(#)debug.h	1.14
 *
 *	Last delta created	17:06:03 2/4/91
 *	This file extracted	14:52:23 11/13/92
 *
 */

#ifdef DEBUG
extern void db_print();

#define ASSERT(x)	{ if (!(x)) \
			  db_print("** False Assertion (%s, line %d) **\n", \
			  __FILE__, __LINE__); \
			}
#define PRINT(m)			db_print((m))
#define PRINT1(m,a)			db_print((m), (a))
#define PRINT2(m,a,b)			db_print((m), (a), (b))
#define PRINT3(m,a,b,c)			db_print((m), (a), (b), (c))
#define PRINT4(m,a,b,c,d)		db_print((m), (a), (b), (c), (d))
#define PRINT5(m,a,b,c,d,e)		db_print((m), (a), (b), (c), (d), (e))
#define PRINT6(m,a,b,c,d,e,f)		db_print((m), (a), (b), (c), (d), (e), (f))
#define PRINT7(m,a,b,c,d,e,f,g)		db_print((m), (a), (b), (c), (d), (e), (f), (g))
#define PRINT8(m,a,b,c,d,e,f,g,h)	db_print((m), (a), (b), (c), (d), (e), (f), (g), (h))
#define PRINT9(m,a,b,c,d,e,f,g,h,i)	db_print((m), (a), (b), (c), (d), (e), (f), (g), (h), (i))
#define ERROR(m)			error((m))
#define ERROR1(m,a)			error((m), (a))
#define ERROR2(m,a,b)			error((m), (a), (b))
#define ERROR3(m,a,b,c)			error((m), (a), (b), (c))
#define ERROR4(m,a,b,c,d)		error((m), (a), (b), (c), (d))
#define ERROR5(m,a,b,c,d,e)		error((m), (a), (b), (c), (d), (e))
#define ERROR6(m,a,b,c,d,e,f)		error((m), (a), (b), (c), (d), (e), (f))
#define ERROR7(m,a,b,c,d,e,f,g)		error((m), (a), (b), (c), (d), (e), (f), (g))
#define ERROR8(m,a,b,c,d,e,f,g,h)	error((m), (a), (b), (c), (d), (e), (f), (g), (h))
#define ERROR9(m,a,b,c,d,e,f,g,h,i)	error((m), (a), (b), (c), (d), (e), (f), (g), (h), (i))
#else /* ~DEBUG */
#define ASSERT(x)
#define PRINT(m)
#define PRINT1(m, a)
#define PRINT2(m, a, b)
#define PRINT3(m, a, b, c)
#define PRINT4(m, a, b, c, d)
#define PRINT5(m, a, b, c, d, e)
#define PRINT6(m, a, b, c, d, e, f)
#define PRINT7(m, a, b, c, d, e, f, g)
#define PRINT8(m, a, b, c, d, e, f, g, h)
#define PRINT9(m, a, b, c, d, e, f, g, h, i)
#define ERROR(m)
#define ERROR1(m, a)
#define ERROR2(m, a, b)
#define ERROR3(m, a, b, c)
#define ERROR4(m, a, b, c, d)
#define ERROR5(m, a, b, c, d, e)
#define ERROR6(m, a, b, c, d, e, f)
#define ERROR7(m, a, b, c, d, e, f, g)
#define ERROR8(m, a, b, c, d, e, f, g, h)
#define ERROR9(m, a, b, c, d, e, f, g, h, i)
#endif /* DEBUG */

/* Prepending X to debug statement turns it off... */

#define XASSERT(x)
#define XPRINT(m)
#define XPRINT1(m, a)
#define XPRINT2(m, a, b)
#define XPRINT3(m, a, b, c)
#define XPRINT4(m, a, b, c, d)
#define XPRINT5(m, a, b, c, d, e)
#define XPRINT6(m, a, b, c, d, e, f)
#define XPRINT7(m, a, b, c, d, e, f, g)
#define XPRINT8(m, a, b, c, d, e, f, g, h)
#define XPRINT9(m, a, b, c, d, e, f, g, h, i)
#define XERROR(m)
#define XERROR1(m, a)
#define XERROR2(m, a, b)
#define XERROR3(m, a, b, c)
#define XERROR4(m, a, b, c, d)
#define XERROR5(m, a, b, c, d, e)
#define XERROR6(m, a, b, c, d, e, f)
#define XERROR7(m, a, b, c, d, e, f, g)
#define XERROR8(m, a, b, c, d, e, f, g, h)
#define XERROR9(m, a, b, c, d, e, f, g, h, i)

#ifdef DEBUG
extern int	debug;
#endif /* DEBUG */
