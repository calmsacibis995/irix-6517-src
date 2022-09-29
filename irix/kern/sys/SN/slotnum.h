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

#ifndef __SYS_SN_SLOTNUM_H__
#define __SYS_SN_SLOTNUM_H__

typedef	unsigned char slotid_t;

#if defined (SN0)
#include <sys/SN/SN0/slotnum.h>
#elif defined (SN1)
#include <sys/SN/SN1/slotnum.h>
#elif defined (_KERNEL)

#error <<BOMB! slotnum defined only for SN0 and SN1 >>

#endif /* !SN0 && !SN1 */

#endif /* __SYS_SN_SLOTNUM_H__ */
