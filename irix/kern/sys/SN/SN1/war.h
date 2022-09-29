/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN1_WAR_H__
#define __SYS_SN_SN1_WAR_H__
/****************************************************************************
 * Support macros and defitions for hardware workarounds in		    *
 * early chip versions.                                                     *
 ****************************************************************************/

/*
 * This is the bitmap of runtime-switched workarounds.
 */
typedef short warbits_t;

extern int warbits_override;

#endif /*  __SYS_SN_SN1_WAR_H__ */
