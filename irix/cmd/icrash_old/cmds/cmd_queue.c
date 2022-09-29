#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/cmds/RCS/cmd_queue.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include "icrash.h"
#include "stream.h"
#include "extern.h"

/* Local variables
 */
extern stream_rec_t strmtab[];
extern int strmtab_valid;
extern int strcnt;

/*
 * queue_banner()
 */
void
queue_banner(FILE *ofp, int flags)
{
	if (flags & BANNER) {
		indent_it(flags, ofp);
		if (PTRSZ64(K)) {
			fprintf(ofp, "           QUEUE  COUNT             FIRST  "
				"            LAST  NAME\n");
		}
		else {
			fprintf(ofp, "   QUEUE          COUNT             FIRST  "
				"            LAST  NAME\n");
		}
	}

	if (flags & SMAJOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "======================================================="
			"======================\n");
	}

	if (flags & SMINOR) {
		indent_it(flags, ofp);
		fprintf(ofp, "-------------------------------------------------------"
			"----------------------\n");
	}
}

/*
 * print_queue() -- Print out queue specific data.
 */
void
print_queue(kaddr_t q, k_ptr_t qp, char *mname, int flags, FILE *ofp)
{
	indent_it(flags, ofp)
	FPRINTF_KADDR(ofp, "  ", q);
	fprintf(ofp, "%5llu  ", KL_UINT(K, qp, "queue", "q_count"));
	fprintf(ofp, "%16llx  ", kl_kaddr(K, qp, "queue", "q_first"));
	fprintf(ofp, "%16llx  ", kl_kaddr(K, qp, "queue", "q_last"));
	fprintf(ofp, "%-15s\n", mname);
	
	if (flags & C_FULL) {
		fprintf(ofp, "\n");
		indent_it(flags, ofp)
		fprintf(ofp, "Q_QINFO=0x%llx, ", kl_kaddr(K, qp, "queue", "q_qinfo"));
		fprintf(ofp, "Q_PTR=0x%llx\n", kl_kaddr(K, qp, "queue", "q_ptr"));
		fprintf(ofp, "Q_NEXT=0x%llx, ", kl_kaddr(K, qp, "queue", "q_next"));
		fprintf(ofp, "Q_LINK=0x%llx\n", kl_kaddr(K, qp, "queue", "q_link"));
		indent_it(flags, ofp)
		fprintf(ofp, "Q_MINPSZ=%lld, ", KL_INT(K, qp, "queue", "q_minpsz"));
		fprintf(ofp, "Q_MAXPSZ=%lld, ", KL_INT(K, qp, "queue", "q_maxpsz"));
		fprintf(ofp, "Q_LOWAT=%llu, ", KL_UINT(K, qp, "queue", "q_lowat"));
		fprintf(ofp, "Q_HIWAT=%llu\n", KL_UINT(K, qp, "queue", "q_hiwat"));
		indent_it(flags, ofp)
		fprintf(ofp, "Q_BANDP=0x%llx, ", kl_kaddr(K, qp, "queue", "q_bandp"));
		fprintf(ofp, "Q_NBAND=%llu, ", KL_UINT(K, qp, "queue", "q_nband"));
		fprintf(ofp, "Q_BLOCKED=%llu\n", KL_UINT(K, qp, "queue", "q_blocked"));
		fprintf(ofp, "\n");
	}
}

#ifdef NOTYET
/*
 * print_stty_ld()
 */
