/*
* sbe.c
*
* Single bit error code for MC3 systems.  This permits us to go through
* each MC3 board on an IP19/21/25 system and look for any particular
* single bit errors that might be logged.  Some of the comments below
* come directly from Alex Petruncola's brief document on single bit
* error logging:
*
* 'icrash' reads through the mc3_errcount array in order to dump out
* single bit error statistics.  Look in irix/kern/ml/EVEREST/everror.c
* to find this variable declaration.  Inside of it, you'll find that
* each bank maintains its own error count, including simm counts inside
* of it.
*
* Note that we do *not* look at any everror_t or everror_ext_t data,
* so we can see what is going on while the system is active.
*
* Alex's information:
*
* By default, the kernel has logging turned off.  There are two
* controls of interest to customer systems.  Each is manipulated with
* 'systune' utility.
* 
* 1. sbe_log_errors, which enables SYSLOG messages on single bit
*    correction occurance.
* 
* 2. sbe_mfr_override, which causes each event to get a log message.
*    Otherwise the occurance of a second correctable on the same bank
*    will disable further reports for 1 hour, in order to avoid flooding
*    the log.  The disable is only for reporting on the particular bank,
*    other banks will continue to make 2 reports before also being
*    disabled for an hour.
* 
* To enable error logging, login as root, and
* 
*     # systune -i
*     Updates will be made to running system and /unix.install
*
*     systune-> sbe_log_errors = 1
*             sbe_log_errors = 0 (0x0)
*             Do you really want to change sbe_log_errors to 1 (0x1)? (y/n)  y
*
*     systune-> quit
* 
* To disable error logging, enter systune, but instead of entering:
*
*     systune-> sbe_log_errors = 1
*
* instead, enter:
*
*     systune-> sbe_log_errors = 0
* 
* The kernel is immediately updated, and a new /unix.install is created
* so that a future reboot will continue to have the new setting.
* 
* 
* WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
* 
* A memory board which has recoverable (single bit) errors is NOT
* considered defective, and will not be replaced.  The purpose of this
* information is in evaluating certain memory problems, and does not in
* itself indicate there is any failure.  The occurance of single bit
* correctable messages DOES NOT identify a board requiring repairs.
* 
* WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
* 
* 
* There are two classes of memory boards, which I will call "old" and
* "new".  A "new" board is considered to be one whose MC3_REVLEVEL is
* set to 1 and whose MC3_BISTRESULT value has bits 16 & 17 set.  The SW
* refers to this as "rev 5".  
* 
* Older revision MC3s cannot report correctable errors.  There is no
* update or jumper change which can be made to an older MC3.  Some
* field people are aware of a jumper on older MC3s which disabled
* correctable error reporting.  --> This jumper may NOT be altered or
* data corruption will result.  The customer will experience corrupted
* data files and panics and hangs.  Absolutely no jumper changes may be
* made. <--
* 
* Older revision MC3s do single bit error correcting without making any
* report to software.  This is an intended design feature of those
* boards.
* 
* Default IRIX operation:
* 
* When an SBE is reported, an error count is incremented.  Individual
* error counts are maintained for each bank of each memory board.  No
* messages are ever logged for SBEs but the counts can be examined at
* any time by a command (TBD).
* 
* In addition, when an SBE occurs, the kernel leaves SBEs disabled for
* a while.  If this is the first error on this bank, then it is left
* disabled for 1 second.  If more than one error has been seen, it is
* left disabled for 60 minutes (this behaviour can be overriden by
* setting the systuneable paramter "sbe_mfr_override", in which case it
* is always left disabled for only one second).
* 
* In addition, the code will attempt to recover the SBE data by
* determining if the data error was "transient" (i.e. flushing the
* cache and re-reading did not result in a subsequent error) or whether
* "scrubbing" is required (i.e. flushes the cache, dirties the line,
* flushes the cache and then determines if the error recurrs).
* 
* Other IRIX operation:
* 
* By setting the systunable parameter sbe_log_errors, additional
* information is logged to the SYSLOG whenver an SBE is reported.  If
* sbe_report_cons is also set, then the information is also reported to
* the console.
* 
* The recovery algorithm mentioned above is still performed in exactly
* the same sequence.  The only difference is that the type of error
* ("transient" or "scrubbed") is also reported.  And the algorithm
* mentioned above for disabling subsequent SBE interrupts is also the
* same.
*
* MC3 SIMM labeling:
*
*                             TOP VIEW
*
*                     EBus midplane connector
* =================================================================
*  +---------+  +---------+  +---------+  +---------+  +---------+
*  |         |  |         |  |         |  |         |  |         |
*  |   MD    |  |   MD    |  |   MA    |  |   MD    |  |   MD    |
*  |         |  |         |  |         |  |         |  |         |
*  +---------+  +---------+  +---------+  +---------+  +---------+
*
*  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
*  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCC  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
*  EEEEEEEEEEEEEEEEEEEEEEEEEEEEEE  EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
*  GGGGGGGGGGGGGGGGGGGGGGGGGGGGGG  GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
*
*  BBBBBBBBBBBBBBBBBBBBBBBBBBBBBB  BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
*  DDDDDDDDDDDDDDDDDDDDDDDDDDDDDD  DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
*  FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF  FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
*  HHHHHHHHHHHHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
*
*  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA  AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
*  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCC  CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
*  EEEEEEEEEEEEEEEEEEEEEEEEEEEEEE  EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE
*  GGGGGGGGGGGGGGGGGGGGGGGGGGGGGG  GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG
*
*  BBBBBBBBBBBBBBBBBBBBBBBBBBBBBB  BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
*  DDDDDDDDDDDDDDDDDDDDDDDDDDDDDD  DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
*  FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF  FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
*  HHHHHHHHHHHHHHHHHHHHHHHHHHHHHH  HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
*
* Rows of letters are SIMMs.  They're lettered on the MC3 board.
* The letters A,C,E,G are leaf 0, and letters B,D,F,H are leaf 1.
*
* "bank n" numbers the banks 0..7 in the order ACEGBDFH.
*
* The format for 'icrash' messages if a bank/simm error is found
* will be:
*
* MC3 In Slot %d:
*     Bank %d: Error Count: %d
*              First Error Time: %s
*              Last Error Time: %s
*              Last Log Time: %s
*
*              Simm %d: Error Count %d\n",
*
* The error log times will be printed out in ctime() format.  The
* rest is obvious.
* ---------------------------------------------------------------------------
* ORIGIN SYSTEMS SBE COUNTS (from Curt McDowell):
*
* Each node has an sbe_info structure pointed to by the nodepda.  The
* sbe_info_t structure contains
*
*        disabled        True if node monitoring has become disabled
*                        because there were too many errors in a bank.
*                        Statistics are no longer being gathered.
*
*        log_cnt         Number of valid entries in log array below.
*                        If log_cnt reaches the maximum SBE_EVENTS_PER_NODE
*                        then it is assumed many errors are accumulating
*                        rapidly in all different pages, probably a stuck
*                        data line, and disables monitoring for that bank
*                        after printing a warning.  Monitoring is disabled
*                        to avoid taking constant error interrupts.
*
*        log             List of SBE events that have not expired yet.
*                        Each entry has the pfn of the error and a repeat
*                        count.  If an error happens in the same page,
*                        the repeat count increments.  Entries that have
*                        not repeated within SBE_TIMEOUT (60 sec) are
*                        removed from the list.  Entries that repeat
*                        SBE_MAX_PER_PAGE times try to take the page out
*                        of service (good luck!)  Entries that repeat
*                        SBE_MAXOUT times per page cause bank monitoring
*                        to be disabled.
*
*        bank_cnt        Simple array of counts of SBE events per bank
*                        since reboot.
*/
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash/cmds/RCS/cmd_sbe.c,v 1.16 1999/05/25 19:21:38 tjm Exp $"
#ifndef _KERNEL
#define _KERNEL 1
#define _K64U64 1
#define _PAGESZ 16384
#include <sys/types.h>
#include <sys/immu.h>
#undef _KERNEL
#include <sys/EVEREST/IP19addrs.h>
#else
#include <sys/types.h>
#include <sys/cpu.h>
#endif /* _KERNEL */
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/gda.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <klib/klib.h>
#include "icrash.h"

