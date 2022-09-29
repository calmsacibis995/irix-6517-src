/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef __IDE_ECC_H__
#define __IDE_ECC_H__


#ident "stand/arcs/ide/IP30/include/ecc.h: $Revision 1.1$"

/* switch for ecc_errgen function */
#define SINGLE_BIT_ERR 0
#define DOUBLE_BIT_ERR 1

#define LSB_ONE_BIT	0x0000000000000001
#define LSB_TWO_BITS	0x0000000000000011

#define COUNT_DOWN	0x0000000000000fff	/* for heart_ecc.c */

/* outputs for _is_ECC_error function */
#define	IS_ECC_ERROR_SINGLE	1
#define	IS_ECC_ERROR_DOUBLE	2

#define	DATA_TYPE_ERROR	0	/* in HEART_MEMERR_ADDR */
#define	ADDR_TYPE_ERROR	HME_ERR_TYPE_ADDR

#endif /* __IDE_ECC_H__ */

