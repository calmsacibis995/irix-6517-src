/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if IP25

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/reg.h>
#include <arcs/restart.h>
#include <arcs/signal.h>
#include <sys/cpu.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/IP25.h>

#include <parser.h>
#include <setjmp.h>
#include <menu.h>
#include <libsc.h>
#include <libsk.h>

#define	EVINTR_IP25MON_PRINT	0x10
#define	EVINTR_IP25MON_DONE	0x11

#define	EVINTR_IP25MON_MASK	((1<<EVINTR_IP25MON_PRINT)|(1<<EVINTR_IP25MON_DONE))

#define	EFRAMES(x)	((eframe_t *)((x) * 8 * 64 + \
                                     (MPCONF_ADDR+MPCONF_SIZE*EV_MAX_CPUS)))

static	void	(*launch_pc)(int)	= 0;

static enum {
    cpuNone = 0, 
    cpuMaster = 1,
    cpuReset = 2,
    cpuRunning = 3,
    cpuExit = 4,
    cpuNotSelected = 5
} cpu_status[EV_MAX_CPUS];

static char *cpu_statusString[] = {
    "Not Present",
    "Master",
    "Reset/Ready",
    "Running",
    "Exit"
};

static	char cpu_selected[EV_MAX_CPUS];

static	int	cpu_loaded = 0;

#define	MPCONF_LAUNCH_LEVEL	8

static	int
parse_cpus(char *c, int argc, char *argv[], char cpus[EV_MAX_CPUS])
{
    int		vid;

    if (argc == 0) {
	for (vid = 1; vid < EV_MAX_CPUS; vid++) {
	    cpus[vid] = (cpu_status[vid] == cpuNone) ? 0 : 1;
	}
	cpus[0] = 0;
	return(0);
    }
    bzero(cpus, sizeof(cpus[0]) * EV_MAX_CPUS);
    for ( ; argc > 0; argc--, argv++) {
	vid = atoi(*argv);
	if ((vid < 1) || (vid >= EV_MAX_CPUS)) {
	    printf("%s: invalid cpu \"%s\"\n", c, *argv);
	    return(1);
	}
	if (cpu_status[vid] != cpuNone) {
	    cpus[vid] = 1;
	}
    }
    cpus[cpuid()] = 0;		/* don't count ourself */
    return(0);
}

static	int
cpuStart(char *c, char cpus[EV_MAX_CPUS])
{
    int		vid;
    int		started;
    char	tcpus[EV_MAX_CPUS];

    started = 0;

    for (vid = 1; vid < EV_MAX_CPUS; vid++) {
	tcpus[vid] = 0;
	if (!cpus || cpus[vid]) {
	    switch(cpu_status[vid]) {
	    case cpuMaster:
	    case cpuNone:
		continue;
	    case cpuReset:
	    case cpuExit:
		if (!cpu_selected[vid]) {
		     break;
		 }
		break;
	    default:
		printf("%s: warning: cpu[%d]: %s\n", c, vid, 
		       cpu_statusString[cpu_status[vid]]);
	    } 
	    started++;
	    tcpus[vid] = 1;
	}
    }
    for (vid = 1; vid < EV_MAX_CPUS; vid++) {
	if (tcpus[vid]) {
	    MPCONF[vid].errno = 0;
	    MPCONF[vid].real_sp = 0;
	    MPCONF[vid].nonbss = started;
	    launch_slave(vid, launch_pc, 0, NULL, 0, 0);
	    if (MPCONF[vid].proc_type == EV_CPU_R10000) {
		EV_SETCONFIG_REG(MPCONF[vid].phys_id >> 2 ,
				 MPCONF[vid].phys_id & 3, 0x26, 
				 ((__uint64_t)launch_pc) >> 32);
		EV_SETCONFIG_REG(MPCONF[vid].phys_id >> 2 ,
				 MPCONF[vid].phys_id & 3, 0x27, 
				 ((__uint64_t)launch_pc) & 0xffffffff);
	    }

	    EV_SET_LOCAL(EV_SENDINT, 
			 EVINTR_VECTOR(MPCONF_LAUNCH_LEVEL, 
				       MPCONF[vid].phys_id));
	    cpu_status[vid] = cpuRunning;
	}
    }
    return(started);
}
    

