#ifdef SPLMETER
/*
 * codes to collect data for spl metering
 */
#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/timers.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/splmeter.h"
#include "sys/systm.h"
#include "sys/kmem.h"
#if EVEREST
#include <sys/EVEREST/everest.h>
#include "sys/EVEREST/evintr.h"
#endif
#include <sys/idbgentry.h>
#include <sys/debug.h>

#define MAXMETER	8	/* only deal with 8 */
#define SPLMAXCNT  4096

#ifdef BTRACE
#define STKFRAME	10
int btrace_flag;
#define BT_TICK	2000	/* want 1ms */
int bttick = BT_TICK;
#define STKMAX	SPLMAXCNT*50	/* stack as deep as 10 frames?? */
stkfr_t stkarray[STKMAX];
int stkmax = STKMAX;
stkfr_t *freestk_list;
#endif
#define CPUID() (splcounter_on ? cpuid() : getcpuid())
static void start_splcount(splmeter_t **, splmeter_t *, inst_t *, uint);

#define SPL1		1	/* spl1() */
#define SPLNET		2	/* splnet() */
#define SPLVME		3	/* splvme() */
#define SPLTTY		4	/* spl5(), spltty(), splimp() */
#define SPLHI		5	/* spl6(), splhi() */
#if !defined( EVEREST)
#define SPLPROF		6	/* spl65(), splprof() */
#endif /* !EVEREST */
#ifdef IP32			/* Avoid naming conflicts on IP32 */
#define SPL7_IP32	7	/* spl7() */
#else
#define SPL7		7	/* spl7() */
#endif /* IP32 */

#ifdef IP32			/* Correctly examine old spl on IP32 */
#define WASSPLHI(ospl)    (((ospl) & SR_IMASK) >= 1<<3+SR_IMASKSHIFT)
#define WASSPLPROF(ospl)  (((ospl) & SR_IMASK) >= 2<<3+SR_IMASKSHIFT)
#define WASSPL7(ospl)     (((ospl) & SR_IMASK) >= 3<<3+SR_IMASKSHIFT)
#endif

int spl_level = SPLHI;		/* the spl level being sought after */

splfunc_t splfn_array[8] = {
	splhi,
	splhi,
	splhi,
	splhi,
	splhi,
	splhi,
	splprof,
	spl7
};

splmeter_t *spl_array;
int	splmax[MAXMETER] = {
				SPLMAXCNT,
				SPLMAXCNT,
				SPLMAXCNT,
				SPLMAXCNT,
				SPLMAXCNT,
				SPLMAXCNT,
				SPLMAXCNT,
				SPLMAXCNT
			};
splmeter_t timerhead[MAXMETER];	/* has the list of all known timing entries */
splmeter_t *timeractive[MAXMETER];/* has the list of all known timing entries */
splmeter_t splhead[MAXMETER];	/* has the list of all known spl entries */
splmeter_t *splactive[MAXMETER];	/* has the list of all active spl entries */
uint spltick;
#define getspltick()	get_timestamp()
int vme_latency = 0;		/* set then calculate vme latency instead */
uint vme_latmax;		/* vme intr latency counter */
uint vme_latcur;		/* vme intr latency counter */
int splcounter_on;
#define CHECK_SPLMETERON()	\
if (splcounter_on == 0) \
	return;
int spl_debug;
int pc_rounding;
splmeter_t *remove_splactive(splmeter_t **, inst_t *);
splmeter_t *getspl(splmeter_t *, inst_t *);
uint stop_splcount(splmeter_t *spl, inst_t *pc, uint tick);
uint ospl_is_lower(unsigned ospl, unsigned target_spl);
uint ospl_is_higher(unsigned ospl, unsigned target_spl);
void cancelsplhi(inst_t *pc);
int onactivelist(splmeter_t **, splmeter_t *);

#define splprintf	if (spl_debug) dprintf

#define ustotick(x)	(((x) * 1000) / timer_unit)

void
splmeter_init(void)
{
	spl_array = kmem_zalloc(sizeof(splmeter_t)*MAXMETER*SPLMAXCNT,
								KM_SLEEP);
	ASSERT(spl_array);
	splcounter_on = 1;
}

void
reset_splmeter(void)
{
	bzero(spl_array, sizeof(splmeter_t)*MAXMETER*SPLMAXCNT);
	bzero(timerhead, sizeof timerhead);
	bzero(timeractive, sizeof timeractive);
	bzero(splhead, sizeof splhead);
	bzero(splactive, sizeof splactive);
}

