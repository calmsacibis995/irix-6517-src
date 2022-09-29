#if IP22 || IP26 || IP28
/* Copyright 1996, Silicon Graphics Inc., Mountain View, CA. */
/*
 * streams driver for the PC keyboard mouse controller.  Supported are:
 *	- Award 8242WB PC (PS/2 compat) and IOC kbd/ms
 *
 * $Revision: 1.63 $
 */

#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/cpu.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/param.h"
#include "sys/sbd.h"
#include "sys/stream.h"
#include "sys/strids.h"
#include "sys/stropts.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/pckm.h"
#include "sys/capability.h"
#include "sys/mac_label.h"
#include "sys/ddi.h"
#include "sys/strmp.h"

typedef struct kmport_s {
	int	km_state;		/* status bits */
	int	km_bsize;		/* streams buffer size */
	int	km_fdelay;		/* delay in forwarding packets */
	toid_t	km_tid;			/* timeout to buffer upstream flow */
	int	(*km_porthandler)(unsigned char);/* textport hook */
	queue_t *km_rq,*km_wq;		/* our queues */
	mblk_t	*km_rmsg, *km_rmsge;	/* current input message head/tail */
	mblk_t	*km_rbp;		/* current input buffer */
} kmport_t;

#define KM_EXISTS	0x0001
#define KM_ISOPEN	0x0002
#define KM_MOUSE	0x0004
#define KM_SE_PENDING	0x0008
#define KM_TR_PENDING	0x0010

#define KEYBOARD_PORT	0L
#define MOUSE_PORT	1L

kmport_t kmports[2] = {
	{ 0, 		12, 2 },	/* 3 buffered keystrokes for keyboard */
	{ KM_MOUSE,	 3, 1 }		/* 1 mouse movemen, latency sensitive */
};

static int kbd_ledstate;
static int kbd_setleds;

static int pckm_nobuf;		/* dropped char count from buffer shortfall */
static int pckm_mparity;	/* count of parity errors for mouse */
static int pckm_kparity;	/* count of parity errors for keyboard */

mutex_t	pckm_mutex;		/* one mutex for pckm */

static void pckm_intr(__psint_t, struct eframe_s *);
static void pckm_intr_lock(void);
static void pckm_sendok(void);
static void pckm_mousecmd(void);
static void pckm_flushinp(void);
static void pckm_setcmdbyte(int);
static void pckm_reinit_lock(int);
static void pckm_reinit(int);
static void pckm_rx(kmport_t *, int);
static void pckm_rx_lock(kmport_t *, int);
static int pckm_rsrv(queue_t *);
static int pckm_wput(queue_t *, mblk_t *);
static int pckm_open(queue_t *, dev_t *, int, int, struct cred *);
static int pckm_close(queue_t *, int, struct cred *);

#define pckm_pollcmd(X,Y)	_pckm_pollcmd((X),(Y),0)
static int _pckm_pollcmd(int, int, int *);

static struct module_info pckm_minfo = {
	0, "PCKM", 0, 1024, 128, 16
};
struct qinit pckm_rinit = {
	NULL, pckm_rsrv, pckm_open, pckm_close, NULL, &pckm_minfo, NULL
};
struct qinit pckm_winit = {
	pckm_wput, NULL, NULL, NULL, NULL, &pckm_minfo, NULL
};
struct streamtab pckminfo = {
	&pckm_rinit, &pckm_winit, NULL, NULL
};

int pckmdevflag = D_MP;

#if 0
#define PROBEDEBUG(X)		cmn_err(CE_CONT,X)
#else
#define PROBEDEBUG(X)
#endif

#ifdef KBDDEBUG
static void updatekbdhist(int);
#endif