static int
launch_cmd(int argc, char *argv[])
{
    char	cpus[EV_MAX_CPUS];

    if (!cpu_loaded) {
	printf("%s: No loaded program\n", argv[0]);
	return(0);
    }

    if (parse_cpus(argv[0], argc - 1, argv + 1, cpus)) {
	return(0);
    }
    (void)cpuStart(argv[0], cpus);

    return(0);
}    

static int
load_cmd(int argc, char *argv[])
{
    extern ULONG loadbin(char *, void *, void *);
    ULONG	error;

    if (argc != 2) {
	return(1);
    }

    error = loadbin(argv[1], &launch_pc, 0);
    if (error) {
	printf("%s: Unable to load file: ");
	perror((long)error, argv[1]);
	cpu_loaded = 0;
    } else {
	flush_cache();
	cpu_loaded = 1;
    }

    return(0);
}

static	int
select_cmd(int argc, char *argv[])
{
    char	cpus[EV_MAX_CPUS];
    char	*argv0;
    int		i;

    if (argc < 2) {
	return(1);
    }

    argv0 = argv[0];

    if (0 == strcmp(argv[1], "-a")) {
	bzero(cpu_selected, sizeof(cpu_selected));
	argv++;
	argc--;
    }
    if (argc < 2) {
	return(0);
    }
  
    if (parse_cpus(argv[0], argc - 1, argv + 1, cpus)) {
	return(0);
    }

    for (i = 0; i < EV_MAX_CPUS; i++) {
	if (cpus[i]) {
	    switch(cpu_status[i]) {
	    case cpuMaster:
		printf("%s: cpu %d is master - not selected\n", argv0, i);
		break;
	    case cpuNone:
		printf("%s: cpu %d does not exist\n", argv0, i);
		break;
	    case cpuRunning:
		printf("%s: warning: selected cpu %d is \"%s\"\n", 
		       argv0, i);
	    case cpuReset:
	    case cpuExit:
		cpu_selected[i] = 1;
		break;
	    case cpuNotSelected:
		break;
	    default:
		break;
	    }
	}
    }
    return(0);
}


static	int
cpuPoll(int interrupt, char cpus[EV_MAX_CPUS])
{
    extern	void	_scandevs(void);
    evreg_t	ip;
    int		vid;
    int		running, wait;
    char	b[256];

    if (interrupt) {
	wait = 100;
	do {
	    ip = EV_GET_REG(EV_IP0);
	    if (ip & EVINTR_IP25MON_MASK) {
		EV_SET_REG(EV_CIPL0, EVINTR_IP25MON_PRINT); /* clear */
		EV_SET_REG(EV_CIPL0, EVINTR_IP25MON_DONE); /* clear */
		break;
	    } else {
		us_delay(1000);
	    }
	} while (wait--);
	if (0 == ip) {
	    _scandevs();		/* allow for console interrupt */
	    return(1);
	}
    }

    running = 0;

    for (vid = 0; vid < EV_MAX_CPUS; vid++) {
	if ((!cpus || cpus[vid]) && (cpuRunning == cpu_status[vid])) {
	    if (MPCONF[vid].real_sp) {
		strncpy(b, MPCONF[vid].real_sp, sizeof(b) - 1);
		printf("%d[0x%x/0x%x]: %s\n", vid,
		       MPCONF[vid].phys_id >> 2, 
		       MPCONF[vid].phys_id & 3, b);
		MPCONF[vid].real_sp = 0;
	    }
	    if (MPCONF[vid].errno & 0x80) { /* check for completion */
		printf("CPU %d [0x%x/0x%x]: Exit(%d)\n", vid, 
		       MPCONF[vid].phys_id >> 2, 
		       MPCONF[vid].phys_id & 3, 
		       MPCONF[vid].errno & 0x7f);
		cpu_status[vid] = cpuExit;
	    } else {
		running ++;
	    }
	}
    }
    _scandevs();			/* allow for console interrupt */
    return(running);
}


