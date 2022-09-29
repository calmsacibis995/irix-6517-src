/*
 * Provide HIM entry points.  Is only does 1 command at a time.
 *
 * $Id: simhim.c,v 1.1 1997/06/05 23:39:26 philw Exp $
 */

#ifdef SIMHIM

#include "stdio.h"
#include "sys/types.h"
#include "sys/scsi.h"
#include "sys/param.h"
#include "sys/ddi.h"
#include "sys/cmn_err.h"
#include "osm.h"
#include "him_scb.h"
#include "him_equ.h"
#include "seq_off.h"
#include "sys/adp78.h"

#include "../../../../IP32/include/DBCsim.h"

extern int	adp_verbose;
extern int	do_devscsi(int targ, unsigned char *cdbbuf, SG_ELEMENT *sgp);
extern void	PH_ScbCompleted(sp_struct *);
#ifdef LE_DISK
extern int	soft_swizzle;
#endif

int ph_cmdcompleted=0;
int ph_cmdinprog=0;
int ph_cmdstatus=0;
sp_struct *ph_cmdscb=NULL;

static int	do_scb(sp_struct *scb_p);

#define CMDSTAT_GOOD			0
#define CMDSTAT_SELTIMEOUT		1
#define CMDSTAT_HARDERROR		2

/*
 * This is the main entry point for the driver.  All disk requests go
 * through here.
 *
 * Take a SCB, convert it to a /dev/scsi request, and send it out.
 */

void
PH_ScbSend(sp_struct *scb_p)
{
	if (ph_cmdinprog) {
		printf("PH_ScbSend error: already a cmd in prog\n");
		return;
	}

	ph_cmdinprog = 1;
	ph_cmdscb = scb_p;

	ph_cmdstatus = do_scb(scb_p);

	ph_cmdinprog = 0;
	ph_cmdcompleted = 1;

}

/*
 * Called by adp_poll_status to see if the sequencer has finished something.
 * Check for command done, and return yes.
 */

unsigned char
Ph_ReadIntstat(AIC_7870 *base)
{
	if (ph_cmdcompleted) {
		return CMDCMPLT;
	} else {
		printf("Ph_ReadIntstat: no completed commands.");
		printf("%d commands in progress\n", ph_cmdinprog);
		return 0;
	}
}


/*
 * Called by adp_poll_status after it finds out the sequencer has finished
 * something.
 *
 * Update the scb status variables, call PH_ScbCompleted, and return good
 * status.
 */
unsigned char
PH_IntHandler(cfp_struct *config_ptr)
{
	if ((ph_cmdscb == NULL) || (ph_cmdcompleted == 0)) {
		printf("PH_IntHandler error: no active scb or cmd completed.");
		printf("cmdinprog %d cmdcompleted %d\n", ph_cmdinprog,
		       ph_cmdcompleted);
		return 0;
	}

	if (ph_cmdstatus == CMDSTAT_HARDERROR) {
		DPRINTF("PH_IntHandler: returning hard error on scb\n");
		ph_cmdscb->SP_Stat = SCB_ERR;
		ph_cmdscb->SP_HaStat = HOST_HW_ERROR;
		ph_cmdscb->SP_TargStat = UNIT_GOOD;
	} else if (ph_cmdstatus == CMDSTAT_SELTIMEOUT) {
		DPRINTF("PH_IntHandler: selection timeout\n");
		ph_cmdscb->SP_Stat = SCB_ERR;
		ph_cmdscb->SP_HaStat = HOST_SEL_TO;
		ph_cmdscb->SP_TargStat = UNIT_GOOD;
	} else {
		/* everything is fine */
		ph_cmdscb->SP_Stat = SCB_COMP;
		ph_cmdscb->SP_HaStat = HOST_NO_STATUS;	/* 0 */
		ph_cmdscb->SP_TargStat = UNIT_GOOD;		/* 0 */
	}

	PH_ScbCompleted(ph_cmdscb);

	ph_cmdcompleted = 0;
	ph_cmdstatus = 0;
	ph_cmdscb = NULL;

	return (OUR_CC_INT | CMDCMPLT);
}

/*
 * Various other uninteresting entry points
 */

void
PH_GetConfig(cfp_struct *config_ptr)
{
	if (adp_verbose)
		PRINT("PH_GetConfig: config_ptr 0x%x\n", config_ptr);
}

UWORD
PH_InitHA(cfp_struct *config_ptr)
{
	if (adp_verbose)
		PRINT("PH_InitHA: config_ptr 0x%x\n", config_ptr);
	return 0;
}

void
Ph_WriteHcntrl(AIC_7870 *base, UBYTE val)
{
	if (adp_verbose)
		PRINT("Ph_WriteHcntrl: writing to base 0x%x value 0x%x\n",
		      base, val);
}

void
PH_Set_Selto(cfp_struct *config_ptr, int stimesel)
{

}

void
Ph_SetNeedNego(UBYTE index, AIC_7870 *base)
{

}

/*
 * Take a UBTYE ptr to an array of len bytes and swizzle it (convert from
 * big to little endian or little to big.  Len must be a multiple of 4
 * and less than 4096.  The buf address also must me mod 4.
 * 
 * Returns 0 if the swizzle was successful, -1 if not.
 */

#define SWIZZLEBUF_SIZE		(4096 * 12)
UBYTE swizzlebuf[SWIZZLEBUF_SIZE];

int
osm_swizzle(UBYTE *buf, uint len)
{
	int word, s;
	uint actual_len;

	if (len%4 != 0) {
		cmn_err(CE_WARN, "osm_swizzle: len not mod 4 (%d)!", len);
		return -1;
	}


	if (((DWORD) buf)%4 != 0) {
		cmn_err(CE_WARN, "osm_swizzle: buf addr not mod 4 (0x%x)!",
			buf);
		return -1;
	}

	s = BLOCK_INTERRUPTS();
	actual_len = MIN(len, SWIZZLEBUF_SIZE);
	while (len > 0) {
		bcopy(buf, swizzlebuf, actual_len);
		word = 0;
		while (word < actual_len) {
			buf[word + 0] = swizzlebuf[word + 3];
			buf[word + 1] = swizzlebuf[word + 2];
			buf[word + 2] = swizzlebuf[word + 1];
			buf[word + 3] = swizzlebuf[word + 0];
			word += 4;
		}

		len -= actual_len;
		buf += actual_len;
	}
 	UNBLOCK_INTERRUPTS(s);

	return 0;
}

/*
 * Internal functions.
 */

/*
 * rip apart the scb and call do_devscsi with something that's understandable.
 */
static int
do_scb(sp_struct *scb_p)
{
	int rval, targ;
	unsigned char cdbbuf[MAX_CDB_LEN];

	targ = scb_p->SP_Tarlun >> 4;
	bcopy((void *) scb_p->Sp_CDB, cdbbuf, MAX_CDB_LEN);

	/*
	 * If the driver is doing swizzling for an LE disk, then
	 * I have to unswizzle it back again.
	 */
#ifdef LE_DISK
	if (soft_swizzle) {
		osm_swizzle(cdbbuf, MAX_CDB_LEN);
	}
#endif

	rval = do_devscsi(targ, cdbbuf, (SG_ELEMENT *) scb_p->SP_SegPtr);
	if (rval == 1)
		return CMDSTAT_SELTIMEOUT;
	else if (rval == -1)
		return CMDSTAT_HARDERROR;
	else
		return CMDSTAT_GOOD;
}

#endif /* SIMHIM */
