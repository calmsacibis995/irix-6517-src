/***********************************************************************\
*       File:           diagval.c                                       *
*									*
\***********************************************************************/

#include "pod_failure.h"
#include "pod.h"
#include "prom_externs.h"
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/evdiag.h>

/* Include the actual string values of the diagval messages */
#include <sys/EVEREST/diagval_strs.i>

void	ccuart_flush(void);
void	ccuart_putc(char);
void	ccuart_puts(char *);
void	lostrncpy(unsigned char *, unsigned char *, int);

char	sysctlr_getpanel(void);

static char mc3conf_failed[] =	"MC3 CONFIG FAILED!";
static char io4_failed[] = 	"MASTER IO4 FAILED!";
static char scache_failed[] = 	"SCACHE FAILED!";
static char dcache_failed[] =	"DCACHE FAILED!";
static char icache_failed[] =	"ICACHE FAILED!";
static char io4prom_failed[]=	"IO4PROM FAILED!";
static char bustags_failed[]=	"BUS TAGS FAILED!";

static char scache_hang[] = 	"SLAVE SCACHE HANG!";
static char dcache_hang[] =	"SLAVE DCACHE HANG!";
static char icache_hang[] =	"SLAVE ICACHE HANG!";
static char cache_init_hang[] =	"SLAVE CACHE INIT HANG!";

static char dcache_tag_failed[] = "DCACHE TAG FAILED!";
static char scache_tag_failed[] = "SCACHE TAG FAILED!";
static char fscache_tag_failed[] ="SCACHE THRU FPU FAILED!";


static char mc3conf_long[]=
	"Memory board configuration has failed.  Cannot load IO PROM.";
static char io4prom_long[]=
	"Diagnostics detected a problem with your IO4 PROM.";
static char nomem_long[]=
	"All memory banks had to be disabled due to test failures.";
static char addrln_long[]=
	"The address line self-test failed.  Cannot continue.";
static char dbldis_long[]=
	"The PROM was unable to disable failing memory banks.";
static char insufmem_long[]=
	"You must have at least 32 megabytes of working memory to load the IO PROM";
static char nomc3_long[]=
	"The IP21 PROM did not recognize any memory boards in the system.";
static char noio4_long[]=
	"The IP21 PROM did not recognize any IO4 boards in the system.";
static char dlbuserr_long[]=
	"An exception occurred while downloading the IO4 PROM to memory.";
static char noepc_long[]=
	"There must be an EPC chip on the IO board in the highest-numbered slot.";
static char badia_long[]=
	"The IA chip on the master IO4 board has failed diagnostics.";
static char badepc_long[]=
	"The EPC chip on the master IO4 board has failed diagnostics.";
static char iocfgbuserr_long[]=
	"An exception occurred while configuring an IO board.";
static char promexc_long[]=
	"The PROM code took an unexpected exception.";
static char promnmi_long[]=
	"The PROM received a nonmaskable interrupt.";