static	int
run_cmd(int argc, char *argv[])
{
    /* Special case - run with no parameters re-runs loaded program */

    if ((argc != 1) || !cpu_loaded) {
	if(load_cmd(argc, argv)) {
	    return(1);
	} else if (!cpu_loaded) {
	    return(0);
	}
    }

    cpuStart(argv[0], NULL);
    while (cpuPoll(0, NULL))
	;

    return(0);				/* never reached */
}


static	char *ertoip_names[] = {
  "Not Defined",				/* 0 */
  "Double bit ECC error write/intervention CPU",/* 1 */
  "Single bit ECC error write/intervention CPU",/* 2 */
  "Parity error on Tag RAM data",		/* 3 */
  "A Chip Addr Parity",				/* 4 */
  "MyRequest timeout on EBus",			/* 5 */
  "A Chip MyResponse D-Resource time out",	/* 6 */
  "A Chip MyIntrvention D-Resource time out", 	/* 7 */
  "Addr Error on MyRequest on Ebus",		/* 8 */
  "My Ebus Data Error",				/* 9 */
  "Intern Bus vs. A_SYNC",			/* 10 */
  "CPU bad data indication",			/* 11 */
  "Parity error on data from D-chip [15:0]",	/* 12 */
  "Parity error on data from D-chip [31:16]",	/* 13 */
  "Parity error on data from D-chip [47:32]",	/* 13 */
  "Parity error on data from D-chip [48:63]",	/* 14 */
  "Double bit ECC error CPU SYSAD/Command",	/* 15 */
  "Single bit ECC error CPU SYSAD/Command",	/* 16 */
  "SysState parity error",			/* 17 */
  "SysCmd parity error",			/* 18 */
  "Multiple Errors detected",			/* 19 */
  "Not defined",				/* 20 */
  "Not defined"					/* 21 */
  "Not defined"					/* 22 */
  "Not defined"					/* 23 */
  "Not defined"					/* 24 */
  "Not defined",				/* 25 */
  "Not defined"					/* 26 */
  "Not defined"					/* 27 */
  "Not defined"					/* 28 */
  "Not defined"					/* 29 */
  "Not defined"					/* 30 */
  "Not defined"					/* 31 */
};

