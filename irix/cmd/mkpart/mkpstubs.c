/*
 * Stubs to test mkpart till syssgi and
 * the ethernet and nvram driver are working.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/syssgi.h>

#include "mkpart.h"

/* ARGSUSED */

int
syssgi_stub(int op, int a1, int a2, void *a3, void *a4)
{
    switch(a1) {
	case SYSSGI_PARTOP_NODEMAP:
	{
	    pn_t *pn = (pn_t *)a3 ;

	    pn[0].pn_partid = 1 ;
	    pn[0].pn_module = 1 ;
	    pn[0].pn_nasid  = 0 ;

	    pn[1].pn_partid = 2 ;
	    pn[1].pn_module = 2 ;
	    pn[1].pn_nasid  = 1 ;

	    pn[2].pn_partid = 1 ;
	    pn[2].pn_module = 3 ;
	    pn[2].pn_nasid  = 2 ;

	    pn[3].pn_partid = 2 ;
	    pn[3].pn_module = 4 ;
	    pn[3].pn_nasid  = 3 ;

	    return 4 ;
	}
/* NOTREACHED */
	break ;

	case SYSSGI_PARTOP_GETPARTID:
	{
	    return 1 ;
	}
/* NOTREACHED */
	break ;

	case SYSSGI_PARTOP_PARTMAP:
	{
	    part_info_t *pi = (part_info_t *)a3 ;
	    pi[0].pi_partid = 1 ;
	    pi[0].pi_flags  = PI_F_ACTIVE ;
	    pi[1].pi_partid = 2 ;
	    pi[1].pi_flags  = PI_F_ACTIVE ;
	    return 2 ;
	}
/* NOTREACHED */
	break ;
    }
    return 1 ;
}

/* ARGSUSED */

int
get_ip_address_stub(partid_t p, char *ip)
{
    strcpy(ip, "192.132.182.140") ; /* sunburst */
    return 1 ;
}

