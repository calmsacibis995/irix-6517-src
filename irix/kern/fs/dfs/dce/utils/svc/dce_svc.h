/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * Serviceability general header file.
 */

/*
 * HISTORY
 * $Log: dce_svc.h,v $
 * Revision 65.1  1997/10/20 19:15:06  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.8.2  1996/02/18  23:33:05  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:21:09  marty]
 *
 * Revision 1.1.8.1  1995/12/08  21:37:31  root
 * 	Submit OSF/DCE 1.2.1
 * 
 * 	HP revision /main/jrr_1.2_mothra/3  1995/11/22  22:11 UTC  psn
 * 	Remove EOF condition and add _STDIO_INCLUDED for HP.
 * 
 * 	HP revision /main/jrr_1.2_mothra/2  1995/11/21  23:25 UTC  psn
 * 	Merge XIDL porting changes.
 * 
 * 	HP revision /main/jrr_1.2_mothra/1  1995/11/21  17:25 UTC  psn
 * 	Merge second XIDL drop for DCE 1.2.1
 * 	[1995/11/17  17:00 UTC  dat  /main/dat_xidl2/1]
 * 
 * Revision 1.1.4.7  1994/07/08  17:11:53  bowe
 * 	Add prototype for dce_svc_init() [CR 11156]
 * 	[1994/07/08  16:34:45  bowe]
 * 
 * Revision 1.1.4.6  1994/06/09  16:05:53  devsrc
 * 	cr10892 - fix copyright
 * 	[1994/06/09  15:50:29  devsrc]
 * 
 * Revision 1.1.4.5  1994/05/26  18:51:37  bowe
 * 	Added prototype for dce_svc_inq_sev_name(). [CR 10483,10478]
 * 	[1994/05/26  18:13:40  bowe]
 * 
 * Revision 1.1.4.4  1994/05/19  13:11:07  rsalz
 * 	Spelt dce_svc__debug_fillin2 wrong in DCE_SVC_GET_LEVEL.
 * 	[1994/05/19  13:07:12  rsalz]
 * 
 * Revision 1.1.4.3  1994/05/18  19:27:06  rsalz
 * 	Make static registration work with DCE_SVC_DEBUG (OT CR 10572)
 * 	[1994/05/18  14:31:07  rsalz]
 * 
 * Revision 1.1.4.2  1994/04/12  16:10:17  rsalz
 * 	Make sure #define values do not overlap (OT CR 10332)
 * 	[1994/04/12  15:59:33  rsalz]
 * 
 * Revision 1.1.4.1  1994/03/09  20:43:19  rsalz
 * 	Add dce_pgm_{printf,fprintf,sprintf} (OT CR 9926).
 * 	[1994/03/09  20:14:42  rsalz]
 * 
 * 	Add DCE_SVC__FILE__ (defaults to __FILE__) and use it (OT CR 10107).
 * 	[1994/03/09  19:45:18  rsalz]
 * 
 * Revision 1.1.2.3  1993/11/04  18:40:40  rsalz
 * 	Make dce_svc_bin_log calling sequence be like dce_svc_printf's.
 * 	[1993/11/04  18:37:46  rsalz]
 * 
 * Revision 1.1.2.2  1993/08/16  18:08:02  rsalz
 * 	Initial release
 * 	[1993/08/16  18:02:41  rsalz]
 * 
 * $EndLog$
 */

#if	!defined(_DCE_SVC_H)
#define _DCE_SVC_H

