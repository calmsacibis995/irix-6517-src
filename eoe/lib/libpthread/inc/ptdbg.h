#ifndef _PTDBG_H_
#define _PTDBG_H_

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include <sys/types.h>


/* Global names
 */
#define ss_thread_init	PFX_NAME(ss_thread_init)
#define jt_rld_pt	PFX_NAME(jt_rld_pt)
#define jt_pt_ss	PFX_NAME(jt_pt_ss)

#include <sys/types.h>

/* Jump tables (call backs)
 */
#ifndef JT_DECLARE
typedef void *(*jt_func_t)(void);
#define JT_DECLARE(jt, n)			\
	typedef struct {			\
		int		jt_vers;	\
		jt_func_t	jt_func[n];	\
	} jt
#endif	/* JT_DECLARE */


/* Entry points in pthreads to be called by rld
 */
#define JT_RLD_PT_VERSION	0
JT_DECLARE(jt_rld_pt_t, 4);
extern jt_rld_pt_t	jt_rld_pt;


/* Entry points in SpeedShop to be called by pthreads
 */
#define JT_PT_SS_VERSION	0
JT_DECLARE(jt_pt_ss_t, 1);
extern jt_pt_ss_t	*jt_pt_ss;

#define SS		(jt_pt_ss)
#define SS_NEW_ID(id, pc) \
			if (SS) { ((void (*)(pthread_t, void* (*)(void *))) \
					jt_pt_ss->jt_func[0])(id, pc); }
#define SS_SET_ID(id)	if (SS) { ((void (*)(pthread_t))	\
					jt_pt_ss->jt_func[1])(id); }
#define SS_DEL_ID(id)	if (SS) { ((void (*)(pthread_t))	\
					jt_pt_ss->jt_func[2])(id); }


/* Library exported information
 *
 * To be used by external tools e.g. debuggers.
 */
#define PT_LIBRARY_INFO_VERSION		1	/* version of library info */
typedef struct {

	/* First field is fixed for all versions
	 */
	__uint32_t	ptl_info_version;

	/* Following data may change based on the debug version
	 */

	__uint32_t	ptl_library_version;

	/* Pthread data
	 *
	 * In this version pthread structures are held in one large
	 * array allocated at run-time.
	 */
	void		*ptl_pt_tbl;	/* (ptr to) pt array */
	__uint32_t	ptl_pt_size;	/* XXX ficus compat */
	unsigned short	*ptl_pt_count;	/* (ptr to) count of used entries */
} pt_lib_info_t;

extern pt_lib_info_t	__pt_lib_info;


#endif /* !_PTDBG_H_ */