void
print_stty_ld(struct stty_ld *stp, int flags, FILE *ofp)
{
	int i;
	struct stty_ld stbuf;

	GET_STTY_LD(stp, &stbuf);

	fprintf(ofp, "\n  Private stty_ld data structure\n\n");

	if (flags & C_FULL) {
		print_termio(&stbuf.st_termio, flags, ofp);
	}
	fprintf(ofp, "  RQ = 0x%x, WQ = 0x%x, STATE = %x\n", 
		stbuf.st_rq, stbuf.st_wq, stbuf.st_state);
	fprintf(ofp, "  LLENGTH = %d, OCOL = %d, ECOL = %d, BCOL = %d\n\n", 
		stbuf.st_llen, stbuf.st_ocol, 
		stbuf.st_xcol, stbuf.st_bcol);
	fprintf(ofp, "  IMSG = 0x%x, IBP = 0x%x\n", 
		stbuf.st_imsg, stbuf.st_ibp);

	if (stbuf.st_imsg) {
		do_msg(stbuf.st_imsg, C_NEXT|C_TOTALS, ofp);
	}
	fprintf(ofp, "  LMSG = 0x%x, LBP = 0x%x\n", 
		stbuf.st_lmsg, stbuf.st_lbp);

	if (stbuf.st_lmsg) {
		do_msg(stbuf.st_lmsg, C_NEXT|C_TOTALS, ofp);
	}
	fprintf(ofp, "  EMSG = 0x%x, EBP = 0x%x\n", 
		stbuf.st_emsg, stbuf.st_ebp);

	if (stbuf.st_emsg) {
		do_msg(stbuf.st_emsg, C_NEXT|C_TOTALS, ofp);
	}
}

/*
 * print_termio()
 */
void
print_termio(struct termio *tp, int flags, FILE *ofp)
{
	int i, j;

	fprintf(ofp, "   TERMIO : IFLAG  OFLAG  CFLAG  LFLAG   LINE\n");
	fprintf(ofp, "            %5d  %5d  %5d  %5d  %5d\n", tp->c_iflag,
		tp->c_oflag, tp->c_cflag, tp->c_lflag, tp->c_line);
	fprintf(ofp, "\n       CC : ");

	for (i = 0, j = 0; i < 23; i++) {
		fprintf(ofp, "%02x  ", tp->c_cc[i]);
		if (i == 11) {
			fprintf(ofp, "\n            ");
		}
	}
	fprintf(ofp, "\n\n");
}
#endif /* NOTYET */

/*
 * do_queues() -- Dump out data on a queue (qinit routines, etc.)
 */