void
early_pckminit(void)
{
	static int pckminited = 0;
	kmport_t *km;
	int data;

	if (pckminited) return;
	pckminited = 1;

	initkbdtype();

	mutex_init(&pckm_mutex, MUTEX_DEFAULT, "pckm");

	mutex_lock(&pckm_mutex, PZERO);

	pckm_flushinp();
	pckm_setcmdbyte(CB_MSDISABLE);

	/* probe keyboard with default disable command, then re-enable */
	km = &kmports[KEYBOARD_PORT];
	pckm_flushinp();
	data = pckm_pollcmd(KEYBOARD_PORT,CMD_DISABLE);
	if (data == KBD_ACK) {
		/* init keyboard, leds off, scan mode 3, make/break */
		pckm_flushinp();
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS);
		(void)pckm_pollcmd(KEYBOARD_PORT,0);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SELSCAN);
		(void)pckm_pollcmd(KEYBOARD_PORT,3);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMBTYPE);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
		(void)pckm_pollcmd(KEYBOARD_PORT,SCN_CAPSLOCK);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
		(void)pckm_pollcmd(KEYBOARD_PORT,SCN_NUMLOCK);

		/* re-enable keyboard */
		pckm_flushinp();
		data = pckm_pollcmd(KEYBOARD_PORT,CMD_ENABLE);
		if (data == KBD_ACK) {
			km->km_state |= KM_EXISTS;
			PROBEDEBUG("pckm: found keyboard\n");
		}
		else
			PROBEDEBUG("pckm: keyboard enable failed!\n");
	}
	else
		PROBEDEBUG("pckm: keyboard disable failed!\n");

	/* probe mouse with defaults, then make sure it is enabled */
	pckm_setcmdbyte(CB_KBDISABLE);
	km = &kmports[MOUSE_PORT];
	DELAY(500); pckm_flushinp();
	data = pckm_pollcmd(MOUSE_PORT,CMD_DEFAULT);
	if (data == KBD_ACK) {
		(void)pckm_pollcmd(MOUSE_PORT,CMD_MSRES);
		(void)pckm_pollcmd(MOUSE_PORT,0x03);
		km->km_state |= KM_EXISTS;
		PROBEDEBUG("pckm: found pcmouse\n");
	}
	else
		PROBEDEBUG("pckm: mouse default failed!\n");

	DELAY(500);
	pckm_flushinp();

	setlclvector(VECTOR_KBDMS,(lcl_intr_func_t *)pckm_intr,0);

	pckm_setcmdbyte(CB_KBINTR|CB_MSINTR);

	mutex_unlock(&pckm_mutex);
}

void
pckminit(void)
{
	/*REFERENCED*/
	graph_error_t rc;
	vertex_hdl_t dev_vhdl;

	early_pckminit();
	kbdbell_init();			/* need u-area set-up */

	rc = hwgraph_char_device_add(hwgraph_root,"input/keyboard","pckm",
				     &dev_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_fastinfo_set(dev_vhdl,(arbitrary_info_t)&kmports[0]);
	
	rc = hwgraph_char_device_add(hwgraph_root,"input/mouse","pckm",
				     &dev_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_fastinfo_set(dev_vhdl, (arbitrary_info_t)&kmports[1]);
}

/*ARGSUSED*/
static int
pckm_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	queue_t *wq = WR(rq);
	kmport_t *km;

	if (sflag)			/* only a simple streams driver */
		return(ENXIO);

	if (!dev_is_vertex(*devp))	/* must be a HWG device */
		return(ENODEV);

	km = (kmport_t *)device_info_get(*devp);

	mutex_lock(&pckm_mutex, PZERO);
	
	/*
	 * Disallow multiple opens.
	 */
	if (km->km_state & KM_ISOPEN) {
		mutex_unlock(&pckm_mutex);
		return(EBUSY);
	}

	/* connect device to stream */
	rq->q_ptr = (caddr_t)km;
	wq->q_ptr = (caddr_t)km;
	km->km_wq = wq;
	km->km_rq = rq;

	/* finish up kmport_t */
	km->km_porthandler = NULL;
	km->km_state |= KM_ISOPEN;

	if ((km->km_state & KM_MOUSE) == 0) {
		/* reset led state */
		kbd_ledstate = kbd_setleds = 0;
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS);
		(void)pckm_pollcmd(KEYBOARD_PORT,kbd_ledstate);

		/* turn off autorepeat */
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMB);
	}
	else {
		/* make sure mouse is enabled and running at full res */
		(void)pckm_pollcmd(MOUSE_PORT,CMD_ENABLE);
		(void)pckm_pollcmd(MOUSE_PORT,CMD_MSRES);
		(void)pckm_pollcmd(MOUSE_PORT,0x03);
	}

	mutex_unlock(&pckm_mutex);

	return(0);
}

