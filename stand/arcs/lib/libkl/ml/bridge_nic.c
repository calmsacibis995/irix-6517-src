/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.8 $"

#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <kllibc.h>		/* for macros like isalpha etc */
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/xbow.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/nic.h>
#include <promgraph.h>
#include <libsc.h>
#include <setjmp.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/intr.h>
#include <sys/SN/kldiag.h>


/*
 * parse_bridge_nic_info : Parses the MFG NIC string obtained from the
 * 			   bridge.
 *
 * returns : The value of the field requested, if present, in field_value.
 *           The function returns NULL if we reached the end of the NIC
 *	     string, else it returns the pointer to where the parsing should
 * 	     start from.
 */

char *
parse_bridge_nic_info(char *nic_string, char *f, char *field_value)
{
        char *nic_ptr = 0 ;
	nic_t	nic ;
	int len = strlen(f);

	if (field_value)
	    *field_value = 0;

        while (*nic_string) {
                if (!strncmp(nic_string, f, len) && nic_string[len] == ':') {
			int i ;
                        nic_ptr = nic_string + len + 1;
			for (i = 0;
			     *nic_ptr && *nic_ptr != ';' && i < 31; i++) 
				field_value[i] = *nic_ptr++;
			field_value[i] = 0 ;	

			if (*nic_ptr)
				return (++nic_ptr) ;
			else
                        	return NULL ;
                }

		while (*nic_string && *nic_string++ != ';')
		    ;
        }

	return NULL ;
}

