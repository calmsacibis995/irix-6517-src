/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.37 $"

#ifdef LICENSE
#include <lmsgi.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <invent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/errno.h>
#include <sys/sbd.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h"

static int ckpt_acquire_stderr(void);
static void ckpt_release_stderr(int);

/* the rlimit of the calling process.  */
static struct rlimit cpr_rlimit[RLIM_NLIMITS];

int
ckpt_license_valid(void)
{
#ifdef LICENSE
/*
 * SGI vendor Daemon
 */
static char *vname = "sgifd";
#define VERSION         "1.0"
#define PRODUCT        "CPR"
LM_CODE(code, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2, VENDOR_KEY1, VENDOR_KEY2,
	VENDOR_KEY3, VENDOR_KEY4, VENDOR_KEY5);

	if (license_init(&code, vname, B_TRUE)) {
		cerror("Failed to initialize FLEXlm license for product %s (%s)\n",
			PRODUCT, license_errstr());
		return (0);
	}
	if (license_set_attr(LM_A_USER_RECONNECT, (LM_A_VAL_TYPE) NULL) ||
	    license_set_attr(LM_A_USER_RECONNECT_DONE, (LM_A_VAL_TYPE) NULL) ||
	    license_set_attr(LM_A_USER_EXITCALL, (LM_A_VAL_TYPE) NULL) ||
	    license_set_attr(LMSGI_NO_SUCH_FEATURE, (LM_A_VAL_TYPE) NULL) ||
	    license_set_attr(LMSGI_30_DAY_WARNING, (LM_A_VAL_TYPE) NULL) ||
	    license_set_attr(LMSGI_60_DAY_WARNING, (LM_A_VAL_TYPE) NULL) ||
	    license_set_attr(LMSGI_90_DAY_WARNING, (LM_A_VAL_TYPE) NULL))
		return (0);

	if (license_chk_out(&code, PRODUCT, VERSION)) {
		cerror("Unable to checkout license for product %s (%s)\n",
			PRODUCT, license_errstr());
		return (0);
	}
	/*
	 * Check right back in since we are doing nodelock
	 */
	license_chk_in(PRODUCT, 0);
#endif
	return (1);
}

int
ckpt_perm_check(ckpt_obj_t *first)
{
	ckpt_obj_t *co;
	uid_t server_ruid = getuid();
	gid_t server_rgid = getgid();

	/*
	 * root can checkpoint unconditionally
	 */
	if (server_ruid == 0)
		return (0);

	FOR_EACH_PROC(first, co) {
		if (co->co_prfd == -1)
			continue;
		/*
		 * root can checkpoint unconditionally
		 */
		if (server_ruid == 0)
			break;

		/*
		 * owner with euid root can checkpoint the client processes
		 */
		if ((server_ruid != co->co_psinfo->pr_uid ||
		      server_rgid != co->co_psinfo->pr_gid)) {
			cerror("process %d has no permission to checkpoint pid %d\n",
				getpid(), co->co_pid);
			return (-1);
		}
	}
	return (0);
}

void
ckpt_signal_procs(ckpt_obj_t *first, int sig)
{
	ckpt_obj_t *co;

	FOR_EACH_PROC(first, co) {
		if (kill(co->co_pid, sig) == -1)
			cerror("failed to send signal SIGCKPT to process %d\n",
				co->co_pid);
	}
	/* 
	 * XXX add ckpt_sgi() wait for the user signal handling code to be done 
	 */
}

int
ckpt_myself(ckpt_obj_t *first)
{
	pid_t mypid = get_mypid();
	ckpt_obj_t *co;
	int rc = 0;

	FOR_EACH_PROC(first, co) {

		if (co->co_pid == mypid) {
			rc = 1;
			break;
		}
	}
	return (rc);
}

void
ckpt_error(const char *format, ...)
{
	va_list	args;
	int errstate;

	errstate = ckpt_acquire_stderr();

	fprintf(stderr, "CPR Error: ");
#ifdef DEBUG
	fprintf(stderr, "Pid %d, File %s, Line %d:\n\t", getpid(), cpr_file, cpr_line);
#endif
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fflush(stderr);

	ckpt_release_stderr(errstate);
}

void
ckpt_notice(const char *format, ...)
{
	va_list	args;
	int errstate;

	errstate = ckpt_acquire_stderr();

	va_start(args, format);
	fprintf(stderr, "CPR Notice(%d): ", getpid());
#ifdef DEBUG
	fprintf(stderr, "File %s, Line %d:\n\t", cpr_file, cpr_line);
#endif
	vfprintf(stderr, format, args);
	va_end(args);
	fflush(stderr);

	ckpt_release_stderr(errstate);
}

