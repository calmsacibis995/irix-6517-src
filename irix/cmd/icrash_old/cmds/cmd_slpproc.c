#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_slpproc.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include "icrash.h"
#include "extern.h"

/*
 * slpproc_banner()
 */
void
slpproc_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		if (PTRSZ64(K)) {
			fprintf(ofp,
				"            PROC   STAT             WCHAN            W2CHAN  "
				"NAME\n");
		}
		else {
		}
	}

	if (flags & SMAJOR) {
		fprintf(ofp,
			"======================================================"
			"========================\n");
	}

	if (flags & SMINOR) {
		fprintf(ofp,
			"------------------------------------------------------"
			"------------------------\n");
	}
}

/*
 * print_slpproc() -- Print out sleeping process information.
 */
int
print_slpproc(kaddr_t proc, k_ptr_t procp, int flags, FILE *ofp)
{
	kaddr_t userp;
	k_ptr_t up = (k_ptr_t)NULL, cp, kt_name;

	if (DEBUG(DC_GLOBAL, 3)) {
		fprintf(ofp, "print_proc: proc=0x%llx, procp=0x%x, flags=0x%x\n", 
			proc, procp, flags);
	}

	switch (KL_INT(K, procp, "proc", "p_stat")) {
#ifdef NOT
		case SSLEEP:
			fprintf(ofp, "%16llx SSLEEP  ", proc);
			break;
		case SXBRK:
			fprintf(ofp, "%16llx  SXBRK  ", proc);
			break;
#endif
		default:
			return(-1);
	}
	if (kl_kaddr(K, procp, "kthread", "k_wchan")) {
		fprintf(ofp, "%16llx  ", kl_kaddr(K, procp, "kthread", "k_wchan"));
	} 
	else {
		fprintf(ofp, "%16s  ", " ");
	}

	if (kl_kaddr(K, procp, "kthread", "k_w2chan")) {
		fprintf(ofp, "%16llx  ", kl_kaddr(K, procp, "kthread", "k_w2chan"));
	} 
	else {
		fprintf(ofp, "%16s  ", " ");
	}

	fprintf(ofp, "%s\n", kl_kthread_name(K, procp));
	return(0);
}

/*
 * slpproc_cmd() -- Dump out sleeping process information.
 */
int
slpproc_cmd(command_t cmd)
{
	int i, mode, slot, first_time = TRUE, slpproc_cnt = 0;
	k_ptr_t procp;
	kaddr_t value, proc;
	char statval;

	for (i = 0; i < cmd.nargs; i++) {
		get_value(cmd.args[i], &mode, 0, &value);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_KTHREAD;
			kl_print_error(K);
			continue;
		}
		if (mode == 0) {
			KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, value, mode);
			kl_print_error(K);
			continue;
		}
		if (mode == 1) {
			proc = kl_pid_to_proc(K, value);
		}
		else {
			proc = value;
		}

		procp = kl_get_kthread(K, proc, cmd.flags);
		if (KL_ERROR) {
			kl_print_error(K);
		}
		else {
			statval = KL_INT(K, procp, "proc", "p_stat");
#ifdef NOT
			if ((statval == SSLEEP) || (statval == SXBRK)) {
				if (first_time || (cmd.flags & C_FULL)) {
					if (first_time) {
						first_time = FALSE;
					}
					slpproc_banner(cmd.ofp, BANNER|SMAJOR);
				}
				print_slpproc(slot, procp, cmd.flags, cmd.ofp);
				slpproc_cnt++;
			}
#endif
			free_block(procp);
		} 
	}
	if (cmd.nargs == 0) {
#ifdef XXX
        for (i = 0; i < pacthashmask; i++) {
            kl_get_kaddr(K, _pacthashtab + (i * nbpw), (k_ptr_t)&proc, "proc");
            if (KL_ERROR || !proc) {
                continue;
            }
            while (proc) {
                procp = kl_get_proc(K, proc, 2, cmd.flags);
                if (KL_ERROR) {
                    if (DEBUG(DC_GLOBAL, 1)) {
                        kl_print_debug(K, "proc_cmd");
                    }
                    break;
                }
				statval = KL_INT(K, procp, "proc", "p_stat");
#ifdef NOT
				if ((statval == SSLEEP) || (statval == SXBRK)) {
					if (first_time || (cmd.flags & C_FULL)) {
						if (first_time) {
							first_time = FALSE;
						}
						slpproc_banner(cmd.ofp, BANNER|SMAJOR);
					}
					print_slpproc(proc, procp, cmd.flags, cmd.ofp);
					slpproc_cnt++;
				}
#endif
				proc = kl_kaddr(K, procp,  "proc", "p_active");
				free_block(procp);
            }
        }
#endif
	}
	slpproc_banner(cmd.ofp, SMAJOR);
	fprintf(cmd.ofp, "%d sleeping process%s found\n", 
		slpproc_cnt, (slpproc_cnt != 1) ? "es" : "");
	return(0);
}

#define _SLPPROC_USAGE "[-w outfile]"

/*
 * slpproc_usage() -- Print the usage string for the 'slpproc' command.
 */
void
slpproc_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SLPPROC_USAGE);
}

/*
 * slpproc_help() -- Print the help information for the 'slpproc' command.
 */
void
slpproc_help(command_t cmd)
{
	CMD_HELP(cmd, _SLPPROC_USAGE,
		"Print out the set of sleeping processes on the system.");
}

/*
 * slpproc_parse() -- Parse the command line arguments for 'slpproc'.
 */
int
slpproc_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE|C_FULL);
}