/*
 * Included directly from the ml/SN0/memerror.c code found in the
 * latest 6.4 kernel roll-up patch.
 *
 */
#define MD_MEM_BANKS            8       /* 8 banks of memory max in M mode */
#define SBE_EVENTS_PER_NODE     16      /* Storage space allocated         */
#define SBE_TIMEOUT             60      /* Seconds                         */
#define SBE_MAXOUT              10      /* Max times for same page error   */
#define SBE_MAX_PER_PAGE        4       /* Errors before page removed      */
#define SBE_DISCARD_TIMEOUT     600     /* Time between discard attempts   */

#define SBE_EVENT_DIR           0x01    /* Flags                           */

#define SBE_MAX_INTR_PER_MIN    600

typedef struct sbe_event_s {
    pfn_t               pfn;            /* Page of error                   */
    int                 flags;
    int                 repcnt;
    time_t              expire;         /* Time until sbe is forgotten     */
} sbe_event_t;

typedef struct sbe_info_s {
    int                 disabled;       /* True if polling is disabled     */
    int                 log_cnt;        /* Num. log array entries in use   */
    sbe_event_t         log[SBE_EVENTS_PER_NODE];
    int                 bank_cnt[MD_MEM_BANKS];  /* Per-bank error count   */
    int                 intr_ct;        /* Safety threshold intr count   */
    time_t              intr_tm;        /* Safety threshold reset time   */
} sbe_info_t;