/*ARGSUSED*/
static int
pckm_close(queue_t *rq, int flag, struct cred *crp)
{
	kmport_t *km;

	mutex_lock(&pckm_mutex, PZERO);

	km = (kmport_t *)rq->q_ptr;

	if (km->km_state & KM_TR_PENDING)
		untimeout(km->km_tid);

	/* clean-up streams buffers */
	if (km->km_state & KM_SE_PENDING) {
		km->km_state &= ~KM_SE_PENDING;
		str_unbcall(rq);
	}
	freemsg(km->km_rmsg);
	km->km_rmsg = 0;
	freemsg(km->km_rbp);
	km->km_rbp = 0;

	km->km_rq = km->km_wq = NULL;
	km->km_state &= ~(KM_ISOPEN|KM_TR_PENDING);

	if ((km->km_state & KM_MOUSE) == 0) {
		/* reset led state */
		kbd_ledstate = kbd_setleds = 0;
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS);
		(void)pckm_pollcmd(KEYBOARD_PORT,kbd_ledstate);

		/* turn on autorepeat */
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMBTYPE);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
		(void)pckm_pollcmd(KEYBOARD_PORT,SCN_CAPSLOCK);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
		(void)pckm_pollcmd(KEYBOARD_PORT,SCN_NUMLOCK);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ENABLE);
	}

	mutex_unlock(&pckm_mutex);

	return(0);
}

static int
pckm_wput(queue_t *wq, mblk_t *bp)
{
	mutex_lock(&pckm_mutex, PZERO);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		/* byte 0 -> reinit mouse/keyboard 
		 *	1 -> set leds
		 */
		if (bp->b_rptr < bp->b_wptr) {
			if (*bp->b_rptr & 0x01)
				pckm_reinit_lock(MOUSE_PORT);
			if (*bp->b_rptr & 0x02)
				pckm_reinit_lock(KEYBOARD_PORT);
			bp->b_rptr++;
		}
		if (bp->b_rptr < bp->b_wptr) {
			pckm_setleds(*bp->b_rptr);
			bp->b_rptr++;
		}
		freemsg(bp);
		break;
	case M_IOCTL:
		/* only should get a TCSETA ioctl, which we just eat */
		freemsg(bp);
		break;
	case M_CTL:
		if (*bp->b_rptr == 0xde) {
			/*
			 * It's a query for keyboard type.  Byte 0 of 
			 * data remains 0xde, byte 1 is id, always plain
			 * AT for us.
			 */
			*bp->b_wptr++ = KEYBD_ID;
			qreply(wq, bp);
			break;
		}
		/*FALLSTHROUGH*/
	default:
		mutex_unlock(&pckm_mutex);
		sdrv_error(wq,bp);
		return(0);
	}

	mutex_unlock(&pckm_mutex);

	return(0);
}

/* Get a new buffer.
 */
static mblk_t *
pckm_getbp(kmport_t *km, u_int pri)
{
	mblk_t *bp, *rbp;

	rbp = km->km_rbp;
	/* if current buffer empty do not need another buffer */
	if (rbp && rbp->b_rptr >= rbp->b_wptr) {
		bp = NULL;
	}
	else {
		while (1) {
			if (bp = allocb(km->km_bsize, pri))
				break;
			if (pri == BPRI_HI)
				continue;
			if (!(km->km_state & KM_SE_PENDING)) {
				bp = str_allocb(km->km_bsize,km->km_rq,
						BPRI_HI);
				if (!bp)
					km->km_state |= KM_SE_PENDING;
			}
			break;
		}
	}

	if (!rbp)
		km->km_rbp = bp;
	else if (bp || rbp->b_wptr >= rbp->b_datap->db_lim) {
		/* we have an old buffer and a new buffer, or
		 * the old buffer is full.
		 */
		str_conmsg(&km->km_rmsg, &km->km_rmsge, rbp);
		km->km_rbp = bp;
	}

	return(bp);
}

/* heartbeat to send input upstream.  used to reduce the large cost of
 * trying to send each input byte upstream by itself.
 */
