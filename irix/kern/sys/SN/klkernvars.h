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

#ifndef __SYS_SN_KLKERNVARS_H
#define __SYS_SN_KLKERNVARS_H

#define KV_MAGIC_OFFSET		0x0
#define KV_RO_NASID_OFFSET	0x4
#define KV_RW_NASID_OFFSET	0x6

#define KV_MAGIC		0x5f4b565f

#if _LANGUAGE_C

typedef struct kern_vars_s {
	__int32_t	kv_magic;
	nasid_t		kv_ro_nasid;
	nasid_t		kv_rw_nasid;
	__psunsigned_t	kv_ro_baseaddr;
	__psunsigned_t	kv_rw_baseaddr;
} kern_vars_t;

#endif /* _LANGUAGE_C */

#endif /* __SYS_SN_KLKERNVARS_H */