/*
** start timer on the routine that this 'opc' belongs to
** call stoptimer to turn off the timer, keep the max time only
** operate out of a different list than the spl list
*/
void
_starttimer(inst_t *opc)
{
	splmeter_t *spl;
	int id = CPUID();

	CHECK_SPLMETERON();
	spl = getspl(&timerhead[id], opc);
	if (spl != 0) {
		/* use original pc not rounded pc */
		start_splcount(&timeractive[id], spl, opc, 0);
	}
#ifdef NOTDEF
	else
		qprintf("out of spl meter struct\n");
#endif
}

/*
** turn off the timer for the routine that this pc belongs to
**
*/
void
_stoptimer(inst_t *pc)
{
	splmeter_t *spl;
	uint tick = getspltick();
	int id = CPUID();


	CHECK_SPLMETERON();
	/* only stop the count for the one that initiates the splhi */
	/* it must be on the spl active list */
	spl = remove_splactive(&timeractive[id], pc);
	if (spl) {
		stop_splcount(spl, pc, tick);
	}
}

void
_canceltimer(inst_t *pc)
{
	volatile splmeter_t *spl = timeractive[CPUID()];
	uint tick = getspltick();


	CHECK_SPLMETERON();
	while (spl != 0) {
		if (spl->splcount != spl->spxcount) {
			stop_splcount((splmeter_t*)spl, pc, tick);
		}
		spl = spl->active;
	}
	/* zap the spl active list */
	timeractive[CPUID()] = 0;
}
/*
** collect data
** - routine that call splhi or spsemahi
** - current tick count
** - is the processor already at hi
** - stack backtrace up to splhi, not including splhi
** - expect the calling routine to raise spl already, only collect timing
**   data here
**
** ospl is expected to be meaningful only if called from splxxx in spl.s
*/
uint
raisespl(inst_t *opc, uint ospl)
{
	splmeter_t *spl;
	int id = CPUID();

	if (ospl_is_higher(ospl, spl_level) || !splcounter_on)
		return(ospl);
	spl = getspl(&splhead[id], opc);
	if (spl != 0) {
		/* use original pc not rounded pc */
		start_splcount(&splactive[id], spl, opc, ospl);
	}
#ifdef NOTDEF
	else
		qprintf("out of spl meter struct\n");
#endif
	return(ospl);
}

uint
raisespl_spl(inst_t *opc, uint ospl, splfunc_t splfn)
{
	if (splfn_array[spl_level] != splfn)
		return ospl;
	
	return raisespl(opc, ospl);
}

/*
** if the new spl is lower than hi then cancel all outstanding splhi counter
** if the new spl is also hi then only stop counting for the routine that
** does the splx
** restore the ospl by calling __splx
*/

void
lowerspl(
	int ospl,
	inst_t *pc)
{
	splmeter_t *spl;
	uint tick = getspltick();
	int id = CPUID();
	int dobt = 0;
#ifdef BTRACE
	caddr_t sp = 0;
#endif

	if (ospl_is_higher(ospl, spl_level) || !splcounter_on) {
		__splx(ospl);
		return;
	}	
	/* if the restored spl level is lower than the target spl level
	** then stop counting
	*/
	if (ospl_is_lower(ospl, spl_level))
		cancelsplhi(pc);
	else  {
		/* only stop the count for the one that initiate the splhi */
		/* it must be on the spl active list */
		spl = remove_splactive(&splactive[id], pc);
		if (spl) {
			dobt = stop_splcount(spl, pc, tick);
#ifdef BTRACE
			/* do btrace only if holding intr for more than 1ms
			** and surpass the tickmax for this spl entry
			*/
			if (btrace_flag && spl->tickmax > bttick && dobt) {
				/* these data are obsolete now */
				free_stktbl(&spl->lobt);
				/* create new stack btrace for splx */
				btrace((unsigned *)pc, (unsigned *)sp, 0,
					STKFRAME, UT_VEC, 0, SPL_BTRACE,
					&spl->lobt);
			}
#endif
		}
#ifdef NOTYET
#ifdef SPLDEBUG
		else {
			__psunsigned_t offst;
			splprintf("splx: <%s> not on active list\n",
				fetch_kname(rpc, &offst));
		}
#endif
#endif /* NOTYET */
	}
	__splx(ospl);
}


void
_spl0(inst_t *pc)
{
	if (!splcounter_on) {
		__spl0();
		return;
	}
	cancelsplhi(pc);
	__spl0();
}

void
suspendspl()
{
	volatile splmeter_t *spl = splactive[CPUID()];
	uint tick = getspltick();

	while (spl != 0) {
		if (spl->splcount != spl->spxcount)
			spl->suspendtick = tick;
		spl = spl->active;
	}
}

void
resumespl()
{
	volatile splmeter_t *spl = splactive[CPUID()];
	uint tick = getspltick();

	while (spl != 0) {
		if (spl->splcount != spl->spxcount && spl->suspendtick)
			spl->resumetick = tick;
		spl = spl->active;
	}
}