/*
 * challenge_sbe_cmd() -- Obtain single bit error output on Challenge systems.
 */
int
challenge_sbe_cmd(int flags, FILE *ofp)
{
	int merror = 0;
	evbrdinfo_t *eb;
	mc3error_t *mc3err;
	mc3_bank_err_t *bnk;
	struct syment *tsym;
	mc3_array_err_t mc3_errcount[EV_MAX_MC3S];
	uint i, j, k, slot, mbid, leaf_num, bank_num, simm_num;
	evcfginfo_t *ecbuf = (evcfginfo_t *)NULL;

	if ((tsym = kl_get_sym("sbe_log_errors", K_TEMP)) == 
				(struct syment *)NULL) {
		fprintf(ofp,
			"sbe: could not retrieve sbe_log_errors kernel value\n");
		return(1);
	}

	if (tsym->n_value == 0) {
		fprintf(ofp,
			"sbe: single bit error logging turned off (sbe_log_errors = 0)\n");
		kl_free_sym(tsym);
		return(1);
	}

	/* Get evconfig 
	 */
	ecbuf = (evcfginfo_t *)kl_alloc_block(sizeof(evcfginfo_t), K_TEMP);
	if (!kl_get_block((kaddr_t)EVCFGINFO, 
							sizeof(evcfginfo_t), ecbuf, "evcfg")) {
		fprintf(ofp, "sbe: could not read evconfiginfo structure\n");
		kl_free_block((k_ptr_t)ecbuf);
		kl_free_sym(tsym);
		return(1);
	}
	kl_free_sym(tsym);

	if (!(tsym = kl_get_sym("mc3_errcount", K_TEMP))) {
		fprintf(ofp,
			"sbe: could not retrieve mc3 bank/simm error count symbol\n");
		kl_free_block((k_ptr_t)ecbuf);
		return(1);
	}
	if (!(kl_get_block(tsym->n_value, sizeof(mc3_errcount),
				mc3_errcount, "mc3_errcount"))) {
		fprintf(ofp, "sbe: could not retrieve mc3 bank/simm error counts\n");
		kl_free_block((k_ptr_t)ecbuf);
		kl_free_sym(tsym);
		return(1);
	}
	kl_free_sym(tsym);
	for (slot = 1; slot < EV_MAX_SLOTS; slot++) {

		/* Scan through all boards, and for each one that is an MC3, check
		 * through all the leaves and see if there are memory errors logged.
		 * If so, report them.  In the cases where we have a partial write
		 * multiple bit error and a single bit read error, analyze the MC3
		 * single-bit error information and find out which bank/leaf/address
		 * we had the error at.
		 */
		eb = &(ecbuf->ecfg_board[slot]);
		if (eb->eb_type == EVTYPE_MC3) {

			/* Now go through all the memory banks and see if there are
			 * any error counts for each.
			 */
			merror = 0;
			mbid = eb->eb_mem.eb_mc3num;
			fprintf(ofp, "\nMC3 In Slot %d:\n", slot);
			if (mbid  && (mbid < EV_MAX_MC3S)) {
				if (mc3_errcount[mbid].m_unk_bank_errcount) {
					fprintf(ofp,
						"    MA Unresolved To Bank Error Count: %d\n",
						mc3_errcount[mbid].m_unk_bank_errcount);
					merror = 1;
				}
				for (j = 0; j < MC3_NUM_BANKS; j++) {
					bnk = &(mc3_errcount[mbid].m_bank_errinfo[j]);
					if (bnk->m_bank_errcount) {
						fprintf(ofp,
							"    Bank %d: Error Count: %d\n"
							"             First Error Time: %s\n"
							"             Last Error Time: %s\n"
							"             Last Log Time: %s\n",
							j, bnk->m_bank_errcount,
							ctime((time_t *)&(bnk->m_first_err_time)),
							ctime((time_t *)&(bnk->m_last_err_time)),
							ctime((time_t *)&(bnk->m_last_log_time)));
						merror = 1;
					}
					for (k = 0; k < MC3_SIMMS_PER_BANK; k++) {
						if (bnk->m_simm_errcount[k]) {
							fprintf(ofp,
								"\n             Simm %d: Error Count %d\n",
								k, bnk->m_simm_errcount[k]);
							merror = 1;
						}
					}
				}
			}
			if (!merror) {
				fprintf(ofp,
					"    No Bank, Simm, or Memory Errors Found "
					"For This Board\n");
			}
		}
	}
	kl_free_block((k_ptr_t)ecbuf);
	return(0);
}