pod_scmsg_t scmsg_map[] = {
    {EVDIAG_SCACHE_DATA,	scache_failed,		(char *)NULL},
    {EVDIAG_SCACHE_ADDR,	scache_failed,		(char *)NULL},
    {EVDIAG_DCACHE_DATA,	dcache_failed,		(char *)NULL},
    {EVDIAG_DCACHE_ADDR,	dcache_failed,		(char *)NULL},
    {EVDIAG_ICACHE_DATA,	icache_failed,		(char *)NULL},
    {EVDIAG_ICACHE_ADDR,	icache_failed,		(char *)NULL},
    {EVDIAG_DCACHE_HANG,	dcache_hang,       	(char *)NULL},
    {EVDIAG_SCACHE_HANG,	scache_hang,            (char *)NULL}, 
    {EVDIAG_ICACHE_HANG,	icache_hang,            (char *)NULL},
    {EVDIAG_CACHE_INIT_HANG,    cache_init_hang,        (char *)NULL},
    {EVDIAG_DCACHE_TAG,		dcache_tag_failed,      (char *)NULL},
    {EVDIAG_SCACHE_TAG,		scache_tag_failed,      (char *)NULL}, 
    {EVDIAG_SCACHE_FTAG,	fscache_tag_failed, 	(char *)NULL},

    /*                           12345678901234567890		*/
    {EVDIAG_BIST_FAILED,	mc3conf_failed,		mc3conf_long},
    {EVDIAG_NO_MEM,		"NO GOOD MEMORY FOUND", nomem_long},
    {EVDIAG_BAD_ADDRLINE,	mc3conf_failed,		addrln_long},
    {EVDIAG_BAD_DATALINE,	mc3conf_failed,		mc3conf_long},
    {EVDIAG_BANK_FAILED,	"MC3 READBACK ERROR!",	mc3conf_long},
    {EVDIAG_MC3CONFBUSERR,	mc3conf_failed,		mc3conf_long},
    {EVDIAG_MC3TESTBUSERR,	mc3conf_failed,		mc3conf_long},
    {EVDIAG_MC3DOUBLEDIS,	mc3conf_failed,		dbldis_long},
    {EVDIAG_MC3NOTENOUGH,	"INSUFFICIENT MEMORY!",	insufmem_long},
    {EVDIAG_NOMC3,		"NO MEM BOARDS FOUND!",	nomc3_long},
    {EVDIAG_NOIO4,		"NO IO BOARDS FOUND!",	noio4_long},
    {EVDIAG_BADCKSUM,		io4prom_failed,		io4prom_long},
    {EVDIAG_BADENTRY,		io4prom_failed,		io4prom_long},
    {EVDIAG_TOOLONG,		io4prom_failed,		io4prom_long},
    {EVDIAG_BADSTART,		io4prom_failed,		io4prom_long},
    {EVDIAG_BADMAGIC,		io4prom_failed,		io4prom_long},
    {EVDIAG_DLBUSERR,		io4prom_failed,		dlbuserr_long},
    {EVDIAG_NOEPC,		"NO EPC CHIP FOUND!",	noepc_long},
    {EVDIAG_CFGBUSERR,		"IO4 CONFIG FAILED!",	iocfgbuserr_long},
    {EVDIAG_IAREG_BUSERR,	io4_failed,		badia_long},
    {EVDIAG_IAPIO_BUSERR,	io4_failed,		badia_long},
    {EVDIAG_IAREG_FAILED,	io4_failed,		badia_long},
    {EVDIAG_IAPIO_BADERR,	io4_failed,		badia_long},
    {EVDIAG_IAPIO_NOINT,	io4_failed,		badia_long},
    {EVDIAG_IAPIO_WRONGLVL,	io4_failed,		badia_long},
    {EVDIAG_MAPRDWR_BUSERR,	io4_failed,		badia_long},
    {EVDIAG_MAPADDR_BUSERR,	io4_failed,		badia_long},
    {EVDIAG_MAPWALK_BUSERR,	io4_failed,		badia_long},
    {EVDIAG_MAP_BUSERR,		io4_failed,		badia_long},
    {EVDIAG_MAPRDWR_FAILED,	io4_failed,		badia_long},
    {EVDIAG_MAPADDR_FAILED,	io4_failed,		badia_long},
    {EVDIAG_MAPWALK_FAILED,	io4_failed,		badia_long},
    {EVDIAG_EPCREG_FAILED,	"EPC CHIP FAILED!",	badepc_long},
    {EVDIAG_LOOPBACK_FAILED,	"EPC UART FAILED!",	(char *)NULL},

    {EVDIAG_BUSTAGDATA,		bustags_failed,		(char *)NULL},
    {EVDIAG_BUSTAGDATA,		bustags_failed,		(char *)NULL},
    {EVDIAG_BUSTAGADDR,		bustags_failed,		(char *)NULL},

    /*                           12345678901234567890		*/
    {EVDIAG_RETURNING,		"Reentering POD mode",	(char *)NULL},
    {EVDIAG_PANIC,		"PROM EXCEPTION!",	promexc_long},
    {EVDIAG_NMI,		"PROM NMI HANDLER",	promnmi_long},
    {EVDIAG_DEBUG,		"CPU in POD mode.",	(char *)NULL}
};

#ifdef PROM
void sysctlr_bootstat(char *message, int leading)
{
    int i;

    ccuart_putc(SC_ESCAPE);
    ccuart_putc(SC_SET);
    ccuart_putc(SC_PROCSTAT);
    for (i = 0; i < leading; i++)
	    ccuart_putc(' ');
    ccuart_puts(message);
    ccuart_putc(SC_TERM);
}


