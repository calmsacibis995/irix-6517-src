/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
 
#ifndef __SYS_GROUPINTR_H__
#define __SYS_GROUPINTR_H__

#if defined(EVEREST)
#include <sys/EVEREST/groupintr.h>
#endif

#if defined(SN)
#include <sys/SN/groupintr.h>
#endif

#if !defined(EVEREST) && !defined(SN)
typedef int intrgroup_t;
#endif

#endif /* __SYS_GROUPINTR_H__ */