#ifdef __cplusplus
    extern "C" {
#endif

#include <dce/service.h>


/*
**  Compilation controls.
*/
#define DCE_SVC_WANT__FILE__
#if	!defined(DCE_SVC__FILE__)
#define DCE_SVC__FILE__		__FILE__
#endif	/* !defined(DCE_SVC__FILE__) */


/*
**  Message severity levels.
*/
#define svc_c_sev_fatal			0x00000010
#define svc_c_sev_error			0x00000020
#define svc_c_sev_warning		0x00000030
#define svc_c_sev_notice		0x00000040
#define svc_c_sev_notice_verbose	0x00000050
#define svc_c_sev_vendor1		0x00000110
#define svc_c_sev_vendor2		0x00000120
#define svc_c_sev_vendor3		0x00000130
#define svc_c_sev_vendor4		0x00000140
#define svc_c_sev_vendor5		0x00000150
#define svc_c_sev_vendor6		0x00000160
#define svc_c_sev_vendor7		0x00000170
#define svc_c_sev_vendor8		0x00000180
#define svc__c_mask			0x000001F0
#define svc__c_shift			4


/*
**  Actions and routings.
*/
#define svc_c_action_none		0x00000000
#define svc_c_action_abort		0x00001000
#define svc_c_action_exit_bad		0x00002000
#define svc_c_action_exit_ok		0x00004000
#define svc_c_action_brief		0x00008000
#define svc_c_route_stderr		0x00010000
#define svc_c_route_nolog		0x00020000
#define svc_c_actroute_vendor1		0x00100000
#define svc_c_actroute_vendor2		0x00200000
#define svc_c_actroute_vendor3		0x00400000
#define svc_c_actroute_vendor4		0x00800000
#define svc__c_action_mask		0x00FFF000


/*
**  Debug levels.
*/
#define svc_c_debug_off			0x00000000
#define svc_c_debug1			0x00000001
#define svc_c_debug2			0x00000002
#define svc_c_debug3			0x00000003
#define svc_c_debug4			0x00000004
#define svc_c_debug5			0x00000005
#define svc_c_debug6			0x00000006
#define svc_c_debug7			0x00000007
#define svc_c_debug8			0x00000008
#define svc_c_debug9			0x00000009
#define svc__c_debugmask		0x0000000F


/*
**  Miscellaneous constants.
*/
#define dce_svc_c_version		1
#define dce_svc_c_progname_buffsize	32
#define dce_svc_c_maxroutes		10


/*
**  Internal structure for message routing.
*/
typedef struct dce_svc_routing_s_t	dce_svc_routing_t;

typedef struct dce_svc_routing_block_s_t {
    int				nroutes;
    dce_svc_routing_t		*routes[dce_svc_c_maxroutes];
} dce_svc_routing_block_t;


/*
**  Serviceability handle.
*/
typedef struct dce_svc_handle_s_t {
    char			*component_name;
    dce_svc_subcomp_t		*table;
    boolean			setup;
    boolean			translated;
    boolean			allocated;
    boolean			intable;
    dce_svc_routing_block_t	routes;
	/* These types are wrong but we don't want everyone to have to
	 * include <stdarg.h> */
    int				(*filter)(void);
    int				(*filterctl)(void);
} *dce_svc_handle_t;


/*
**  Compile-time macro to define a handle.
*/
#define DCE_SVC_DEFINE_HANDLE(h, t, comp) \
    static struct dce_svc_handle_s_t DCE_CONCAT(svc__, h) = { comp, t }; \
    dce_svc_handle_t h = &DCE_CONCAT(svc__, h);


/*
**  Function declarations.
*/
extern void dce_svc_set_progname(
    char*			/* progname */,
    error_status_t*		/* st */
);

extern dce_svc_handle_t dce_svc_register(
    dce_svc_subcomp_t*		/* table */,
    const idl_char*		/* component_name */,
    error_status_t*		/* status */
);


extern void dce_svc_unregister(
    dce_svc_handle_t		/* handle */,
    error_status_t*		/* status */
);

extern const char *
dce_svc_inq_sev_name(
    unsigned32          	/* attributes */
);

void
dce_svc_init(
    unsigned32			/* flags */,
    error_status_t *		/* status */
);


#if	defined(DCE_SVC_WANT__FILE__)
#define DCE_SVC(handle, args)	handle, DCE_SVC__FILE__, __LINE__, args
#define dce_svc_printf		dce_svc_printf_withfile
extern void dce_svc_printf_withfile(
    dce_svc_handle_t		/* handle */,
    const char*			/* file */,
    const int			/* line */,
    const char*			/* argtypes */,
    const unsigned32		/* table_index */,
    const unsigned32		/* attributes */,
    const unsigned32		/* message_index */,
    ...
);
#else
#define DCE_SVC(handle, args)	handle, args
#define dce_svc_printf		dce_svc_printf_nofile
extern void dce_svc_printf_nofile(
    dce_svc_handle_t		/* handle */,
    const char*			/* args */,
    const unsigned32		/* table_index */,
    const unsigned32		/* attributes */,
    const unsigned32		/* message_index */,
    ...
);
#endif	/* defined(DCE_SVC_WANT__FILE__) */


extern void dce_svc_routing(
    unsigned char*		/* where */,
    error_status_t*		/* status */
);


extern int dce_printf(
    const unsigned32		/* message_index */,
    ...
);

extern int dce_pgm_printf(
    const unsigned32		/* message_index */,
    ...
);


#if	defined(_STDIO_INCLUDED) || defined(_H_STDIO)
extern int dce_fprintf(
    FILE*			/* stream */,
    const unsigned32		/* message_index */,
    ...
);

extern int dce_pgm_fprintf(
    FILE*			/* stream */,
    const unsigned32		/* message_index */,
    ...
);
#endif	/* defined(_STDIO_INCLUDED) || defined(_H_STDIO) */


extern unsigned char *dce_sprintf(
    const unsigned32		/* message_index */,
    ...
);

extern unsigned char *dce_pgm_sprintf(
    const unsigned32		/* message_index */,
    ...
);

#if	defined(DCE_DEBUG)

#define DCE_SVC_DEBUG(arglist)	dce_svc__debug arglist

#define DCE_SVC_GET_LEVEL(handle, table_index)				\
	((handle)->setup ? (handle)->table[(table_index)].sc_level	\
			 : dce_svc__debug_fillin2((handle), (table_index)))
#define DCE_SVC_DEBUG_ATLEAST(handle, table_index, debug_level)		\
	(DCE_SVC_GET_LEVEL((handle), (table_index)) >= (debug_level))
#define DCE_SVC_DEBUG_IS(handle, table_index, debug_level)		\
	(DCE_SVC_GET_LEVEL((handle), (table_index)) == (debug_level))


extern void dce_svc__debug(
    dce_svc_handle_t		/* handle */,
    const int			/* table_index */,
    const unsigned32		/* debug_level */,
    char*			/* format */,
    ...
);

extern unsigned32 dce_svc__debug_fillin2(
    dce_svc_handle_t		/* handle */,
    const unsigned32		/* table_index */
);

#if	defined(DCE_SVC_WANT__FILE__)
#define dce_svc_bin_log		dce_svc_bin_log_withfile
extern void dce_svc_bin_log_withfile(
    dce_svc_handle_t		/* handle */,
    const char			*file,
    const int			line,
    const char*			/* argtypes */,
    const int			/* table_index */,
    const unsigned32		/* debug_level */,
    const unsigned32		/* message_index */,
    ...
);
#else
#define dce_svc_bin_log		dce_svc_bin_log_nofile
extern void dce_svc_bin_log_nofile(
    dce_svc_handle_t		/* handle */,
    const char*			/* argtypes */,
    const int			/* table_index */,
    const unsigned32		/* debug_level */,
    const unsigned32		/* message_index */,
    ...
);
#endif	/* defined(DCE_SVC_WANT__FILE__) */
#define DCE_SVC_LOG(arglist)	dce_svc_bin_log arglist

void dce_svc_debug_routing(
    unsigned char*		/* where */,
    error_status_t*		/* status */
);

extern void dce_svc_debug_set_levels(
    unsigned char*		/* where */,
    error_status_t*		/* status */
);

#else

#define DCE_SVC_DEBUG(arglist)	/* NULL */
#define DCE_SVC_LOG(arglist)	/* NULL */

#define DCE_SVC_DEBUG_ATLEAST(handle, table_index, debug_level)	(0)
#define DCE_SVC_DEBUG_IS(handle, table_index, debug_level)	(0)

#endif	/* defined(DCE_DEBUG) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* !defined(_DCE_SVC_H) */
