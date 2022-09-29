/***********************************************************************\
 *       File:           diagval.c                                       *
 *									*
 \***********************************************************************/

#include <sys/types.h>
#include <sys/SN/kldiag.h>
#include "pod_failure.h"
#include "ip27prom.h"
#include "pod.h"
#include "libasm.h"

static char memconf_failed[] =	"MEM CONFIG FAILED!";
static char io6_failed[] =	"MASTER BASEIO FAILED!";
static char scache_failed[] =	"SCACHE FAILED!";
static char dcache_failed[] =	"DCACHE FAILED!";
static char icache_failed[] =	"ICACHE FAILED!";
static char io6prom_failed[]=	"BASEIO PROM FAILED!";

static char scache_hang[] =	"SLAVE SCACHE HANG!";
static char dcache_hang[] =	"SLAVE DCACHE HANG!";
static char icache_hang[] =	"SLAVE ICACHE HANG!";
static char cache_init_hang[] =	"SLAVE CACHE INIT HANG!";

static char dcache_tag_failed[] = "DCACHE TAG FAILED!";
static char scache_tag_failed[] = "SCACHE TAG FAILED!";
static char fscache_tag_failed[] ="SCACHE THRU FPU FAILED!";


static char memconf_long[]=
"Memory configuration has failed.  Cannot load BASEIO PROM.";
static char io6prom_long[]=
"Diagnostics detected a problem with your BASEIO PROM.";
static char memdsbl_long[]=
"All memory banks had to be disabled due to test failures.";
static char addrln_long[]=
"The address line self-test failed.  Cannot continue.";
static char dbldis_long[]=
"The PROM was unable to disable failing memory banks.";
static char insufmem_long[]=
"You must have at least 32 megabytes of working memory to load the IO PROM";
static char nomem_long[]=
"The IP27 PROM did not recognize any memory in the system.";
static char noio6_long[]=
"The IP27 PROM did not recognize any BASEIO boards in the system.";
static char dlbuserr_long[]=
"An exception occurred while downloading the BASEIO PROM to memory.";
static char badioc3_long[]=
"The IOC3 chip on the master BASEIO board has failed diagnostics.";
static char iocfgbuserr_long[]=
"An exception occurred while configuring an IO board.";
static char promexc_long[]=
"The PROM code took an unexpected General Exception.";
static char promnmi_long[]=
"The PROM received a nonmaskable interrupt.";


pod_scmsg_t scmsg_map[] = {
{KLDIAG_SCACHE_DATA_WAY0,	scache_failed,		(char *)NULL},
{KLDIAG_SCACHE_DATA_WAY1,	scache_failed,		(char *)NULL},
{KLDIAG_SCACHE_ADDR_WAY0,	scache_failed,		(char *)NULL},
{KLDIAG_SCACHE_ADDR_WAY1,	scache_failed,		(char *)NULL},
{KLDIAG_DCACHE_DATA_WAY0,	dcache_failed,		(char *)NULL},
{KLDIAG_DCACHE_DATA_WAY1,	dcache_failed,		(char *)NULL},
{KLDIAG_DCACHE_ADDR_WAY0,	dcache_failed,		(char *)NULL},
{KLDIAG_DCACHE_ADDR_WAY1,	dcache_failed,		(char *)NULL},
{KLDIAG_ICACHE_DATA_WAY0,	icache_failed,		(char *)NULL},
{KLDIAG_ICACHE_DATA_WAY1,	icache_failed,		(char *)NULL},
{KLDIAG_ICACHE_ADDR_WAY0,	icache_failed,		(char *)NULL},
{KLDIAG_ICACHE_ADDR_WAY1,	icache_failed,		(char *)NULL},
{KLDIAG_DCACHE_HANG,		dcache_hang,		(char *)NULL},
{KLDIAG_SCACHE_HANG,		scache_hang,		(char *)NULL},
{KLDIAG_ICACHE_HANG,		icache_hang,		(char *)NULL},
{KLDIAG_CACHE_INIT_HANG,	cache_init_hang,	(char *)NULL},
{KLDIAG_DCACHE_TAG,		dcache_tag_failed,	(char *)NULL},
{KLDIAG_SCACHE_TAG_WAY0,	scache_tag_failed,	(char *)NULL},
{KLDIAG_SCACHE_TAG_WAY1,	scache_tag_failed,	(char *)NULL},
{KLDIAG_SCACHE_FTAG,	fscache_tag_failed,	(char *)NULL},

/*                           12345678901234567890		*/

/* XXX fixme: add some more values here XXX */
{KLDIAG_RETURNING,		"Reentering POD mode",	(char *)NULL},
{KLDIAG_EXC_GENERAL,	"PROM GENERAL EXCEPTION!",	promexc_long},
/* FIX THIS WHOLE THING AND ADD OTHER EXCEPTIONS */
{KLDIAG_NMI,		"PROM NMI HANDLER",	promnmi_long},
{KLDIAG_DEBUG,		"CPU in POD mode.",	(char *)NULL}
};

void code_msg(unsigned char diagcode, char *buffer)
{
    char *cp;
    int i, c;


    if ((diagcode == KLDIAG_DEBUG) || (diagcode == KLDIAG_RETURNING)) {
	buffer[0] = '\0';
	return;
    }

    for (cp = "Diagnostic code #"; (c = LBYTE(cp)) != 0; cp++)
	*buffer++ = c;

    *buffer++ = '0' + diagcode / 100;
    *buffer++ = '0' + (diagcode / 10) % 10;
    *buffer++ = '0' + (diagcode % 10);

    *buffer = '\0';
}

int sc_disp(unsigned char diagcode)
{
    int num_entries;
    int i;
    char buffer[21];

    num_entries = sizeof(scmsg_map) / sizeof(pod_scmsg_t);

    /* printf("Diagcode %x\n", diagcode); */
    for (i = 0; i < num_entries; i++) {
	if (LBYTEU((char *) &scmsg_map[i].dv_code) == diagcode) {
#if 0
	    /* XXX Update to use elsc_display_mesg */
	    sysctlr_message(scmsg_map[i].short_msg);
#endif
	    return 1;
	}
    }

    code_msg(diagcode, buffer);
#if 0
    /* XXX Update to use elsc_display_mesg */
    sysctlr_message(buffer);
#endif

    return 0;
}


unsigned char *get_long_scmsg(unsigned char diagcode)
{
    int num_entries;
    int i;

    num_entries = sizeof(scmsg_map) / sizeof(pod_scmsg_t);

    for (i = 0; i < num_entries; i++) {
	if (LBYTEU((char *) &scmsg_map[i].dv_code) == diagcode) {
	    return (unsigned char *)scmsg_map[i].long_msg;
	}
    }
    return (unsigned char *)NULL;
}

void scroll_n_print(unsigned char diagcode)
{
#if 0
    printf("XXX fixme: scroll_n_print\n");
    unsigned char *cp;
    unsigned char buffer[21];

    if ((cp = get_long_scmsg(diagcode))) {
	while (!poll()) {
	    scroll_message(cp, 1);
	    if (!poll()) {
		code_msg(diagcode, (char *)buffer);
		scroll_message(buffer, 1);
	    }
	}
    }

    code_msg(diagcode, (char *)buffer);

    /* Update to use elsc_nvram_write */
    sysctlr_bootstat((char *)buffer, 0);
#endif
}