static	int
sccstate_cmd(int argc, char *argv[])
{
    int	i, slot, slice;
    __uint64_t r;
/*
 * IP25 processor board, per-PROCESSOR configuration registers. Note that
 * many of the CPU local registers are shadowed here.
 */
#define EV_CFG_CMPREG0		0x10	/* LSB of the timer comparator */
#define EV_CFG_CMPREG1		0x11	/* Byte 1 of the timer comparator */
#define EV_CFG_CMPREG2		0x12	/* Byte 2 of the timer comparator */
#define EV_CFG_CMPREG3		0x13	/* MSB of the timer comparator */
#define EV_CFG_PGBRDEN		0x14	/* piggyback read enable */
#define EV_CFG_ECCHKDIS		0x15	/* ECC checking disable register */
#define	EV_CFG_SCCREV		0x19	/* SCC revision number */
#define	EV_CFG_IWTRIG		0x20	/* Intervention/write trigger */
#define	EV_CFG_ERTOIP		0x28	/* EV_ERTOIP shadow */
#define	EV_CFG_CERTOIP		0x29	/* EV_CERTOIP shadow */
#define	EV_CFG_ERADDR_HI	0x2a	/* EV_ERADDR shadow - MSW */
#define	EV_CFG_ERADDR_LO	0x2b	/* EV_ERADDR shadow - LSW*/
#define	EV_CFG_ERTAG		0x2c	/* EV_ERTAG shadow  */
#define	EV_CFG_ERSYSBUS_HI	0x2d	/* EV_ERSYSBUS_HI shadow */
#define	EV_CFG_ERSYSBUS_LO_LO	0x2e	/* EV_ERSYSBUS_LO shadow - LSW */
#define	EV_CFG_ERSYSBUS_LO_HI	0x2f	/* EV_ERSYSBUS_LO shadow - MSW */
#define	EV_CFG_FTOUT		0x30	/* Force time out */

#define	EV_CFG_SCRATCH_HI	0x26
#define	EV_CFG_SCRATCH_LO	0x27
#define	EV_CFG_CACHE_SZ		0x3e	/* Cache size */


/*
 * SET and GET configuration register values.  The address to write/read
 * is a function of the processor's slot #, which processor on that board
 * (0-3), and the register number to be written/read.
 */

#define	EV_CONFIGREG_IP25	0x9000000018008000

#define EV_CONFIGADDR_25(slot,proc,reg) \
	((evreg_t *)(EV_CONFIGREG_IP25 + ((slot)<<11) + ((reg+(0x40*(proc)))<<3)))

#define EV_SETCONFIG_REG(slot,proc,reg,value) \
	(store_double((long long *)EV_CONFIGADDR((slot),(proc),(reg)), (long long)(value)))

#define EV_GETCONFIG_REG(slot,proc,reg) \
      ((evreg_t)load_double((long long *)EV_CONFIGADDR((slot),(proc),(reg))))

#define	CONFIG(reg)	\
        EV_GETCONFIG_REG(slot, slice, (reg))

    if (argc != 3) {
	return(1);
    }

    slot = atoi(argv[1]);
    slice= atoi(argv[2]);

    /* Read All state and print it out - SCC only. */

    r = CONFIG(EV_CFG_PGBRDEN);
    printf("\tPGBRDN=0x%x (%s) ", r, r ? "Enabled" : "Disabled");
    r = CONFIG(EV_CFG_ECCHKDIS);
    printf(" EVVCHKDIS=0x%x", r);
    r = CONFIG(EV_CFG_SCCREV);
    printf(" SCCREV=0x%x", r & 0xf);
    r = CONFIG(EV_CFG_IWTRIG);
    printf(" IWTRIG=0x%x ", r & 0xf);	/* next line */
    r = CONFIG(EV_CFG_ERTAG);
    printf("ERTAG=0x%x\n", r);

    r = CONFIG(EV_CFG_ERSYSBUS_HI);	
    printf("ERSYSBUS_HI=0x%x  ", r);
    r = CONFIG(EV_CFG_ERSYSBUS_LO_LO);
    printf("ERSYSBUS_LO_LO=0x%x  ", r);
    r = CONFIG(EV_CFG_ERSYSBUS_LO_HI);
    printf("ERSYSBUS_LO_HI=0x%x\n", r);
    r = CONFIG(EV_CFG_CACHE_SZ);
    printf("CACHE_SZ=0x%x\n", r);
    r = CONFIG(EV_CFG_ERTOIP);
    printf("*** Error/TimeOut Interrupt(s) Pending: %x ==\n", r);

    for (i = 0; i < (sizeof(ertoip_names) / sizeof(ertoip_names[0])); i++)
	if (r & (1 << i))
	    printf("\t %s\n", ertoip_names[i]);

    r = CONFIG(EV_CFG_ERADDR_HI);
    printf("ERADDR_HI=0x%x\n", r);
    r = CONFIG(EV_CFG_ERADDR_LO);
    printf("ERADDR_LO=0x%x\n", r);
    r = CONFIG(EV_CFG_SCRATCH_HI);
    printf("SCRATCH_HI=0x%x ", r);
    r = CONFIG(EV_CFG_SCRATCH_LO);
    printf("SCRATCH_LO=0x%x\n ", r);

    return(0);

#undef	CONFIG    
}

int
t5int_cmd(int argc, char *argv[])
{
    int		vid, level;
    char	cpus[EV_MAX_CPUS];
    char	*c = argv[0];

    if (argc == 1) {
	return(1);
    }

    level = atoi(argv[1]);
    if (level > 127 || level < 0) {
	printf("%s: invalid interrupt level \"%s\"\n", argv[0], argv[1]);
	return(0);
    }
    
    argv += 2;
    argc -= 2;

    if (parse_cpus(c, argc, argv, cpus)) {
	return(0);
    }
    for (vid = 0; vid < EV_MAX_CPUS; vid++) {
	if (cpus[vid]) {
	    EV_SET_LOCAL(EV_SENDINT,EVINTR_VECTOR(level,MPCONF[vid].phys_id));
	}
    }
    return(0);
}