void scroll_message(unsigned char *message, int reps)
{
	int display_size;
	int length;
	int i;
	unsigned char buffer[80];
	char c;

	display_size = 20;
	for (i = 0; i < 3; i++) {
		c = sysctlr_getpanel();
		if (c == 'T') {
			display_size = 40;
			break;
		} else if (c == 'E') {
			display_size = 20;
			break;
		}
	}

	ccuart_flush();

	i = 0;
	while(get_char(message + i) != '\0')
		i++;
	length = i - 1;
	if (length < display_size) {
		i = 0;
		sysctlr_bootstat((char *)message, 0);
		while(!pod_poll() && ((reps == -1) || (i < reps * length))) {
			i++;
			delay(500000);
		}
		sysctlr_bootstat("", 1);
		return;
	}

	i = 0;
	while (!pod_poll() && (reps != 0)) {
		if (i < display_size) {
			lostrncpy(buffer, message, i);
			sysctlr_bootstat((char *)buffer, display_size - i);
		} else {
			lostrncpy(buffer, message + (i - display_size),
								display_size);
			sysctlr_bootstat((char *)buffer, 0);
		}
		if (i > (length + display_size)) {
			i = 0;
			if (reps > 0)
				reps--;
		}
		i++;
		delay(400000);
	}
	sysctlr_bootstat("", 1);
}


void code_msg(unsigned char diagcode, char *buffer)
{
    unsigned char *cp;
    int i;


    if ((diagcode == EVDIAG_DEBUG) || (diagcode == EVDIAG_RETURNING)) {
	buffer[0] = '\0';
	return;
    }

    /*    012345678901234567890 */
    cp = "Diagnostic code #";

    for (i = 0; get_char(cp); i++)
	buffer[i] = get_char(cp++);
    buffer[17] = '0' + diagcode / 100;
    buffer[18] = '0' + (diagcode / 10) % 10;
    buffer[19] = '0' + (diagcode % 10);

    buffer[20] = '\0';
}

int sc_disp(unsigned char diagcode)
{
    int num_entries;
    int i;
    char buffer[21];

    num_entries = sizeof(scmsg_map) / sizeof(pod_scmsg_t);

    /* loprintf("Diagcode %x\n", diagcode); */
    for (i = 0; i < num_entries; i++) {
	if (get_char(&scmsg_map[i].dv_code) == diagcode) {
	    sysctlr_message(scmsg_map[i].short_msg);
	    return 1;
	}
    }

    code_msg(diagcode, buffer);
    sysctlr_message(buffer);
    return 0;
}


unsigned char *get_long_scmsg(unsigned char diagcode)
{
    int num_entries;
    int i;

    num_entries = sizeof(scmsg_map) / sizeof(pod_scmsg_t);

    for (i = 0; i < num_entries; i++) {
	if (get_char(&scmsg_map[i].dv_code) == diagcode) {
	    return (unsigned char *)scmsg_map[i].long_msg;
	}
    }
    return (unsigned char *)NULL;
}


char *get_diag_string(uint diagcode)
{
    int num_entries;
    int i;


    num_entries = sizeof(diagval_map) / sizeof(diagval_t);

    for (i = 0; i < num_entries; i++) {
	if (get_char(&diagval_map[i].dv_code) == EVDIAG_DIAGCODE(diagcode))
	    return diagval_map[i].dv_msg;
    }
    loprintf("diagcode = %d\n", diagcode);
    return "<check evdiag.h> ";
}


void scroll_n_print(unsigned char diagcode)
{
    unsigned char *cp;
    unsigned char buffer[21];

    if ((cp = get_long_scmsg(diagcode))) {
	while (!pod_poll()) {
	    scroll_message(cp, 1);
	    if (!pod_poll()) {
		    code_msg(diagcode, (char *)buffer);
		    scroll_message(buffer, 1);
	    }
	}
    }

    code_msg(diagcode, (char *)buffer);
    sysctlr_bootstat((char *)buffer, 0);
}


#else

main()
{
    int i;
    int num_entries;

    printf("IP21 PROM Diagnostic Codes and Their Meanings\n\n");

    printf("CODE  MEANING\n\n");
    num_entries = sizeof(diagval_map) / sizeof(diagval_t);
    for (i = 0; i < num_entries; i++)
	printf("%03d    %s\n", diagval_map[i].dv_code, diagval_map[i].dv_msg);
   
    num_entries =  sizeof(scmsg_map) / sizeof(pod_scmsg_t);
    printf("\nCODE  System Controller Short Message\n\n");
    for (i = 0; i < num_entries; i++)
	printf("%03d   %s\n", scmsg_map[i].dv_code, scmsg_map[i].short_msg);

    printf("\nCODE  System Controller Long Message\n\n");
    for (i = 0; i < num_entries; i++)
	if (scmsg_map[i].long_msg)
	    printf("%03d   %s\n", scmsg_map[i].dv_code, scmsg_map[i].long_msg);
    printf("\n");
}

#endif
