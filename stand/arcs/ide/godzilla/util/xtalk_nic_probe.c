/*
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 */
#ident "ide/godzilla/util/xtalk_nic_probe.c: $Revision: 1.6 $"

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/nic.h>
#include <uif.h>
#include <libsk.h>
#include <libsc.h>
#include <sys/mgrashw.h>
#include <sys/xtalk/hq4.h>
#include <sys/odsy.h>
#include <sys/nic.h>

int
check_tmez_nic(void)
{
	return (1);
}

/* taken from IP30prom/IP30.c */
void
ide_dump_nic_info(char *buf,int dumpflag)
{
        int n = 0;

        if (dumpflag) {
                msg_printf(SUM,"%s\n",buf);
                return;
        }

        while (*buf != '\0') {
                switch (*buf) {
                case ':':
                        msg_printf(SUM,": ");
                        buf++;
                        while (*buf != '\0' && *buf != ';') {
				msg_printf(SUM, "%c", *buf);
                                buf++;
                        }
                        msg_printf(SUM,"  ");
                        n++;
                        break;
                default:
                        if (strncmp(buf, "Part:", 5) == 0 ||
                            strncmp(buf, "NIC:", 4) == 0) {
                                msg_printf(SUM, "\n      ");
                                n = 0;
                        }
                        else if (n == 4) {
                                msg_printf(SUM, "\n         ");
                                n = 0;
                        }
			msg_printf(SUM, "%c", *buf);
                        break;
                }
                buf++;
        }
        msg_printf(SUM, "\n");
}

/* This code is based on code from IP30prom/IP30.c 
 * xtalk_nic_probe will check the specified port, read the NIC, and compare
 * the name read to the expected name if do_compare is set.
 */
