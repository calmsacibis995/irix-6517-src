/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systeminfo.h>
#include <sys/prctl.h>
#include <sys/syssgi.h> /* for MAXSYSIDSIZE */
#include <sys/immu.h> /* for PRDAADDR  */

/* NOTE: we want as much static stuff in here as possible, so that it's
 * not so trivial for people to see what we are doing, and hack the
 * kernel in memory to circumvent it.  Use somewhat innocuous names
 * so as to not immediately clue hackers in on what's going on.
 * Basicly we are putting the systemid in prda, so programs can check it
 * whenever they want.
 *
 * otherprda is called once at boot from the end of mlsetup
 * with NULL, and we initialize then (same as SGI_MODULE_INFO in
 * syssgi().  The other times it is
 * a pointer to the prda, from lockprda().  We do *NOT* fill it in
 * for a simple faulting in of the prda, and we require the lockprda()
 * to make the scheme work (typically via schedctl(SETHINTS) or
 * syssgi(SGI_PROCMASK_LOCATION)).  If those syscalls are
 * is intercepted, then the systemid field will always be 0,
 * and the program can react appropriately.  The program should not 
 * use the prda address returned by the SETHINTS, just in case the
 * bad guy's program changes that return address to some other "safe"
 * page for them to modify.
 *
 * At various points (currently only from kt_startthread() other_prda is
 * called and if the systemid in the prda seems to have been tampered with,
 * we set it to all ff's, so the program can detect that somebody is messing
 * with it (probably via /proc), and do something appropriate.
 * 
 * For this first time, we put only the
 * "main" systemid (IP27 issue only) in the prda, eventually we
 * put the first 8 into the prda, which takes us up to (nominally, at 8
 * cpu's/module) 64 cpu's.
 * 
 * This all grew out of the 'runner' program that circumvents the syssgi call
 * to get the system serial number by running the app  under /proc, and
 * checking for the SYSID subcall, and returning an id passed on the
 * command line (so one license could be used on many machines).
 * This will be used by A|W only initially, and therefore isn't documented
 * at this time.  It will also go into 6.2 and 6.3 patches at some point.
 * deliberately has no prototypes in any header files.
 * for systems like IP27, returns only the new-style system id's.
 *
 * We "know" that all of our sysid's are at most 4 bytes long, and that
 * the alignment is correct, so we do this as a single uint32_t.
 * Since this can get called a lot, it has to be pretty fast, and avoid
 * cache misses where possible.
 *
 * See bug #422425.
 * Dave Olson, 1/97.
*/

extern int get_host_id(char *, int *, char *);
extern int get_kmod_info(int , module_info_t *);
static uint32_t i;

void
other_prda(struct prda *pr)
{
	/* keep rewriting to -1 once hacked, this could happen perhaps
	 * after prda setup and before otherprda() called from lockprda(),
	 * but that's OK, because the app can't run in between. */
	if(i != pr->sys2_prda.rsvd[0])
		pr->sys2_prda.rsvd[0] = (uint32_t)-1;
}

void
otherprda(struct prda *pr)
{
	if(pr)	/* init for this process */
		pr->sys2_prda.rsvd[0] = i;
	else {
	/* this code is pretty much a copy of SGI_MODULE_INFO from
	 * syssgi.c, except that it does apply to IP20, since we are
	 * doing the system id * here, not the serial number, unlike
	 * that function */
#ifdef SN0
		module_info_t 	m_info;

		if(!get_kmod_info(0, &m_info))
			i = m_info.serial_num;
#else /* !SN0 */
		int s_number;
		char hostident[MAXSYSIDSIZE];

		if(!get_host_id(hostident, &s_number, NULL))
			i = s_number;
#endif /* !SN0 */
	}
}