/*
 * origin_sbe_cmd() -- Obtain single bit error output on Origin systems.
 */
int
origin_sbe_cmd(int flags, FILE *ofp)
{
	k_ptr_t npdap;
	int i, count = 0, node, bank, started = FALSE;
	kaddr_t value, nodepdaval, sbeinfoval;
	sbe_info_t *sbebuf;

	npdap = kl_alloc_block(NODEPDA_S_SIZE, K_TEMP);

	/*
	 * Now walk through all of the nodes, being sure to dump out any
	 * and all errors for all banks.  Be very specific as to which node
	 * has had its memory disabled.
	 */
	for (i = 0; i < K_NUMNODES; i++) {
		value = (K_NODEPDAINDR + (i * K_NBPW));
		kl_get_kaddr(value, &nodepdaval, "nodepda_s");
		if (KL_ERROR) {
			continue;
		}
		kl_get_struct(nodepdaval, NODEPDA_S_SIZE, npdap, "nodepda_s");
		if (KL_ERROR) {
			continue;
		}

		sbeinfoval = kl_kaddr(npdap, "nodepda_s", "sbe_info");
		if (sbeinfoval) {
			sbebuf = (sbe_info_t *)kl_alloc_block(sizeof(sbe_info_t), K_TEMP);
			if (!kl_get_block(sbeinfoval, sizeof(sbe_info_t),
				sbebuf, "sbe_info_t")) {
					continue;
			}
			for (bank = 0; bank < 8; bank++) {
				if (sbebuf->bank_cnt[bank] > 0) {
					if (started == FALSE) {
						if (report_flag) {
							fprintf(ofp, "\n");
						}
						fprintf(ofp,
							"NODE   BANK   # BANK ERRORS    DISABLED (Y/N)\n");
						fprintf(ofp,
							"=============================================\n");
						started = TRUE;
					}
					fprintf(ofp, "%4d   %4d    %12d    %s\n",
						i, bank, sbebuf->bank_cnt[bank],
							(sbebuf->disabled ?
								"             Y" : "             N"));
					count++;
				}
			}
		}
	}
	if (started == FALSE) {
		if (report_flag) {
			fprintf(ofp,
				"\nNo single-bit errors found on any node\n");
		} else {
			fprintf(ofp,
				"sbe: No single-bit errors found for any node\n");
		}
	} else {
		fprintf(ofp,
			"=============================================\n"
			"%d node%s with sbe errors found\n",
			count, (count != 1) ? "s" : "");
	}
	return(0);
}

/*
 * sbe_cmd() -- Run the 'sbe' command.
 */
int
sbe_cmd(command_t cmd)
{

	if ((K_IP == 19) || (K_IP == 21) || (K_IP == 25)) {
		return(challenge_sbe_cmd(cmd.flags, cmd.ofp));
	} else if ((K_IP == 27) || (K_IP == 29)) {
		return(origin_sbe_cmd(cmd.flags, cmd.ofp));
	}

	fprintf(cmd.ofp,
		"sbe: this command is only valid on Challenge and Origin systems\n");
	return (1);
}

#define _SBE_USAGE "[-w outfile]"

/*
 * sbe_usage() -- Print the usage string for the 'sbe' command.
 */
void
sbe_usage(command_t cmd)
{
	CMD_USAGE(cmd, _SBE_USAGE);
}

/*
 * sbe_help() -- Print the help information for the 'sbe' command.
 */
void
sbe_help(command_t cmd)
{
	CMD_HELP(cmd, _SBE_USAGE,
		"Print out the single bit error information for Challenge or Origin "
		"systems.  If the flags for single bit error logging in the kernel "
		"are not turned on, the command will return no results.");
}

/*
 * sbe_parse() -- Parse the command line arguments for 'sbe'.
 */
int
sbe_parse(command_t cmd)
{
	return (C_FALSE|C_WRITE);
}