int
do_queues(kaddr_t q, int flags, FILE *ofp)
{
	int first_time = 1;
	kaddr_t nq, oq;
	k_ptr_t qp, oqp, qip, oqip, mip, omip;
	char *mname, *omname;

	qp = alloc_block(QUEUE_SIZE(K), B_TEMP);
	qip = alloc_block(QINIT_SIZE(K), B_TEMP);
	mip = alloc_block(MODULE_INFO_SIZE(K), B_TEMP);
	mname = (char *)alloc_block(128, B_TEMP);

	nq = q;
	while (nq) {
		kl_get_struct(K, nq, QUEUE_SIZE(K), qp, "queue");
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_QUEUE;
			kl_print_error(K);
			free_block(qp);
			free_block(qip);
			free_block(mip);
			free_block((k_ptr_t)mname);
			return(-1);
		}

		kl_get_struct(K, kl_kaddr(K, qp, "queue", "q_qinfo"), 
			QINIT_SIZE(K), qip, "qinit");
		kl_get_struct(K, kl_kaddr(K, qip, "qinit", 
			"qi_minfo"), MODULE_INFO_SIZE(K), mip, "module_info");
		kl_get_block(K, kl_kaddr(K, mip, "module_info", 
			"mi_idname"), 128, mname, "mname");

		if (first_time || (flags & C_FULL)) {
			queue_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
			if (first_time) {
				first_time = 0;
			}
		}
		print_queue(nq, qp, mname, (flags & C_INDENT), ofp);

		if (flags & C_NEXT) {

			oqp = alloc_block(QUEUE_SIZE(K), B_TEMP);

			/* Get the print the other queue 
			 */
			oq = _OTHERQ(nq, qp);
			if (!kl_get_struct(K, oq, QUEUE_SIZE(K), oqp, "queue")) {
				if (DEBUG(DC_GLOBAL, 1)) {
					fprintf(ofp, "0x%llx does not point to a valid queue!\n", 
						_OTHERQ(nq, qp));
				}
				free_block(qp);
				free_block(qip);
				free_block(mip);
				free_block((k_ptr_t)mname);
				free_block(oqp);
				return(-1);
			}
			oqip = alloc_block(QINIT_SIZE(K), B_TEMP);
			omip = alloc_block(MODULE_INFO_SIZE(K), B_TEMP);
			omname = (char *)alloc_block(128, B_TEMP);

			kl_get_struct(K, kl_kaddr(K, oqp, "queue", "q_qinfo"), QINIT_SIZE(K), 
				oqip, "qinit");
			kl_get_struct(K, kl_kaddr(K, oqip, "qinit", "qi_minfo"), 
				MODULE_INFO_SIZE(K), omip, "module_info");
			kl_get_block(K, kl_kaddr(K, omip, "module_info", "mi_idname"), 128,
				omname, "mname");
			if (flags & C_FULL) {
				queue_banner(ofp, (flags & C_INDENT)|BANNER|SMINOR);
			}
			print_queue(oq, oqp, omname, flags, ofp);
			free_block(oqp);
			free_block(oqip);
			free_block(omip);
			free_block((k_ptr_t)omname);
		}

#ifdef NOTYET
		/* Print the module private data structure if this queue
		 * pair does not belong to the stream head. Both queues in
		 * the module should point to the same data structure.
		 */
		if (flags & C_FULL) {
			if (strcmp(mname, "STTY_LD") == 0) {
				print_stty_ld(qbuf.q_ptr, flags, ofp);
				if ((flags & C_NEXT) && (qbuf.q_ptr != oqbuf.q_ptr)) {
					print_stty_ld(oqbuf.q_ptr, flags, ofp);
				}
			}
		}
#endif /* NOTYET */
		nq = kl_kaddr(K, qp, "queue", "q_next");
		if (!(flags & C_FULL)) {
			queue_banner(ofp, (flags & C_INDENT)|SMINOR);
		}
		if (!(flags & C_NEXT)) {
			free_block(qp);
			free_block(qip);
			free_block(mip);
			free_block((k_ptr_t)mname);
			return(0);
		}
	}
	fprintf(ofp, "\n");
	free_block(qp);
	free_block(qip);
	free_block(mip);
	free_block((k_ptr_t)mname);
	return(0);
}

/*
 * queue_cmd() -- Perform the 'queue' command on a certain address.
 */
int
queue_cmd(command_t cmd)
{
	int i, queue_cnt;
	kaddr_t q;

	for (i = 0; i < cmd.nargs; i++) {
		fprintf(cmd.ofp, "\n");
		GET_VALUE(cmd.args[i], &q);
		if (KL_ERROR) {
			KL_ERROR |= KLE_BAD_QUEUE;
			kl_print_error(K);
			continue;
		}
		do_queues(q, cmd.flags, cmd.ofp);
	}
	return(0);
}

#define _QUEUE_USAGE "[-f] [-n] [-w outfile] queue_list"

/*
 * queue_usage() -- Print the usage string for the 'queue' command.
 */
void
queue_usage(command_t cmd)
{
	CMD_USAGE(cmd, _QUEUE_USAGE);
}

/*
 * queue_help() -- Print the help information for the 'queue' command.
 */
void
queue_help(command_t cmd)
{
	CMD_HELP(cmd, _QUEUE_USAGE,
		"Display the queue structure located at each virtual address "
		"included in queue_list.  If the next option (-n) is specified, "
		"a linked list of queues, starting with each specified queue "
		"then following the q_next field, will be displayed.");
}

/*
 * queue_parse() -- Parse the command line arguments for 'queue'.
 */
int
queue_parse(command_t cmd)
{
	return (C_TRUE|C_WRITE|C_FULL|C_NEXT);
}