void
cancelsplhi(inst_t *pc)
{
	volatile splmeter_t *spl = splactive[CPUID()];
	uint tick = getspltick();
#ifdef BTRACE
	int *lobt;
	int didbt = 0;
#endif
	int dobt = 0;


	while (spl != 0) {
		if (spl->splcount != spl->spxcount) {
			dobt = stop_splcount((splmeter_t *)spl, pc, tick);
#ifdef BTRACE
			if (btrace_flag && spl->tickmax > bttick && dobt) {
				/*
				** backtrace for splx now, once only
				*/
				if (!didbt) {
					/* create new stack btrace for splx */
					lobt = 0;
					btrace((unsigned *)pc, (unsigned *)sp,
					0, STKFRAME, UT_VEC, 0, SPL_BTRACE,
							&lobt);
					didbt = 1;
					/* these data are obsolete now */
					free_stktbl(&spl->lobt);
					/* set new stack btrace for splx */
					spl->lobt = (stkfr_t *)lobt;
				}
				else {
					/* these data are obsolete now */
					free_stktbl(&spl->lobt);
					/* copy the previous btrace result */
					dupbt(&spl->lobt, lobt);
				}
			}
#endif
		}
		spl = spl->active;
	}
	/* zap the spl active list */
	splactive[CPUID()] = 0;
}

