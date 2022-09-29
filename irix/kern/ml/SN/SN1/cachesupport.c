/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996-1998 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/SN/klconfig.h>
#include <ksys/cacheops.h>
#include <sys/kthread.h>
#include <sys/rt.h>