static	int
add_cmd(int argc, char *argv[])
{
    uint	slot, slice, free, id, i;
    mpconf_t	*m;

    if (argc != 3) {
	return(1);
    }

    free = 0;

    slot = atoi(argv[1]);
    slice = atoi(argv[2]);

    if ((slot > 18) || (slice > 3)) {
	printf("%s: Invalid slot/slice\n", argv[0]);
	return(0);
    }

    id = (slot << 2) | slice;

    /* Look for entry */

    for (i = 0, m = MPCONF; i < EV_MAX_CPUS; i++, m++) {
	if (m->mpconf_magic == MPCONF_MAGIC) {
	    if (m->phys_id == id) {
		printf("%s: processor already exists: CPU %d\n", 
		       argv[0], i);
		return(0);
	    }
	} else if (0 == free) {
	    free = i;
	}
    }
    if (free == 0) {
	printf("%s: too many cpus\n", argv[0]);
	return(0);
    }
    m = &MPCONF[free];
    m->mpconf_magic = MPCONF_MAGIC;
    m->phys_id = id;
    m->virt_id = i;
    m->launch = 0;
    m->real_sp = 0;
    m->proc_type = EV_CPU_R10000;

    cpu_status[free] = cpuReset;
    printf("%s: adding cpu %d\n", argv[0], free);
    return(0);
}
    

static	void
printReg(__uint64_t v)
{
    char	b[20];
    static const char hex[] = "0123456789abcdef";
    int	i;

    b[0] = ' ';
    b[1] = '0';
    b[2] = 'x';
    for(i = 0; i < 16; i++, v >>= 4) {
	b[3+16-i-1] = hex[v & 0xf];
    }
    b[2+16+1] = '\0';
    printf(b);
}

static	int
print_cmd(int argc, char **argv)
{
    int		vid, poll, i;
    char	cpus[EV_MAX_CPUS];
    char	*c = argv[0];

    if ((argc > 1) && ((argv[1][0] == '-' && argv[1][1] == 'p'))) { /* poll */
	argc--;
	argv++;
	poll = 1;
    } else {
	poll = 0;
    }

    if (parse_cpus(c, argc - 1, argv + 1, cpus)) {
	return(0);
    }
	
    printf("Watching cpus:");
    i = 0;
    for (vid = 0; vid < EV_MAX_CPUS; vid++) {
	if (cpus[vid] && (cpuRunning == cpu_status[vid])) {
	    printf(" %d", vid);
	    i++;
	}
    }
    if (0 == i) {
	printf(" hmmmmm ... looks like none, seems kinda silly\n");
	return(0);
    }
    printf("\n");
    
    /*
     * If polling - do not wait for an interrupt, if not polling, 
     * watch for an EBUS interrupt at level XXXXXXX
     */
    while (cpuPoll(!poll, cpus)) 
	;
#if 0
	evreg_t ip;
	us_delay(1000000);
	ip = EV_GET_REG(EV_CIPL0);
	printf("ip="); printReg(ip);printf("\n");
    }
#endif
	
    return(0);
}

