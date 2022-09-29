/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1996-1998, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


#ifndef __SYS_SN_SN1_SNACPI_H__
#define __SYS_SN_SN1_SNACPI_H__

#include <sys/SN/SN1/snacpiregs.h>

/*
 * setup aliases so that SN1 can reuse SN0 code
 */
#define    PI_INT_PEND_MOD	     PI_INT_PEND0_MOD
#define    PI_RT_EN_A		     PI_RT_INT_EN_A
#define    PI_RT_EN_B		     PI_RT_INT_EN_B
#define    PI_PROF_EN_A		     PI_PROF_INT_EN_A
#define    PI_PROF_EN_B		     PI_PROF_INT_EN_B

#define    PI_RT_PEND_A		     PI_RT_INT_PEND_A
#define    PI_RT_PEND_B		     PI_RT_INT_PEND_B
#define    PI_PROF_PEND_A	     PI_PROF_INT_EN_A
#define    PI_PROF_PEND_B	     PI_PROF_INT_EN_B

#endif  /* __SYS_SN_SN1_SNACPI_H__ */
