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

/*
 * File: agent.h
 *	This file has definitions for the hub and snac interfaces.
 */

#ifndef __SYS_SN_AGENT_H__
#define __SYS_SN_AGENT_H__

#include <sys/SN/addrs.h>
#include <sys/SN/arch.h>
#include <sys/SN/io.h>

#if defined (SN0)
#include <sys/SN/SN0/hub.h>
#elif defined (SN1)
#include <sys/SN/SN1/snac.h>
#endif /*!SN0 && !SN1 */

/*
 * NIC register macros
 */

#define HUB_NIC_ADDR(_cpuid) 						   \
	REMOTE_HUB_ADDR(COMPACT_TO_NASID_NODEID(cputocnode(_cpuid)),       \
		MD_MLAN_CTL)

#define SET_HUB_NIC(_my_cpuid, _val) 				  	   \
	(HUB_S(HUB_NIC_ADDR(_my_cpuid), (_val)))

#define SET_MY_HUB_NIC(_v) 					           \
	SET_HUB_NIC(cpuid(), (_v))

#define GET_HUB_NIC(_my_cpuid) 						   \
	(HUB_L(HUB_NIC_ADDR(_my_cpuid)))

#define GET_MY_HUB_NIC() 						   \
	GET_HUB_NIC(cpuid())


#endif /* __SYS_SN_AGENT_H__ */