static void pckm_rsrv_timer_lock(kmport_t *km);
static void
pckm_rsrv_timer(kmport_t *km)
{
	mutex_lock(&pckm_mutex, PZERO);
	pckm_rsrv_timer_lock(km);
	mutex_unlock(&pckm_mutex);
}
static void
pckm_rsrv_timer_lock(kmport_t *km)
{
	if ((km->km_state & KM_ISOPEN) == 0) {
		km->km_state &= ~KM_TR_PENDING;
		return;			/* close should have done free */
	}

	if (canenable(km->km_rq)) {
		queue_t *q = km->km_rq;

		km->km_state &= ~KM_TR_PENDING;
		mutex_unlock(&pckm_mutex);	/* unlock b4 qenable */
		(void)STREAMS_INTERRUPT((strintrfunc_t)qenable, q, 0, 0);
		mutex_lock(&pckm_mutex, PZERO);
		return;
	}

	km->km_tid = STREAMS_TIMEOUT(pckm_rsrv_timer,(void *)km,km->km_fdelay);
}

static int
pckm_rsrv(queue_t *rq)
{
	kmport_t *km = (kmport_t *)rq->q_ptr;
	mblk_t *bp;

	mutex_lock(&pckm_mutex, PZERO);

	if (!canput(rq->q_next)) {	/* quit if upstream congested */
		noenable(rq);
		mutex_unlock(&pckm_mutex);
		return(0);
	}

	enableok(rq);

	if (km->km_state & KM_SE_PENDING) {
		km->km_state &= ~KM_SE_PENDING;
		str_unbcall(rq);
	}

	/* when we do not have an old buffer to send up, or when we are
	 * timing things, send up the current buffer.
	 */
	bp = km->km_rbp;
	if (bp && (bp->b_wptr > bp->b_rptr) &&
	    (!km->km_rmsg || !(km->km_state&KM_TR_PENDING))) {
		str_conmsg(&km->km_rmsg, &km->km_rmsge, bp);
		km->km_rbp = NULL;
	}

	bp = km->km_rmsg;
	if (bp) {
		km->km_rmsg = NULL;
		mutex_unlock(&pckm_mutex);
		putnext(rq,bp);		/* send the message w/o blocking */
		mutex_lock(&pckm_mutex, PZERO);
	}

	/* get a buffer now, rather than waiting for an interrupt */
	if (!km->km_rbp)
		  (void)pckm_getbp(km, BPRI_LO);

	mutex_unlock(&pckm_mutex);

	return(0);
}


static void
pckm_reinit(int device)
{
	mutex_lock(&pckm_mutex, PZERO);
	pckm_reinit_lock(device);
	mutex_unlock(&pckm_mutex);
}

static void
pckm_reinit_lock(int device)
{
	if (device == KEYBOARD_PORT) {
		pckm_setcmdbyte(CB_MSDISABLE);

		(void)pckm_pollcmd(device,CMD_DISABLE);
		(void)pckm_pollcmd(device,CMD_SETLEDS);
		(void)pckm_pollcmd(device,kbd_ledstate);
		(void)pckm_pollcmd(device,CMD_SELSCAN);
		(void)pckm_pollcmd(device,3);
		(void)pckm_pollcmd(device,CMD_ALLMBTYPE);
		(void)pckm_pollcmd(device,CMD_KEYMB);
		(void)pckm_pollcmd(device,SCN_CAPSLOCK);
		(void)pckm_pollcmd(device,CMD_KEYMB);
		(void)pckm_pollcmd(device,SCN_NUMLOCK);
		(void)pckm_pollcmd(device,CMD_ENABLE);
		if (kmports[device].km_state & KM_ISOPEN)
			(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMB);
	}
	else {
		pckm_setcmdbyte(CB_KBDISABLE);
		(void)pckm_pollcmd(MOUSE_PORT,CMD_MSRES);
		(void)pckm_pollcmd(MOUSE_PORT,0x03);
		(void)pckm_pollcmd(device,CMD_ENABLE);
	}

	pckm_setcmdbyte(CB_KBINTR|CB_MSINTR);
}

static void
pckm_rx_lock(kmport_t *km, int data)
{
	mutex_lock(&pckm_mutex, PZERO);
	pckm_rx(km, data);
	mutex_unlock(&pckm_mutex);
}

