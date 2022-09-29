/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1999, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef	__SYS_UMAPOP_H__
#define	__SYS_UMAPOP_H__

#include <sys/immu.h>
#include <ksys/rmap.h>

extern int fetchop_flush(pfd_t *pfd);

/*  
 * WARNING *
 *   DELETE_MAPPING is a performance senstive macro and should 
 *   not be used as a kitchen sink
 */



#if SN0

#define	DELETE_MAPPING(pfdp, pdep) \
        if (pg_isfetchop(pdep)) \
                fetchop_flush(pfdp); \
        RMAP_DELMAP(pfdp, pdep);
#else

#define	DELETE_MAPPING(pfdp, pdep)	RMAP_DELMAP(pfdp, pdep);
#endif


#endif	/* __SYS_UMAPOP_H__ */
