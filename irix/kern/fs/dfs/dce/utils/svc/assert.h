/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * The dce_assert() macro; like ANSI C's assert().
 */

/*
 * HISTORY
 * $Log: assert.h,v $
 * Revision 65.1  1997/10/20 19:15:06  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.6.2  1996/02/18  23:33:02  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:21:05  marty]
 *
 * Revision 1.1.6.1  1995/12/08  21:37:27  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/08  18:09:22  root]
 * 
 * Revision 1.1.4.2  1994/06/09  16:05:50  devsrc
 * 	cr10892 - fix copyright
 * 	[1994/06/09  15:50:26  devsrc]
 * 
 * Revision 1.1.4.1  1994/03/09  20:43:18  rsalz
 * 	Add DCE_SVC__FILE__ (defaults to __FILE__) and use it (OT CR 10107).
 * 	[1994/03/09  19:43:56  rsalz]
 * 
 * Revision 1.1.2.2  1993/08/16  18:07:52  rsalz
 * 	Initial release
 * 	[1993/08/16  18:02:21  rsalz]
 * 
 * $EndLog$
 */

#if	defined(dce_assert)
#undef dce_assert
#endif	/* defined(dce_assert) */

#if	defined(DCE_ASSERT)

#if	!defined(DCE_SVC__FILE__)
#define DCE_SVC__FILE__		__FILE__
#endif	/* !defined(DCE_SVC__FILE__) */

/*
**  Use the struct, not the typedef, so that <dce/dce_svc.h> is not required.
*/
extern void dce___assert(
    struct dce_svc_handle_s_t *, const char *, const char *, int
);

#define dce_assert(h, EX)	\
	((EX) ? ((void)0) : dce___assert(h, # EX, DCE_SVC__FILE__, __LINE__))

#else

#define dce_assert(h, EX)	((void)0)

#endif	/* defined(DCE_ASSERT) */
