#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_defkthread.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <errno.h>
#include "icrash.h"
#include "extern.h"

/*
 * defkthread_cmd() -- Run the 'defkthread' command.
 */
int
defkthread_cmd(command_t cmd)
{
	int mode;
	kaddr_t kthread;
	k_ptr_t ktp;

	if (cmd.nargs == 0) {
		if (!K_DEFKTHREAD(K)) {
			fprintf(cmd.ofp, "No default kthreadset\n");
		} 
		else {
			fprintf(cmd.ofp, "Default kthread is 0x%llx\n", K_DEFKTHREAD(K));
		}
		return(0);
	}

	get_value(cmd.args[0], &mode, 0, &kthread);
	if (KL_ERROR || (mode != 2)) {
		KL_ERROR |= KLE_BAD_KTHREAD;
		kl_print_error(K);
		fprintf(cmd.ofp, "No default kthread set\n");
		K_DEFKTHREAD(K) = 0;
		return(1);
	}

	/* Check and make sure that the kthread address is valid
	 */
	kl_is_valid_kaddr(K, kthread, (k_ptr_t*)NULL, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		KL_SET_ERROR_NVAL(KLE_BAD_KTHREAD, kthread, 2);
		kl_print_error(K);
		fprintf(cmd.ofp, "No default kthread set\n");
		K_DEFKTHREAD(K) = 0;
		return(1);
	}

	/* Now make sure this is a valid kthread
	 */
	ktp = kl_get_kthread(K, kthread, 0);
	if (KL_ERROR) {
		if (KL_ERROR == KLE_BAD_KTHREAD) {

			int kthread_cnt;
			k_ptr_t p;

			/* It's possible that the address contained in kthread is a
			 * pointer to a proc struct. The proc struct no longer 
			 * contains a kthread struct. Instead, it contains a 
			 * proc_proxy_s struct (located first in the proc struct) 
			 * which does contain a pointer to a linked list of associated
			 * uthread_s/kthread sturct(s). Lets try out the address as a 
			 * proc struct pointer and see if it works. If it does, 
			 * determine what its kthread pointer is and set defkthread 
			 * to that. Note that if there is more than one uthread
			 * associated with this proc, we will not be able to determine
			 * which one the user realy wants. In this case, diaplay a 
			 * list of the associated uthreads and return with defkthread
			 * unchanged.
			 */
			kl_reset_error();
			p = kl_get_proc(K, kthread, 2, 0);
			if (!KL_ERROR) {
				kthread = kl_proc_to_kthread(K, p, &kthread_cnt);
				if (kthread_cnt > 1) {
					fprintf(cmd.ofp, "0x%llx is a valid proc pointer. "
						"However, it has more than one uthread associated "
						"with it.\n", kthread);

					/* XXX - need to print listing of associated uthreads.
					 */

					fprintf(cmd.ofp, "No default kthread set\n");
					K_DEFKTHREAD(K) = 0;
					return (1);
				}
				ktp = kl_get_kthread(K, kthread, 0);
			}
		}
		if (KL_ERROR) {
			KL_ERROR = KLE_BAD_KTHREAD;
			kl_print_error(K);
			fprintf(cmd.ofp, "No default kthread set\n");
			K_DEFKTHREAD(K) = 0;
			return (1);
		}
	}

	/* Set defkthread 
	 */
	K_DEFKTHREAD(K) = kthread;
	free_block(ktp);
	fprintf(cmd.ofp, "Default kthread is 0x%llx\n", K_DEFKTHREAD(K));
	return(0);
}

#define _DEFKTHREAD_USAGE "[-w outfile] [kthread]"

/*
 * defkthread_usage() -- Print the usage string for the 'defkthread' command.
 */
void
defkthread_usage(command_t cmd)
{
	CMD_USAGE(cmd, _DEFKTHREAD_USAGE);
}

/*
 * defkthread_help() -- Print the help information for the 'defkthread' command.
 */
void
defkthread_help(command_t cmd)
{
	CMD_HELP(cmd, _DEFKTHREAD_USAGE,
		"Set the default kthread if if one is indicated.  Otherwise, "
		"display the current value of defkthread. When 'icrash' is run "
		"on a system core dump, defkthread gets set automatically "
		"to the kthread that was active when the system PANIC occurred."
		"When 'icrash' is run on a live system, defkthread is not set "
		"by default.\n\n" 
		
		"The defkthread value is used by 'icrash' in a number of ways. "
		"The trace command will display a trace for the default "
		"kthread if one is set. Also, the translation of certain kernel "
		"virtual addresses (primarily those contained in the kernelstack) "
		"depend upon defkthread being set.\n\n"

		"Note that if the addressed passed as a paramater to defkthread is "
		"a pointer to a proc struct, defkthread is set equal to the pointer "
		"to the kthread controling that proc.");
}

/*
 * defkthread_parse() -- Parse the command line arguments for 'defkthread'.
 */
int
defkthread_parse(command_t cmd)
{
	return (C_MAYBE|C_WRITE);
}
