/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       evo_nic.c
*/

#include "evo_diag.h"

extern void _xtalk_nic_probe(__int32_t, char *, __int32_t);
/*
 * Forward Function References
 */
void    evo_nic_check(void);

void evo_nic_check(void)
{

        __int32_t port, do_compare;
        char *evo_name;
	int err = 0;

	msg_printf(SUM, "--> Evo_nic_test started\n");
        port = evo_bridge_port;  /* this is the xtalk port where the evo is */
	msg_printf(SUM, "port is %d\n",port);
        do_compare = 1;

        strcpy(evo_name, "EVO");

	msg_printf(SUM, "entering _xtalk_nic_probe\n");
         _xtalk_nic_probe(port, evo_name, do_compare);
#if 0
	if (err == 0){ 
		msg_printf(SUM, "--> Evo_nic_test passed\n");
	} else {
		msg_printf(SUM, "--> Evo_nic_test failed\n");
	}
#endif

}
