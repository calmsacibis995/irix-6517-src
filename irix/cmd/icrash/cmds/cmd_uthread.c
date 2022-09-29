#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_uthread.c,v 1.9 1999/11/19 20:39:58 lucc Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>
#include "icrash.h"

typedef struct cbr {
	command_t cmd;
	int cnt;
} cbr;

/* XXX need to hardcode these defines */
#define UT_SXBRK	0x00000008      /* thread stopped for lack of memory */
#define UT_STOP         0x00000010      /* uthread jctl stopped */
#define SRUN		1               /* Running.                     */
#define PWEIGHTLESS	-5


static int
check_evenrun(kaddr_t ut, cbr *val)
{
k_ptr_t utp, pp;
kaddr_t paddr;
uint kflags, uflags;
int good=0;

	/* get the associated proc pointer and check for S_RUN */
	kl_reset_error();
	utp = kl_get_kthread(ut, 0);
	if (KL_ERROR) { return 0; }

	paddr = kl_kaddr(utp, "uthread_s", "ut_proc");

	pp = kl_get_proc(paddr, 2, K_TEMP);
	if (KL_ERROR) { return 0; }

	uflags=KL_UINT(utp, "uthread_s", "ut_flags");
	kflags=KL_UINT(utp,  "kthread", "k_flags");

	if(KL_INT(pp, "proc", "p_stat") == SRUN) {

		if((kflags & KT_SLEEP) && !(uflags & UT_SXBRK)) {

			if ((kflags  & (KT_LTWAIT|KT_NWAKE)) == KT_NWAKE) good++;

			goto out;
		}
		if(uflags & UT_STOP) goto out;

		/* KL_INT casts a short into a int */
		if(KL_INT(utp, "kthread", "k_basepri") > PWEIGHTLESS) good++;
	}
out:
	if(good) {

		if (val->cmd.flags & C_KTHREAD) {
			print_kthread(ut, utp, val->cmd.flags, val->cmd.ofp);
		}
		else {
			print_uthread_s(ut, utp, val->cmd.flags, val->cmd.ofp);
		}
		val->cnt++;

	}
	kl_free_block(utp);
	kl_free_block(pp);
	return 0;
}


/*
 * uthread_cmd() -- Dump out uthread struct information 
 */
int
uthread_cmd(command_t cmd)
{
	int i, j, utcnt = 0, mode, first_time = TRUE, uthread_cnt = 0;
	k_ptr_t procp, utp; 
	kaddr_t value, proc, uthread;

	if(cmd.flags & C_LIST) {

	cbr x;

		x.cmd=cmd;
		x.cnt=0;

		if (cmd.flags & C_KTHREAD) {
			kthread_banner(cmd.ofp, BANNER|SMAJOR);
		}
		else {
			uthread_s_banner(cmd.ofp, BANNER|SMAJOR);
		}

		walk_uthreads(check_evenrun, &x);

		uthread_cnt = x.cnt;

	} 
	else if (!cmd.nargs) {
		uthread_cnt = list_active_uthreads(cmd.flags, cmd.ofp);
	} 
	else {
		for (i = 0; i < cmd.nargs; i++) {

			if ((first_time == TRUE) || (cmd.flags & C_FULL) ||
					(cmd.flags & C_NEXT)) {
				if (cmd.flags & C_KTHREAD) {
					kthread_banner(cmd.ofp, BANNER|SMAJOR);
				}
				else {
					uthread_s_banner(cmd.ofp, BANNER|SMAJOR);
				}
				first_time = FALSE;
			}

			kl_get_value(cmd.args[i], &mode, 0, &value);
			if (KL_ERROR) {
				KL_ERROR |= KLE_BAD_UTHREAD_S;
				kl_print_error();
				continue;
			}

			if (mode == 0) {
				KL_SET_ERROR_NVAL(KLE_BAD_UTHREAD_S, value, mode);
				kl_print_error();
			}
			else if (mode == 1) {

				/* Treat value as a PID. This will allow us to print
				 * out all uthreads associated with a particular PID.
				 */
				proc = kl_pid_to_proc(value);
				procp = kl_get_proc(proc, 2, 0);
				uthread = kl_proc_to_kthread(procp, &utcnt);
				kl_free_block(procp);
			}
			else {
				if (cmd.flags & C_SIBLING) {

					/* Print out all siblings for the specified uthread
					 */
					utp = kl_get_uthread_s(value, 2, (cmd.flags|C_ALL));
					proc = kl_kaddr(utp, "uthread_s", "ut_proc");
					kl_free_block(utp);
					procp = kl_get_proc(proc, 2, 0);
					uthread = kl_proc_to_kthread(procp, &utcnt);
					kl_free_block(procp);
				}
				else {
					uthread = value;
					utcnt = 1;
				}
			}

			for (j = 0; j < utcnt; j++) {
				utp = kl_get_uthread_s(uthread, 2, (cmd.flags|C_ALL));
				if (KL_ERROR) {
					KL_SET_ERROR_NVAL(KL_ERROR, value, mode);
					kl_print_error();
					break;
				}
				if (cmd.flags & C_KTHREAD) {
					print_kthread(uthread, utp, cmd.flags, cmd.ofp);
				}
				else {
					print_uthread_s(uthread, utp, cmd.flags, cmd.ofp);
				}
				uthread_cnt++;
				uthread = kl_kaddr(utp, "uthread_s", "ut_next");
				kl_free_block(utp);
			}
		}
	}
	if (cmd.flags & C_KTHREAD) {
		kthread_banner(cmd.ofp, SMAJOR);
	}
	else {
		uthread_s_banner(cmd.ofp, SMAJOR);
	}
	fprintf(cmd.ofp, "%d active uthreads found\n", uthread_cnt);
	return(0);
}

#define _UTHREAD_USAGE "[-l] [-f] [-n] [-k] [-S] [-w outfile] [uthread_list]"

/*
 * uthread_usage() -- Print the usage string for the 'uthread' command.
 */
void
uthread_usage(command_t cmd)
{
	CMD_USAGE(cmd, _UTHREAD_USAGE);
}

/*
 * uthread_help() -- Print the help information for the 'uthread' command.
 */
void
uthread_help(command_t cmd)
{
	CMD_HELP(cmd, _UTHREAD_USAGE,
		"Display relevant information for each entry in uthread_list. If "
		"no entries are specified, display information for all active "
		"uthreads. When the -S command line option is specified, displays "
		"a list of all siblings for the specified uthread. Entries in "
		"uthread_list can take the form of a process PID (following a '#'), "
		"or virtual address. Note that with the PID option, all uthreads "
		"that share a specified PID will be displayed. When the -k command "
		"line option is specified, display all uthread_s structs as "
		"kthread structs. When the -l option is used, only uthreads contributing "
		"to the load average are shown.\n");
}

/*
 * uthread_parse() -- Parse the command line arguments for 'uthread'.
 */
int
uthread_parse(command_t cmd)
{
	return (C_MAYBE|C_FULL|C_NEXT|C_KTHREAD|C_SIBLING|C_WRITE|C_LIST);
}
