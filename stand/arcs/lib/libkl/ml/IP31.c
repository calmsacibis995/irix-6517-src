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
 * File: IP31.c
 * IP31 and config related information
 */

#include <sys/types.h>
#include <sys/nic.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/hub.h>
#define	DEF_IP27_CONFIG_TABLE
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/IP31.h>
#include <libkl.h>
#include <libsc.h>

int
ip31_get_config(nasid_t nasid, ip27config_t *c)
{
	int	index, pimm_psc = PIMM_PSC(nasid);

	if ((index = ip31_psc_to_flash_config[pimm_psc]) == IP27_CONFIG_UNKNOWN)
		return -1;

	c->time_const = (uint) CONFIG_TIME_CONST;
	c->r10k_mode = ip27config_table[index].r10k_mode;
	c->magic = (__uint64_t) CONFIG_MAGIC;
	c->freq_cpu = ip27config_table[index].freq_cpu;
	c->freq_hub = ip27config_table[index].freq_hub;
	c->freq_rtc = (__uint64_t) IP27C_KHZ(IP27_RTC_FREQ);
	c->ecc_enable = (uint) CONFIG_ECC_ENABLE;
	c->fprom_cyc = ip27config_table[index].fprom_cyc;
	c->mach_type = ip27config_table[index].mach_type;
	c->check_sum_adj = (uint) 0;
	c->fprom_wr = ip27config_table[index].fprom_wr;

	return 0;
}

int
ip31_pimm_psc(nasid_t nasid, char *psc, char *nic)
{
	extern	int get_hub_nic_info(nic_data_t, char *);
	char	hub_nic_info[1024];

	if(nic)
		strcpy(hub_nic_info, nic);
	else
		get_hub_nic_info((nic_data_t) REMOTE_HUB(nasid, MD_MLAN_CTL),
		hub_nic_info);

	if (!hub_nic_info)
		return -1;
	else if (!strstr(hub_nic_info, "IP31"))
		return -1;

	if (PIMM_PRESENT(nasid)) {
		if (psc)
			*psc = PIMM_PSC(nasid);
		return 0;
	}
	else
		return -1;
}