static	void
printEframe(__uint64_t *e)
{
    printf("\tv0/v1:"); 
        printReg(e[EF_V0]); 
        printReg(e[EF_V1]);
        printf("\n");
    printf("\ta0/a3:");
        printReg(e[EF_A0]);
        printReg(e[EF_A1]);
        printReg(e[EF_A2]);
        printReg(e[EF_A3]);
        printf("\n");
    printf("\ta4/a7:");
        printReg(e[EF_A4]);
        printReg(e[EF_A5]);
        printReg(e[EF_A6]);
        printReg(e[EF_A7]);
        printf("\n");
    printf("\tt0/t3:");
        printReg(e[EF_T0]);
        printReg(e[EF_T1]);
        printReg(e[EF_T2]);
        printReg(e[EF_T3]);
        printf("\n");
    printf("\ts0/s3:");
        printReg(e[EF_S0]);
        printReg(e[EF_S1]);
        printReg(e[EF_S2]);
        printReg(e[EF_S3]);
        printf("\n");
    printf("\ts4/s7:");
        printReg(e[EF_S4]);
        printReg(e[EF_S5]);
        printReg(e[EF_S6]);
        printReg(e[EF_S7]);
        printf("\n");
    printf("\tt8/t9:");
        printReg(e[EF_T8]);
        printReg(e[EF_T9]);
        printf("\n");
    printf("\tk0/k1:");
        printReg(e[EF_K0]);
        printReg(e[EF_K1]);
        printf("\n");
    printf("\tsp/fp:");
        printReg(e[EF_SP]);
        printReg(e[EF_FP]);
        printf("\n");
    printf("\tra   :");
        printReg(e[EF_RA]);
        printf("\n");
    printf("\tepc/cel:");
        printReg(e[EF_EPC]);
        printReg(e[EF_CEL]);
        printf("\n");
    printf("\tBadvaddr/Cause:");
        printReg(e[EF_BADVADDR]);
        printReg(e[EF_CAUSE]);
        printf("\n");
}


static	int
cpu_cmd(int argc, char *argv[])
{
    char	cpus[EV_MAX_CPUS];
    char	*t;
    char	*c = argv[0];
    int		vid, regs;

    regs = (argc > 1) && (argv[1][0] == '-') && (argv[1][1] == 'r');
    if (regs) {
	argc--;
	argv++;
    }
    if (parse_cpus(c, argc - 1, argv + 1, cpus)) {
	return(0);
    }

    /* Now display status from mpconf blocks */

    for (vid = 0; vid < EV_MAX_CPUS; vid++) {
	if (!cpus[vid]) {
	    continue;
	}
	if (MPCONF[vid].mpconf_magic == MPCONF_MAGIC) {	/* valid? */
	    printf("%c %sCPU %d: [0x%x/0x%x] Status: %s (%d)%s",
		   cpu_selected[vid] ? '*' : ' ',
		   regs ? "\n" : "", 
		   vid, MPCONF[vid].phys_id >> 2, MPCONF[vid].phys_id & 3,
		   cpu_statusString[cpu_status[vid]], 
		   MPCONF[vid].errno & 0x7f,
		   MPCONF[vid].real_sp ? " Pending I/O" : "");
	    switch(MPCONF[vid].proc_type) {
	    case EV_CPU_R4000:
		t = "R4000";
		break;
	    case EV_CPU_TFP:
		t = "R8000";
		break;
	    case EV_CPU_R10000:
		t = "R10000";
		break;
	    default:
		t = "unknown";
		break;
	    }
	    printf(" type(%s)\n", t);
	    if (regs) {
		printEframe((__uint64_t *)EFRAMES(MPCONF[vid].phys_id));
	    }
	}
    }
    return(0);
}

static	int	
dc_cmd(int argc, char *argv[])
{
    __uint64_t	r;
    uint slot, proc, reg;

    if (argc != 4) {
	return(1);
    }

    slot = atoi(argv[1]);
    proc = atoi(argv[2]);
    reg  = atoi(argv[3]);

    if ((slot > 15) || (proc > 3)) {
	printf("%s: invalid cpu\n", argv[0]);
	return(0);
    }
    if (reg >= 0x40) {
	printf("%s: register must be < 0x40\n");
	return(0);
    }	

    r = load_double(EV_CONFIGADDR(slot, proc, reg));

    printf("[0x%x/0x%x]: Register[%x]: 0x%x\n", slot, proc, reg, r);
    return(0);
}