__int32_t
_xtalk_nic_probe(__int32_t port, char * expected_name, __int32_t do_compare)
{
	widget_cfg_t *widget;
	int id, dumpflag=0;
	char str[80], *buf;
	__uint32_t errors = 0;	
	nic_data_t mcr;

        if (!xtalk_probe(XBOW_K1PTR, port)) {
		msg_printf(SUM, "No device seen at port 0x%x\n",
			port);
       	        return (0);
	}

	buf = malloc(1024);

	strcpy(str, "Name:");
	strcat(str, expected_name);

        widget = (widget_cfg_t *)K1_MAIN_WIDGET(port);
        id = ((widget->w_id) & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;

        if (id == BRIDGE_WIDGET_PART_NUM) {
                bridge_t *bridge = (bridge_t *)widget;
                mcr = (nic_data_t)&bridge->b_nic;
                cfg_get_nic_info(mcr, buf);
		if (do_compare) {
			if (strstr(buf, str) == 0x0) {
                	   msg_printf(SUM, "bridge(%d):\n", 
				XWIDGET_REV_NUM(bridge->b_wid_id));
			   msg_printf(ERR, "Bad/Missing NIC on %s board, port 0x%x\n", expected_name, port);
			   msg_printf(SUM, "The Name field in the NIC was not %s. Here is what was read from the NIC: \n\n", str);
			   ide_dump_nic_info(buf,dumpflag);
			   msg_printf(SUM, "\n\n");
			   errors++;
			}
			else {
				if (*Reportlevel >= VRB) {
					ide_dump_nic_info(buf,dumpflag);
			   		msg_printf(SUM, "\n\n");
				}
			}
		}
		else {
                	msg_printf(SUM, 
			    "bridge(%d): ", XWIDGET_REV_NUM(bridge->b_wid_id));
                	ide_dump_nic_info(buf,dumpflag);
		}
        }
        else if (id == HQ4_WIDGET_PART_NUM) {
                mgras_hw *hq4 = (mgras_hw *)widget;
                mcr = (nic_data_t)&hq4->microlan_access;
                cfg_get_nic_info(mcr, buf);
		if (do_compare) { /* check for 1 of 4 gfx types */
			if ((strstr(buf, "Name:GM10") == 0x0) && 
			    (strstr(buf, "Name:GM20") == 0x0) && 
			    (strstr(buf, "Name:MOT10") == 0x0) && 
			    (strstr(buf, "Name:MOT20") == 0x0))  {
                	   msg_printf(SUM, "HQ4: ");
			   msg_printf(ERR, "Bad/Missing NIC on graphics board, port 0x%x\n", port);
			   msg_printf(SUM, "The Name field in the NIC was not GM10, GM20, MOT10, or MOT20. Here is what was read from the NIC: \n\n");
			   ide_dump_nic_info(buf,dumpflag);
			   msg_printf(SUM, "\n\n");
			   errors++;
			   return (errors);
			}
			else {
				if (*Reportlevel >= VRB) {
					ide_dump_nic_info(buf,dumpflag);
			   		msg_printf(SUM, "\n\n");
				}
			}
			/* Check for tmez nics only if HQ4 nic was okay */
			if (check_tmez_nic()) {
			   if (strstr(buf, "data not available") || 
					strstr(buf, "nic_match_rom") ||
					strstr(buf, "loop in NIC") ||
					strstr(buf, "crc16") ||
					strstr(buf, "unreadable"))
			   {
                	   	msg_printf(SUM, "TMEZ: ");
			   	msg_printf(ERR, "Bad/Missing NIC on TMEZ board, port 0x%x\n",  port);
			   	msg_printf(SUM, "Here is what was read from the NIC: \n\n");
			   	ide_dump_nic_info(buf,dumpflag);
			   	msg_printf(SUM, "\n\n");
			   	errors++;
			   }
			   else {
				/* No error msgs implies no NIC or a good nic
				 * which is ok 
				 */
			   }
			}
		}
		else {
                	ide_dump_nic_info(buf,dumpflag);
			errors = HQ4_WIDGET_PART_NUM;
		}
        }
        else if (id == ODSY_XT_ID_PART_NUM_VALUE) {
	/* temp hack XXXX until textport code is checked in */
                msg_printf(VRB, "Odyssey board seen in port 0x%x\n", port);
		return(ODSY_XT_ID_PART_NUM_VALUE);
	}
        else {
                msg_printf(VRB, "unknown widget partno=0x%x\n",id);
	}

        free(buf);

	return(errors);
}

/* UI for _xtalk_nic_probe 
 * Usage: xtalk_nic_probe 
 *		-n <optional string name> 
 *		-p <optional port #> 
 *		-c (turn compare on, default is off) 
 */
int
xtalk_nic_probe(int argc, char ** argv)
{
	__int32_t i, port, last_port, first_port, do_compare, bad_arg, errors=0;
	char name[80];

	/* default */
	last_port = HEART_ID;
	first_port = BRIDGE_ID-1;
	strcpy(name, "IP30");
	do_compare = 0;
	bad_arg = 0;

	argc--; argv++;                                                 
        while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {          
                switch (argv[0][1]) {                                 
                        case 'p':                                    
                                if (argv[0][2]=='\0') {             
                                        atob(&argv[1][0], (int*)&port);
                                        argc--; argv++;                 
                                }                                      
                                else                                  
                                        atob(&argv[0][2], (int*)&port);
				first_port = port;
				last_port = first_port - 1;
                                break;                      
                        case 'c': 
                                do_compare = 1; break;     
                        case 'n':                                       
                                if (argv[0][2]=='\0') {                 
                                        strcpy(name, &argv[1][0]);
                                        argc--; argv++;                 
                                }                                       
                                else                                    
                                        strcpy(name, &argv[0][2]);
                                break;                                  
                        default: bad_arg++; break;                      
                }                                                       
                argc--; argv++;                                         
        }                                                               
        if ((bad_arg) || (argc)) {                                      
           msg_printf(SUM, "Usage: -n <optional name> -p <optional port number> -c (do comparison, default is no comparison\n");
                return (0);                                             
        }                          

	/* look for other bridge/HQ4 NICs on xtalk */
	for (i = first_port; i > last_port; i--) {
		errors += _xtalk_nic_probe(i, name, do_compare);
	}

	if (do_compare) {
		return (errors);
	}
	else {
		if (errors == ODSY_XT_ID_PART_NUM_VALUE) {
			return(ODSY_XT_ID_PART_NUM_VALUE); /* odyssey */
		}
		else if (errors == HQ4_WIDGET_PART_NUM) {
			return(HQ4_WIDGET_PART_NUM); /* Impact */
		}
	   	else return (0);
	}
}