/*
** start counter for this spl entry
*/
static void
start_splcount(
	splmeter_t **active,
	splmeter_t *spl,
	inst_t *pc,
	uint ospl)
{
	__psunsigned_t offst;

	spl->splcount++;
#ifdef IP32
	if (WASSPLHI(ospl)) {
#else
	if (issplhi(ospl)) {
#endif /* IP32 */
		spl->ishicount++;
		spl->curishi = 1;
	}
	else
		spl->curishi = 0;
	/* add to the spl active list  */
	if ( !onactivelist(active, spl) ) {
		spl->active = *active;
		*active = spl;
	}
	else {
		splprintf("start_splcount: <%s> already on active list\n",
			fetch_kname(pc, &offst));
		if (spl_debug)
			debug(0);
	}
	spl->cpustart = CPUID();
	spl->cur_hipc = pc;
	/* do this last */
	spl->tickstart = getspltick();
	spl->suspendtick = spl->resumetick = 0;
}

int lat_stop = 0;


uint
stop_splcount(
	splmeter_t *spl,
	inst_t *pc,
	uint tick)
{
	uint ticklen;
	uint sticklen;

	spl->cpuend = CPUID();
	if (tick < spl->tickstart) 	/* wrap around */
		ticklen = ((unsigned int)(0xffffffff) -
			spl->tickstart) + tick;
	else
		ticklen = tick - spl->tickstart;
	if (ticklen >= ustotick(100)) {
		spl->latbust++;
		if (lat_stop)
			debug("ring");
	}
	if (spl->tickmax < ticklen) {
		spl->tickmax = ticklen;
		if (spl->suspendtick) {
			if (spl->suspendtick > spl->resumetick)
				sticklen = ((unsigned int)(0xffffffff) -
				spl->suspendtick) + spl->resumetick;
			else
				sticklen = spl->resumetick - spl->suspendtick;
			spl->realtickmax = ticklen - sticklen;
			spl->suspendtick = spl->resumetick = 0;
		}
		spl->lopc = pc;
		spl->hipc = spl->cur_hipc;
		spl->mxishi = spl->curishi;
		spl->timestamp = tick;
		spl->spxcount++;
		/* btrace results are done by calling routines */
		return(1);
	}
	return(0);
}


/*
** given the starting pc of the routine, find the corresponding spl entry from
** 'head'
*/
splmeter_t *
getspl(
	splmeter_t *head,
	inst_t *pc)
{
	splmeter_t *spl = head->next;
	splmeter_t *ospl = head;
	splmeter_t *nspl;
	register int id = CPUID();

	pc = (inst_t *)roundpc(pc);
#ifdef MP
	if ((pc == (inst_t *)roundpc((inst_t *)mutex_spinlock_spl))
	|| (pc == (inst_t *)roundpc((inst_t *)mutex_bitlock_spl))
	|| (pc == (inst_t *)roundpc((inst_t *)mutex_spintrylock_spl)))
		return 0;
#endif
	while (spl != 0) {
		if (spl->callingpc == pc)
			return(spl);
		if (spl->callingpc > pc)
			break;
		ospl = spl;
		spl = spl->next;
	}
	/*
	** pc is not in list, get new splmeter_t struct
	** insert in the list in ascending pc address
	*/
	if (splmax[id] <= 0) {
		return(0);
	}

	nspl = spl_array + (id * SPLMAXCNT + (--splmax[id]));
	nspl->next = spl;
	nspl->callingpc = pc;	/* init the key */
	ospl->next = nspl;
	return(nspl);
}

#ifdef BTRACE

stkfr_t *
getstkfr()
{
	stkfr_t *newstk;

	newstk = freestk_list;
	if (newstk == 0) {
		if (stkmax)
			newstk = &stkarray[--stkmax];
		else
			return(0);
	}
	else
		freestk_list = freestk_list->next;
	newstk->next = 0;
	return(newstk);
}

/*
** put back on free list
** clear the list
*/
void
free_stktbl(bt)
stkfr_t **bt;
{
	stkfr_t *curbt, *savebt;

	curbt = *bt;
	while (curbt) {
		savebt = curbt->next;
		curbt->next = freestk_list;
		freestk_list = curbt;
		curbt = savebt;
	}
	*bt = 0;
}

/*
** create another btrace table bt, base on nbt
*/
void
dupbt(bt, nbt)
stkfr_t **bt, *nbt;
{
	stkfr_t *stk, *nstk, *ostk;

	stk = nbt;
	while (stk) {
		nstk = getstkfr();
		bcopy((char *)stk, (char *)nstk, sizeof(stkfr_t));
		if (*bt == 0) {
			ostk = nstk;
			*bt = nstk;
		}
		else {
			ostk->next = nstk;
			ostk = nstk;
		}
		stk = stk->next;
	}
}

#endif

splmeter_t *
remove_splactive(
	splmeter_t **active,
	inst_t *callingpc)
{
	splmeter_t *ospl, *spl;

	ospl = spl = *active;
	callingpc = (inst_t *)roundpc(callingpc);
	while (spl != 0) {
		if (spl->callingpc == callingpc)
			break;
		ospl = spl;
		spl = spl->active;
	}
	if (spl) {
		if (spl == *active)
			*active = spl->active;
		else
			ospl->active = spl->active;
		return(spl);
	}
	return((splmeter_t *)0);
}

onactivelist(active, spl)
splmeter_t **active;
splmeter_t *spl;
{
	splmeter_t *splp = *active;

	while (splp != 0) {
		if (splp == spl)
			return(1);
		else
		  	splp = splp->active;
	}
	return(0);
}


/*ARGSUSED*/
uint
ospl_is_lower(unsigned ospl, unsigned target_spl)
{
#ifdef EVEREST
	static int splmask[] = {
	SRB_SWTIMO|SRB_TIMOCLK,	/* spl1 */
	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK,	/* splnet */
	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|EVINTR_LEVEL_MAXLODEV+1, /* splvme */
	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|EVINTR_LEVEL_MAXHIDEV+1,/*tty*/
	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK|EVINTR_LEVEL_HIGH+1, /* spl6 */
	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_UART|SRB_SCHEDCLK|EVINTR_LEVEL_EPC_PROFTIM+1, /* splprof */
	SR_IMASK|EVINTR_LEVEL_MAX};

	if ((ospl & SR_IE) != 0) { /* XXX SR_EXL? */
		uint srmask = splmask[spl_level] & SR_IMASK;
		uint osrmask = ospl & SR_IMASK;
		uint celevel = splmask[spl_level] & 0xff;
		uint ocelevel = ospl & 0xff;

		if ((osrmask & srmask) != 0)
			return(1);
		if (ocelevel < celevel)
			return(1);
	}
	return(0);
#else
#ifdef IP32
	switch (target_spl) {
	case SPLHI:
		return !WASSPLHI(ospl);
	case SPLPROF:
		return !WASSPLPROF(ospl);
	case SPL7_IP32:
#ifdef NOTDEF
		return !WASSPL7(ospl);
#endif
		return 1;
	}
	return 0;
#else /* !IP32 */
	switch (target_spl) {
	case SPLHI:
		return !issplhi(ospl|SR_IE);
	case SPLPROF:
		return !issplprof(ospl|SR_IE);
	case SPL7:
#ifdef NOTDEF
		return !isspl7(ospl|SR_IE);
#endif
		return 1;
	}
	return 0;
#endif /* IP32 */
#endif /* EVEREST */
}

uint
ospl_is_higher(unsigned ospl, unsigned target_spl)
{
#ifdef IP32
	switch (target_spl) {
	case SPLHI:
		return WASSPLPROF(ospl);
	case SPLPROF:
#ifdef NOTDEF
		return WASSPL7(ospl);
#else
		return 0;
#endif
	case SPL7_IP32:
		return 0;
	}
	return 0;

#else /* !IP32 */
	switch (target_spl) {
	case SPLHI:
		return issplprof(ospl|SR_IE);
	case SPLPROF:
#ifdef NOTDEF
		return isspl7(ospl|SR_IE);
#else
		return 0;
#endif
	case SPL7:
		return 0;
	}
	return 0;
#endif /* IP32 */
}
#endif