static	int	
wc_cmd(int argc, char *argv[])
{
    __uint64_t	r, v;
    uint slot, proc, reg;

    if (argc != 5) {
	return(1);
    }

    slot = atoi(argv[1]);
    proc = atoi(argv[2]);
    reg  = atoi(argv[3]);
    v    = atoi(argv[4]);

    printf("slot=0x%x proc=0x%x reg=0x%x value=0x%x\n", slot, proc, reg, v);

    if ((slot > 16) || (proc > 3)) {
	printf("%s: invalid cpu\n", argv[0]);
	return(0);
    }

    if (reg >= 0x40) {
	printf("%s: register must be < 0x40\n");
	return(0);
    }	

    r = EV_GETCONFIG_REG(slot, proc, reg);
    EV_SETCONFIG_REG(slot, proc, reg, v);

    printf("[0x%x/0x%x]: Register[%d]: 0x%x <-- 0x%x\n", slot, proc, reg, r, v);
    return(0);
}

static struct cmd_table ip25mon_table[] = {
{"add",   CT_ROUTINE add_cmd, 
     "add:\t\tadd <slot> <proc>"},
{"run",	  CT_ROUTINE run_cmd, 
     "run:\t\trun <filename> (same as load/start/poll)"},
{"dc",	  CT_ROUTINE dc_cmd, 
     "dc:\t\tdc <slot> <slice> <reg>"},
{"wc",	  CT_ROUTINE wc_cmd, 
     "wc:\t\t wc <slot> <slice> <reg> <value>"}, 
{"load",  CT_ROUTINE load_cmd, 
     "load:\t\tload <filename>"},
{"scc",   CT_ROUTINE sccstate_cmd,
     "scc:\t\tscc <slot> <slice>"},
{"int",   CT_ROUTINE t5int_cmd,
     "int:\t\tint <slot>"},
{"poll", CT_ROUTINE print_cmd, 
     "poll:\t\tpoll [cpu list]"},
{"select", CT_ROUTINE select_cmd, 
     "select:\t select [-a] [cpu list]"},  
{"start", CT_ROUTINE launch_cmd, 
     "start:\tstart [cpu list]"},
{"cpu", CT_ROUTINE cpu_cmd, 
     "cpu:\t\tcpu [-r] [cpu list]"},
{0, 0, ""}
};

static	void
ip25mon_init(void)
{
    int			i;
    static	int	done = 0;

    if (!done) {
	for (i = 0; i < EV_MAX_CPUS; i++) {
	    if (MPCONF[i].mpconf_magic == MPCONF_MAGIC) {
		cpu_status[i] = cpuReset;
		cpu_selected[i] = 1;
	    } else {
		cpu_status[i] = cpuNone;
		cpu_selected[i] = 0;
	    }
	}
	cpu_status[0] = cpuMaster;
	cpu_selected[0] = 0;
	done = 1;
    }
}
    

static	jmp_buf	ip25mon_buf;
static	void	(*ip25mon_func)(void);

static	void
ip25mon_intr(void)
{

    printf("\n");
    Signal(SIGALRM, SIGIgnore);
    longjmp(ip25mon_buf, 1);
}

int
ip25mon_cmd(int argc, char **argv, char *env, struct cmd_table *parent)
/*
 * Function: ip25mon_cmd
 * Purpose: Process the ip25mon command
 * Parameters: Either command to execute, or go into interactive mode.
 * Returns: 0
 */
{
    extern void _hook_exceptions(void);
    struct cmd_table *ct;

    ip25mon_init();

    ip25mon_func = Signal(SIGINT, ip25mon_intr);

    if (setjmp(ip25mon_buf) && (1 < argc)) { /* not in monitor */
	Signal(SIGINT, ip25mon_func);
	_hook_exceptions();
	return(0);
    }

    if (1 == argc) {			/* no paramters */
	command_parser(ip25mon_table, "ip25> ", 1, parent);
    } else {
	ct = lookup_cmd(ip25mon_table, argv[1]);
	if (NULL == ct) {
	    printf("%s: command \"%s\" not found\n", argv[0], argv[1]);
	    Signal(SIGINT, ip25mon_func);
	    return(1);
	} else if (ct->ct_routine(argc - 1, &argv[1], NULL, parent)) {
	    printf(ct->ct_usage);
	}
    }
    _hook_exceptions();
    return(0);
}
	    
#endif