static void
pckm_rx(kmport_t *km, int data)
{
	mblk_t *bp;

	/*  Check if we get a BAT success or BAT failure data.
	 * This data implies that the keyboard/mouse was just
	 * plugged so it needs to be re-initialized.  Only
	 * look for KBD_BATOK on mouse since the failure case
	 * looks like mouse data.
	 */

	ASSERT(mutex_mine(&pckm_mutex));

	if ((data == KBD_BATOK) || (data == KBD_BATFAIL)) {
		if (km->km_state & KM_MOUSE) {
			if (data == KBD_BATOK) {
				dtimeout(pckm_reinit,(void *)MOUSE_PORT,
					 1,plbase,cpuid());
				return;
			}
		}
		else {
			dtimeout(pckm_reinit,(void *)KEYBOARD_PORT,1,
				 plbase,cpuid());
			return;
		}
	}
			
	/* If not open (normal or textport) drop character.
	 */
	if ((km->km_state & KM_ISOPEN) == 0 && km->km_porthandler == NULL)
		return;

	if (km->km_porthandler && (*km->km_porthandler)(data))
		return;

#ifdef KBDDEBUG
	if ((km->km_state & KM_MOUSE) == 0) {
		updatekbdhist(data);
	}
#endif

	/* If there are no buffers availiable, drop the input.
	 */
	if ((bp = km->km_rbp) || (bp = pckm_getbp(km, BPRI_HI))) {
		*bp->b_wptr = data;
		if (++bp->b_wptr >= bp->b_datap->db_lim)
			(void)pckm_getbp(km, BPRI_LO);	/* send when full */

		/*  Forward caps lock and numlock post haste since LEDs
		 * must be tripped at interrupt level, try and turn LED
		 * request quickly.
		 */
		if ((data == SCN_CAPSLOCK) || (data == SCN_NUMLOCK)) {
			if (km->km_state & KM_TR_PENDING)
				untimeout(km->km_tid);
			pckm_rsrv_timer_lock(km); /* clears KM_TR_PENDING */
		}
		else if ((km->km_state & KM_TR_PENDING) == 0) {
			km->km_state |= KM_TR_PENDING;
			km->km_tid = STREAMS_TIMEOUT(pckm_rsrv_timer,
				(void *)km, km->km_fdelay);
		}
	}
	else
		pckm_nobuf++;
}

static void
pckm_intr_lock(void)
{
	static volatile int atintr = 0;
	int data, status;

	if (++atintr > 1) /* guard against nest calling via pckm_sendok() */
		return;

	status = inp(KB_REG_64);
	data = inp(KB_REG_60);

moredata:
	if ((status & (SR_MSFULL|SR_OBF)) == (SR_MSFULL|SR_OBF)) {
		SYSINFO.rcvint++;
		if (status & SR_PARITY) {
			pckm_mparity++;
			pckm_mousecmd();
			pckm_sendok();
			outp(KB_REG_60,CMD_RESEND);
			goto done;
		}
		
		(void)STREAMS_INTERRUPT((strintrfunc_t)pckm_rx_lock,
					&kmports[MOUSE_PORT],
                                        (__psint_t)data, 0);
	}
	else if (status & SR_OBF) {
		SYSINFO.rcvint++;
		if (status & SR_PARITY) {
			pckm_kparity++;
			pckm_sendok();
			outp(KB_REG_60,CMD_RESEND);
			goto done;
		}
		if (status & SR_TO) {		/* keyboard timed out */
			kbd_setleds = 0;	/* no keyboard to change */
			goto done;
		}
		(void)STREAMS_INTERRUPT((strintrfunc_t)pckm_rx_lock,
					&kmports[KEYBOARD_PORT],
                                        (__psint_t)data, 0);
	}

done:
	/* Need to update leds, now which is the safest time! */
	if (kbd_setleds) {
		data = _pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS,&status);
		switch (data) {
		case -1:		/* timeout -- give up. */
			break;
		case KBD_ACK:		/* everything ok, send led state */
			kbd_setleds = 0;
			data = pckm_pollcmd(KEYBOARD_PORT,kbd_ledstate);
	/* 
	 * HACK HACK HACK ...
	 * Microsoft keyboards have a problem with CAPS_LOCK keys... 
	 * they seem return 0x14 (SCN_CAPSLOCK) instead of 0xfa.  
	 * At that point in time the keyboard will lock up ... 
	 * flushing the keyboard input seems to clear the problem.
	 */
			if (data == SCN_CAPSLOCK) {
				pckm_flushinp();
#ifdef KBDDEBUG
				cmn_err_tag(9,CE_WARN,"Keyboard error, recovering.");
#endif
			}
			break;
		default:
			/*  Some real input got returned from the led set-up.
			 * Loop through the interrupt handler again.
			 */
			goto moredata;
		}
	}

	atintr = 0;
}

