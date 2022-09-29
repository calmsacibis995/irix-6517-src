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

#ifndef __SYS_SN_ROUTERDRV_H__
#define __SYS_SN_ROUTERDRV_H__

/**************************************************************************
  This file is obsolete.  It will be removed shortly.
  Please use sn0drv.h and/or hubstat.h instead.
 **************************************************************************/

#if defined (SN0)
#include <sys/SN/SN0/sn0drv.h>
#include <sys/SN/SN0/hubstat.h>

#define ROUTERINFO_GETINFO	SN0DRV_GET_ROUTERINFO
#define GETSTRUCTSIZE		SN0DRV_GET_INFOSIZE
#define HUBINFO_GETINFO		SN0DRV_GET_HUBINFO

#define ROUTERINFO_GETSZ	SN0DRV_GET_INFOSIZE

#define RTRDRV_UNKNOWN_DEVICE	-1
#define RTRDRV_ROUTER_DEVICE	1
#define RTRDRV_HUB_DEVICE	2
#endif

#endif /* __SYS_SN_ROUTERDRV_H__ */