#ifdef DEBUG
void
cdebug(const char *format, ...)
{
	va_list	args;
	int errstate;
	char buf0[256];
	char buf1[256];

	sprintf(buf0, "CDEBUG(%d): ", getpid());
	va_start(args, format);
	vsprintf(buf1, format, args);
	va_end(args);
	strcat(buf0, buf1);
	errstate = ckpt_acquire_stderr();

	fprintf(stderr, buf0);
	fflush(stderr);

	ckpt_release_stderr(errstate);
}
#endif
/*
 * Hack to support restarting shared addr/non-shared fd sprocs
 *
 * This is only a problem during restart and from target context.
 *
 * Since sharing addr, they all "see" the same stderr iob struct,
 * but since not sharing fds, an update by one may not work for another.
 *
 * For such sprocs, we stash a copy of their stderrs fd in the prda.
 */
int ckpt_stderr_initialized = 0;

static int
ckpt_acquire_stderr(void)
{
	int errfd;

	if (!saddrlist || !saddrlist->saddr_liblock || !ckpt_stderr_initialized)
		return (0);

	if((errfd = *((int *)(PRDA->usr_prda.fill))) < 0)
		return (0);

	ckpt_acquire_mutex(saddrlist->saddr_liblock, get_mypid());

	fileno(stderr) = errfd;

	return (1);
}

static void
ckpt_release_stderr(int state)
{
	if (state == 0)
		return;

	fileno(stderr) = -1;

	ckpt_release_mutex(saddrlist->saddr_liblock, get_mypid());
}

/*
 * Check if we are batch scheduled
 */
int
ckpt_batch_scheduled(void)
{
	int prfd;
	char procname[32];
	prpsinfo_t psinfo;

	sprintf(procname, "/proc/pinfo/%d", getpid());
	if ((prfd = open(procname, O_RDONLY)) < 0) {
		cerror("proc open (%s)\n", STRERR);
		return (-1);
	}
	if (ioctl(prfd, PIOCPSINFO, &psinfo) < 0) {
		cerror("proc psinfo ioctl (%s)\n", STRERR);
		return (-1);
	}
	(void)close(prfd);

	IFDEB1(cdebug("cpr scheduler clas name:%s\n", psinfo.pr_clname));

	if ((strcmp("B", psinfo.pr_clname) == 0)||
	    (strcmp("BC", psinfo.pr_clname) == 0))
		return (1);

	return (0);
}

/*
 * chpt_allow_uncached
 *
 * Check hardware to determine if non-cached op is okay
 */
int
ckpt_allow_uncached(void)
{
	inventory_t *invp;

	setinvent();

	while(invp = getinvent()) {
		if ((invp->inv_class == INV_PROCESSOR)&&
		    (invp->inv_type == INV_CPUBOARD)) {
			if (invp->inv_state == INV_IP26BOARD) {
				endinvent();
				return (0);
			}
			endinvent();
			return (1);
		}
	}	
	endinvent();

	return (0);
}

struct imp_tbl {
	char *it_name;
	unsigned it_imp;
};

/*
 * This table must be updated as new cpu types are supported
 */