/*ARGSUSED*/
static void
pckm_intr(__psint_t arg, struct eframe_s *ep)
{
	mutex_lock(&pckm_mutex, PZERO);
	pckm_intr_lock();
	mutex_unlock(&pckm_mutex);
}

static int
_pckm_pollcmd(int port, int cmd, int *rsr)
{
	int sr,timeout,data;
	int rcnt=2;		/* try up to three times */

	if (!rsr) rsr = &sr;	/* allow rsr=NULL for dont care */

again:
	if (port == MOUSE_PORT) pckm_mousecmd();
	pckm_sendok();
	outp(KB_REG_60,cmd);
	DELAY(5);
	for (timeout=400; timeout ; timeout--) {
		sr = *rsr = inp(KB_REG_64);

		if ((sr & SR_PARITY) && rcnt) {
			(void)inp(KB_REG_60);		/* flush buffer */
			rcnt--;
			goto again;
		}
		/* controller timeout on keyboard port */
		if ((sr & SR_TO) && (port == KEYBOARD_PORT))
			return(-1);

		if (sr & SR_OBF) {
			DELAY(5);
			data = inp(KB_REG_60);
			if (data == KBD_RESEND && rcnt) {
				rcnt--;
				goto again;
			}
			if (data != KBD_OVERRUN)
				return(data);
		}
		DELAY(50);
	}

	/* retry the command for safetys sake */
	if (rcnt) {
		rcnt--;
		goto again;
	}

	return(-1);
}

/* routines used by textport/console */
int
pckm_kbdexists(void)
{
	return((kmports[KEYBOARD_PORT].km_state & KM_EXISTS) != 0);
}

/* must be called at with lock held */
void
pckm_setleds(int newvalue)
{
	if (kbd_ledstate == newvalue)
		return;

	kbd_ledstate = newvalue;
	kbd_setleds = 1;		/* interrupt handler will get it */
}

static void
pckm_sendok(void)
{
	int sr, retry = 20834;			/* ~1/4 second */

	do {
		DELAY(2);
		sr = inp(KB_REG_64);
		if (sr & (SR_PARITY|SR_TO)) {
			DELAY(2);
			(void)inp(KB_REG_60);	/* flush buffer */
		}
		else if (sr&SR_OBF)
			pckm_intr_lock();		/* handle any input */
	} while((sr & SR_IBF) && --retry);
	DELAY(2);
}

static void
pckm_mousecmd(void)
{
	pckm_sendok();
	outp(KB_REG_64, CMD_NEXTMS);
}

static void
pckm_flushinp(void)
{
	int retry = 25000;			/* ~1/4 second */

	DELAY(2);
	while ((inp(KB_REG_64) & SR_OBF) && --retry) {
		DELAY(2);
		(void)inp(KB_REG_60);
		DELAY(2);
	}
}

static void
pckm_setcmdbyte(int value)
{
	pckm_sendok();
	outp(KB_REG_64,CMD_WCB);
	pckm_sendok();
	outp(KB_REG_60,value);
}

/*ARGSUSED*/
void
gl_setporthandler(int port, int (*fp)(unsigned char))
{
	kmports[KEYBOARD_PORT].km_porthandler = fp;
}

int
du_keyboard_port(void)
{
	return(-1);
}

#ifdef KBDDEBUG
#define KBDHIST		12
int kbdhist[KBDHIST];
static void
updatekbdhist(int data)
{
	int i;

	for (i=0; i < KBDHIST-1; i++)
		kbdhist[i] = kbdhist[i+1];
	kbdhist[KBDHIST-1] = data;
}
#endif

#endif /* IP22 || IP26 || IP28 */
