/**************************************************************************
 *									  *
 * 		 Copyright (C) 1987, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* cpu board specific definitions
 */

#ifndef __SYS_CPU_H__
#define __SYS_CPU_H__

#ident "$Revision: 1.32 $"

/* CPU type definitions for cputype */

#define CPU_IP19	19	/* R4400 multiprocessor */
#define CPU_IP20	20	/* Indigo with 50/75 Mhz R4000 */
#define CPU_IP21	21	/* R8000 multiprocessor */
#define CPU_IP22	22	/* Indigo2 with R4x00 */
#define CPU_IP25	25	/* R10000 multiprocessor */
#define CPU_IP26	26	/* Indigo2 with (?) Mhz TFP */
#define CPU_IP27	27	/* R10000 in SN0 */
#define CPU_IP28	28	/* Indigo2 with R10000 */
#define CPU_IP30	30	/* RACER */
#define CPU_IP32        32      /* Moosehead */
#define CPU_IP33        33      /* Beast in SN1 */

#if IP19
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/IP19.h"
#endif

#if IP20
#include "sys/IP20.h"
#endif

#if IP21
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/IP21.h"
#endif

#if IP22
#include "sys/IP22.h"
#endif

#if IP25
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/IP25.h"
#endif

#if IP26
#include "sys/IP26.h"
#endif

#if IP27
#include "sys/SN/SN0/IP27.h"
#include "sys/SN/SN0/hub.h"
#include "sys/SN/router.h"
#endif

#if IP28
#include "sys/IP22.h"			/* very similar to IP22 baseboard */
#endif

#if IP30
#include "sys/RACER/IP30.h"
#endif

#if IP32
#include "sys/IP32.h"
#endif

#if IPMHSIM
#include "sys/IPMHSIM.h"
#endif

#if IP33
#include "sys/SN/SN1/IP33.h"
#include "sys/SN/SN1/snac.h"
#include "sys/SN/router.h"
#endif

#endif /* __SYS_CPU_H__ */