static struct imp_tbl cpu_imp_tbl[] = {
	{ "Unknown CPU type.",  C0_MAKE_REVID(C0_IMP_UNDEFINED,0,0) },
	{ "MIPS R5000 Processor Chip",  C0_MAKE_REVID(C0_IMP_R5000,0,0) },
	{ "MIPS R4650 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4650,0,0) },
	{ "MIPS R4700 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4700,0,0) },
	{ "MIPS R4600 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4600,0,0) },
	{ "MIPS R8000 Processor Chip",  C0_MAKE_REVID(C0_IMP_R8000,0,0) },
	{ "MIPS R12000 Processor Chip", C0_MAKE_REVID(C0_IMP_R12000,0,0) },
	{ "MIPS R10000 Processor Chip", C0_MAKE_REVID(C0_IMP_R10000,0,0) },
	{ "MIPS R6000A Processor Chip", C0_MAKE_REVID(C0_IMP_R6000A,0,0) },
	{ "MIPS R4400 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4400,0,0) },
	{ "MIPS R4000 Processor Chip",  C0_MAKE_REVID(C0_IMP_R4000,0,0) },
};

char *
ckpt_cpuname(int cputype)
{
	int i;

	cputype &= C0_IMPMASK;	/* only interested in implementation, not rev */

	for (i = 0; i < sizeof(cpu_imp_tbl)/sizeof(struct imp_tbl); i++) {
		if (cputype == cpu_imp_tbl[i].it_imp)
			return (cpu_imp_tbl[i].it_name);
	}
	IFDEB1(cdebug("couldn't find cputype %x\n", cputype));
	/*
	 * didn't find cputype
	 */
	return (cpu_imp_tbl[0].it_name);
}

static struct imp_tbl board_imp_tbl[] = {
	{ "Unknown Processor Board",	-1 },
	{ "IP19 Processor Board", INV_IP19BOARD },
	{ "IP20 Processor Board", INV_IP20BOARD },
	{ "IP21 Processor Board", INV_IP21BOARD },
	{ "IP22 Processor Board", INV_IP22BOARD },
	{ "IP26 Processor Board", INV_IP26BOARD },
	{ "IP25 Processor Board", INV_IP25BOARD },
	{ "IP30 Processor Board", INV_IP30BOARD },
	{ "IP28 Processor Board", INV_IP28BOARD },
	{ "IP32 Processor Board", INV_IP32BOARD },
	{ "IP27 Processor Board", INV_IP27BOARD },
};

char *
ckpt_boardname(int boardtype)
{
	int i;

	for (i = 0; i < sizeof(board_imp_tbl)/sizeof(struct imp_tbl); i++) {
		if (boardtype == board_imp_tbl[i].it_imp)
			return (board_imp_tbl[i].it_name);
	}
	IFDEB1(cdebug("couldn't find boardtype %x\n", boardtype));
	/*
	 * didn't find boardtype
	 */
	return (board_imp_tbl[0].it_name);
}

/*
 * Get processor board and cpu type fom hardware inventory and save
 * to shared state file
 */
int
ckpt_save_hinv(ch_t *ch, ckpt_prop_t *pp)
{
	ckpt_hinv_t hinv;
	inventory_t *invp;
	long rc;

	hinv.ch_magic = CKPT_MAGIC_HINV;
	hinv.ch_board = -1;
	hinv.ch_cpu = -1;

	setinvent();

	while(((hinv.ch_board == -1)||(hinv.ch_cpu == -1))&&
	      (invp = getinvent())) {
		if (invp->inv_class == INV_PROCESSOR) {
			if (invp->inv_type == INV_CPUBOARD)
				hinv.ch_board = invp->inv_state;

			else if (invp->inv_type == INV_CPUCHIP)
				hinv.ch_cpu = invp->inv_state;

		}
	}
	endinvent();

	if ((hinv.ch_board == -1)||(hinv.ch_cpu == -1)) {
		cerror("Failedto locate processor board and cpu types\n");
		setoserror(ECKPT);
		return (-1);
	}
	CWRITE(ch->ch_sfd, &hinv, sizeof(hinv), 1, pp, rc);
	if (rc < 0) {
		cerror("hinv write (%s)\n", STRERR);
		return (-1);
	}
	return (0);
}
/*
 * Perform a check on the checkpointed vs. current cpu architectures
 */
/* ARGSUSED */
int 
ckpt_read_hinv(cr_t *crp, ckpt_ta_t *tp, ckpt_magic_t magic, int *misc)
{
	ckpt_hinv_t hinv;
	int cputype = -1;
	int cpuboard = -1;
	inventory_t *invp;

	if (read(crp->cr_sfd, &hinv, sizeof(hinv)) < 0) {
		cerror("hinv read (%s)\n", STRERR);
		return (-1);
	}
	if (hinv.ch_magic != magic) {
		cerror("hinv bad magic %s expected %s\n",
			&hinv.ch_magic, &magic);
		setoserror(ECKPT);
		return (-1);
	}
	setinvent();

	while(((cputype == -1) || (cpuboard == -1))&&(invp = getinvent())) {
		if (invp->inv_class == INV_PROCESSOR) {
			if (invp->inv_type == INV_CPUBOARD)
				cpuboard = invp->inv_state;
			else if (invp->inv_type == INV_CPUCHIP)
				cputype = invp->inv_state;
		}
	}
	endinvent();

	if (cputype == -1) {
		cerror("Failedto locate cpu type\n");
		setoserror(ECKPT);
		return (-1);
	}
	IFDEB1(cdebug("checkpoint cpu: %s, restart cpu %s\n",
			ckpt_cpuname(hinv.ch_cpu),
			ckpt_cpuname(cputype)));
	/*
	 * Restart calls with a NULL pointer, wants a check done
	 * stat calls will a non-NULL pointer, wants hinv returned
	 */
	if (misc == NULL) {

		hinv.ch_cpu &= C0_IMPMASK;
		cputype &= C0_IMPMASK;

		if (hinv.ch_cpu != cputype || hinv.ch_board != cpuboard) {
			cerror("Attempt to restart on different architecture:"
		         "checkpointed on cpu %s, board %s\n"
			 "current system cpu %s, board %s\n",
			ckpt_cpuname(hinv.ch_cpu),ckpt_boardname(hinv.ch_board),
			ckpt_cpuname(cputype), ckpt_boardname(cpuboard));
			setoserror(ECKPT);
			return (-1);
		}
	} else
		*((ckpt_hinv_t *)misc) = hinv;

	return (0);
}
#ifdef DEBUG_MEMALLOC
int
touch(void *X)
{
        return (*(int *)X);
}
#endif


/* 
 * this is a copy from libgen/src/dirname.c 
 * It is moved here to avoid dependency on gen lib. 
 */
char *
ckpt_path_to_dirname(char *s)
{

	register char	*p;

	if( !s  ||  !*s )			/* zero or empty argument */
		return  ".";

	p = s + strlen( s );
	while( p != s  &&  *--p == '/' )	/* trim trailing /s */
		;
	
	if ( p == s && *p == '/' )
		return "/";

	while( p != s )
		if( *--p == '/' ) {
			if ( p == s )
				return "/";
			while ( *p == '/' )
				p--;
			*++p = '\0';
			return  s;
		}
	
	return  ".";
}

/* 
 * this is a copy from libgen/src/basename.c 
 * It is moved here to avoid dependency on gen lib. 
 */
char *
ckpt_path_to_basename(char *s)
{
	register char   *p;

	if( !s  ||  !*s )		       /* zero or empty argument */
		return  ".";

	p = s + strlen( s );
	while( p != s  &&  *--p == '/' )	/* skip trailing /s */
		*p = '\0';

	if ( p == s && *p == '\0' )	     /* all slashes */
		return "/";

	while( p != s )
		if( *--p == '/' )
			return  ++p;

	return  p;
}


void
ckpt_bump_limits(void)
{
	struct rlimit rl;
	int i;
	/*
	 * We want to unlimited access to a bunch of resources
	 */
	rl.rlim_cur = RLIM_INFINITY;
	rl.rlim_max = RLIM_INFINITY;

	for (i = 0; i < RLIM_NLIMITS; i++) {
		switch (i) {
		case RLIMIT_FSIZE:
		case RLIMIT_DATA:
		case RLIMIT_NOFILE:
		case RLIMIT_VMEM:
		case RLIMIT_RSS:
		case RLIMIT_STACK:
#if (RLIMIT_AS != RLIMIT_VMEM)
		case RLIMIT_AS:
#endif
			/* remember the callers rlimits */
			getrlimit(i, &cpr_rlimit[i]);
			if (setrlimit(i, &rl)) {
				cnotice("Failed to increase limit on %d (%s)\n", i, STRERR);
				(void) getrlimit(i, &rl);
				cnotice("rlim_cur=%d, rlim_max=%d\n", rl.rlim_cur, rl.rlim_max);
			}
			break;
		default:
			break;
		}
	}
}

void
ckpt_restore_limits(void)
{
	int i;
	struct rlimit rl;

	for (i = 0; i < RLIM_NLIMITS; i++) {
		switch (i) {
		case RLIMIT_FSIZE:
		case RLIMIT_DATA:
		case RLIMIT_NOFILE:
		case RLIMIT_VMEM:
		case RLIMIT_RSS:
		case RLIMIT_STACK:
#if (RLIMIT_AS != RLIMIT_VMEM)
		case RLIMIT_AS:
#endif
			if (setrlimit(i, &cpr_rlimit[i])) {
				cnotice("Failed to restore limit on %d (%s)\n",
								 i, STRERR);
				(void) getrlimit(i, &rl);
				cnotice("rlim_cur=%d, rlim_max=%d\n", 
						rl.rlim_cur, rl.rlim_max);
			}
			break;
		default:
			break;
		}
	}
}

