/*
 * Copyright (C) 1986, 1992, 1993, 1994, 1995, 1996 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/*
 * streams driver for the (PS/2 compat) keyboard and mouse
 * 
 *
 */

#include "sys/types.h"
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
#include "sys/mace.h"
#include "sys/kopt.h"
#include "sys/strmp.h"
#include "sys/ddi.h"
#include "sys/PCI/pciio.h"
#include <sys/invent.h>

#define PPCKM		PZERO
#define PCKM_MACEMASK	0xA00
#define KEYBOARD_MINOR	1
#define MOUSE_MINOR	2

int mh_mouse_port, mh_keyboard_port;

#define KEYBOARD_PORT	mh_keyboard_port
#define MOUSE_PORT	mh_mouse_port

/* mace interprets the line control bit as start bit of
 * next character. This causes the actual start bit to 
 * be interpreted as the first data bit and msb to be 
 * dropped. If the last data bit (msb) was 1, that also
 * cause parity bit to be set in the status byte.
 */
int mace_rev = 0;

/*
 * There are a lot of electrical noise problems with the mouse on O2 causing a
 * significant amount of bogus data to be recieved.  Usually when
 * bogus data arrives we get a Parity of Framing error, but not always.
 * There is nothing we can do in that case (no way to decide whats
 * bad data and whats not).
 *
 * mhkm_reset determines action to take upon Framing or Parity error.
 * Choices are:		
 *	0 => ask device to resend last data.  This is the normal case.
 *      1 => re-initialize device Manuf needs this because of wierd 
 *		equipment set-up they have.
 *      >1 => do nothing.  For playing around.
 *
 * mhkm_print determines whether or not to print an error message
 * when a parity or framing error is recieved.
 */
int mhkm_reset = 0;

#ifdef DEBUG
int mhkm_print = 1;
#else
int mhkm_print = 0;
#endif

typedef struct kmport_s {
	int	km_state;		/* status bits */
	struct ps2if *ifp;		/* PS/2 interface */
	toid_t	km_tid;			/* timeout to buffer upstream flow */
	vertex_hdl_t conn_vhdl;         /* connection to pci services */
	mutex_t mutex;			/* protect access to kbd/ms */
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

kmport_t kmports[] = {
	{ 0, (struct ps2if *) PHYS_TO_K1(MACE_KBDMS) },		/* port 0 */
	{ 0, (struct ps2if *) PHYS_TO_K1(MACE_KBDMS+0x20) }	/* port 1 */
};

#define PERIPHERAL_STATUS_REGISTER      PHYS_TO_K1(ISA_INT_STS_REG)
#define PERIPHERAL_INTMSK_REGISTER      PHYS_TO_K1(ISA_INT_MSK_REG)
#define MACE_ID_REGISTER		PHYS_TO_K1(ISA_RINGBASE)

/*
 * Despite the name, these pciio_pio_* functions really belong here.
 * They are part of the CRIME 1.1 PIO read war, so don't touch them unless
 * you understand the consequences.
 */
#define offset(x)	           ((int) &((struct ps2if *) 0)->x)
#define WRITE_ISA(reg, val) pciio_pio_write64((uint64_t)(val & 0xff), (volatile uint64_t *)((ulong)km->ifp + offset(reg)))
#define READ_ISA(reg)       (pciio_pio_read64((volatile uint64_t *)((ulong)km->ifp + offset(reg))) & 0xff)

static int kbd_ledstate;

static int pckm_nobuf;		/* dropped char count from buffer shortfall */
int pckm_parity; 		/* Count of parity errors for keybd and mouse */

void pckm_intr(struct eframe_s *, int arg);
static void pckm_sendok(kmport_t *);
static void pckm_flushinp(kmport_t *);
static void pckm_reinit(int);
static int pckm_rsrv(queue_t *);
static int pckm_wput(queue_t *, mblk_t *);
static int pckm_open(queue_t *, dev_t *, int, int, struct cred *);
static int pckm_close(queue_t *, int, struct cred *);
static void pckm_enable_interrupt( void );

int pckm_pollcmd(int, int);
int _pckm_pollcmd(int, int);
void pckm_getstat();

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

int pckmdevflag = 0;

#define RMSG_LEN	12

/* Time to wait before forwarding packets.  Wait to reduce avg intr time. */
#define FWDDELAY	2

#if DEBUG
#define PROBEDEBUG(X)		cmn_err(CE_CONT,X)
#else
#define PROBEDEBUG(X)
#endif

#ifdef KBDDEBUG
static void updatekbdhist(int);
#endif

#define MAGIC 12

void
early_pckminit(void)
{
}

void
pckminit(void)
{
}

/*
 * pckm_attach
 *    Create keyboard and mouse hwgraph vertices;
 *    called by init_all_devices() in kern/ml/MOOSEHEAD/IP32init.c
 */
void
pckm_attach(vertex_hdl_t conn_vhdl)
{
	/*REFERENCED*/
        graph_error_t   rc;
        vertex_hdl_t    pckb_vhdl;
        vertex_hdl_t    pcms_vhdl;
        static int      pckminited = 0;
        kmport_t        *km, *kp;
        int             data, data1, i;
        char            *env;

#if 0
	_macereg_t macebits = 0x0000000000001E00;
#endif /* 0 */
	if (pckminited) return;
	pckminited = 1;

	initkbdtype();

	/* pciio_pio_read64 call is part of CRIME 1.1 PIO read war */
	data = pciio_pio_read64((volatile uint64_t *)MACE_ID_REGISTER);
	if (data & 0x10)
		mace_rev = 1;
	else
		mace_rev = 0;

	/* 
	 * manufacturing wants to use mhkm_reset mode.  They say
	 * we can key off the variable nogfxkbd.
	 */
	env = kopt_find("nogfxkbd");
	if (env){
		if (atoi(env) == 1){
			mhkm_reset++;
			mhkm_print++;
		}
	}
		
	/* 
	 * Keyboard and mouse can be in either MACE port - so go
	 * figure out which is which.  The keyboard will return
	 * an ACK followed by 0x83, 0xAB, while the mouse returns 0s'.
	 * Unfortunatly I don't whether all keyboards return this ID
	 * or just ours...I need to get this answer.
	 */
	mh_keyboard_port = mh_mouse_port = MAGIC;
	for (i=0; i < 2; i++){
		km = &kmports[i];
		pckm_flushinp(km);
		data = pckm_pollcmd(i,CMD_DISABLE);
		if (data == KBD_ACK) {
			pckm_flushinp(km);
			/* Try to read keyboard ID */
			data = pckm_pollcmd(i,0xf2);
			if (data != KBD_ACK){
				printf("Can't issue ID command kbd/mouse port %d\n", i);
				continue;
			}
			DELAY(2500);
			data = READ_ISA(rx_buf);
			DELAY(2500);
			data1 = READ_ISA(rx_buf);
			if ((data == 0xab) && (data1 == 0x83))
				mh_keyboard_port = i;
			if ((data == 0) && (data1 == 0)){
				mh_mouse_port = i;
			}
		} else {
			if (showconfig)
			printf("No kbd/mouse on port %d\n", i);
		}
		DELAY(200);
	}

	/*
	 * This part is kind of hacky, since I'm not sure what to do, but....
	 * If we don't find either device assign the defaults and let
	 * things procede normally.  If we find only one device, assign the
	 * other to the other port and continue.  There doesn't seem to 
	 * be any way to communicate failure.
	 */
	if (mh_keyboard_port == MAGIC && mh_mouse_port == MAGIC){
		mh_keyboard_port = 0;
		mh_mouse_port = 1;
	}
	if (mh_keyboard_port == MAGIC)
		mh_keyboard_port = !mh_mouse_port;
	if (mh_mouse_port == MAGIC)
		mh_mouse_port = !mh_keyboard_port;

	kmports[mh_mouse_port].km_state |= KM_MOUSE;

	if (showconfig)
		printf("keyboard on port %d, mouse on port %d\n", KEYBOARD_PORT, MOUSE_PORT);

	/* 
	 * Continue wth normal initialization .....
	 */

	/* probe keyboard with default disable command, then re-enable */
	kp = &kmports[KEYBOARD_PORT];

        /* create pckb in /hw/node/io */
        rc = hwgraph_char_device_add(conn_vhdl, "pckb", "pckm", &pckb_vhdl);
        ASSERT(rc == GRAPH_SUCCESS);
        hwgraph_chmod(pckb_vhdl, HWGRAPH_PERM_KBD);
        hwgraph_fastinfo_set(pckb_vhdl, (arbitrary_info_t)kp);
        kp->conn_vhdl = conn_vhdl;

        /* add keyboard inventory */
        device_inventory_add(pckb_vhdl, INV_MISC, INV_MISC_PCKM, 1, 0, 1);

	pckm_flushinp(kp);
	data = pckm_pollcmd(KEYBOARD_PORT,CMD_DISABLE);
	if (data == KBD_ACK) {
		/* init keyboard, leds off, scan mode 3, make/break */
		pckm_flushinp(kp);
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
		pckm_flushinp(kp);
		data = pckm_pollcmd(KEYBOARD_PORT,CMD_ENABLE);
		if (data == KBD_ACK) {
			kp->km_state |= KM_EXISTS;
			PROBEDEBUG("pckm: found keyboard\n");
		}
		else
			PROBEDEBUG("pckm: keyboard enable failed!\n");
	}
	else{
		PROBEDEBUG("pckm: keyboard disable failed!\n");
	}

	/* probe mouse with defaults, then make sure it is enabled */
	km = &kmports[MOUSE_PORT];
	
        /* create pcms in /hw/node/io */
        rc = hwgraph_char_device_add(conn_vhdl, "pcms", "pckm", &pcms_vhdl);
        ASSERT(rc == GRAPH_SUCCESS);
        hwgraph_chmod(pcms_vhdl, HWGRAPH_PERM_MOUSE);
        hwgraph_fastinfo_set(pcms_vhdl, (arbitrary_info_t)km);
        km->conn_vhdl = conn_vhdl;

        /* add mouse inventory */
        device_inventory_add(pcms_vhdl, INV_MISC, INV_MISC_PCKM, 1, 0, 0);

	DELAY(500); pckm_flushinp(km);
	data = pckm_pollcmd(MOUSE_PORT,CMD_DEFAULT);
	if (data == KBD_ACK) {
		(void)pckm_pollcmd(MOUSE_PORT,CMD_MSRES);
		(void)pckm_pollcmd(MOUSE_PORT,0x03);
		(void)pckm_pollcmd(MOUSE_PORT,CMD_ENABLE);
		km->km_state |= KM_EXISTS;
		PROBEDEBUG("pckm: found pcmouse\n");
	}
	else
		PROBEDEBUG("pckm: mouse default failed!\n");

        /* the X folks want the /hw/input directory */
        rc = hwgraph_char_device_add(hwgraph_root, "input/keyboard", "pckm",
                                &pckb_vhdl);
        ASSERT(rc == GRAPH_SUCCESS);
        hwgraph_chmod(pckb_vhdl, HWGRAPH_PERM_KBD);
        hwgraph_fastinfo_set(pckb_vhdl, (arbitrary_info_t)kp);
        rc = hwgraph_char_device_add(hwgraph_root, "input/mouse", "pckm",
                        &pcms_vhdl);
        ASSERT(rc == GRAPH_SUCCESS);
        hwgraph_chmod(pcms_vhdl, HWGRAPH_PERM_MOUSE);
        hwgraph_fastinfo_set(pcms_vhdl, (arbitrary_info_t)km);

	init_mutex(&kp->mutex, MUTEX_DEFAULT, "pckm", 0);
	init_mutex(&km->mutex, MUTEX_DEFAULT, "pckm", 0);

	/* Setup the interrupt vector.
	 * However, we may not want the intr context overhead for every
	 * keyboard and mouse interrupt so they are likely to be polled
	 * in the clock handler. 
	 *
	 * setcrimevector(MACE_INTR(5), SPL5, pckm_intr, 0, 0);
	 *
	 */
	setmaceisavector(MACE_INTR(5), (_macereg_t)PCKM_MACEMASK, pckm_intr);
	pckm_enable_interrupt();

}

/*ARGSUSED3*/
static int
pckm_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	queue_t *wq = WR(rq);
	kmport_t *km;

	if (sflag)		/* only a simple streams driver */
		return(ENXIO);

        if (!dev_is_vertex(*devp)) {     /* must be a HWG device */ 
		return(ENODEV);
	}

	km = (kmport_t *)device_info_get(*devp);

	if (km->km_state & KM_ISOPEN) {
		return(EBUSY);
	}

	/* connect device to stream */
	rq->q_ptr = (caddr_t)km;
	wq->q_ptr = (caddr_t)km;
	km->km_wq = wq;
	km->km_rq = rq;

	mutex_lock(&km->mutex, PPCKM);

	/* finish up kmport_t */
	km->km_state |= KM_ISOPEN;

	if ((km->km_state & KM_MOUSE) == 0) {
		/* reset led state */
		kbd_ledstate = 0;
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS);
		(void)pckm_pollcmd(KEYBOARD_PORT,kbd_ledstate);

		/* turn off autorepeat */
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMB);
	} else {
		/* make sure mouse is enabled and running at full res */
		(void)pckm_pollcmd(MOUSE_PORT,CMD_ENABLE);
		(void)pckm_pollcmd(MOUSE_PORT,CMD_MSRES);
		(void)pckm_pollcmd(MOUSE_PORT,0x03);
		(void)pckm_pollcmd(MOUSE_PORT,CMD_ENABLE);
	}
	pckm_enable_interrupt();

	mutex_unlock(&km->mutex);

	return(0);
}

/*ARGSUSED*/
static int
pckm_close(queue_t *rq, int flag, struct cred *crp)
{
	kmport_t *km = (kmport_t *)rq->q_ptr;

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

		mutex_lock(&km->mutex, PPCKM);

		kbd_ledstate = 0;
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS);
		(void)pckm_pollcmd(KEYBOARD_PORT,kbd_ledstate);

		/* turn on autorepeat */
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMBTYPE);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
		(void)pckm_pollcmd(KEYBOARD_PORT,SCN_CAPSLOCK);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
		(void)pckm_pollcmd(KEYBOARD_PORT,SCN_NUMLOCK);
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ENABLE);

		mutex_unlock(&km->mutex);
	}

	return(0);
}

static int
pckm_wput(queue_t *wq, mblk_t *bp)
{
	kmport_t *km = (kmport_t *)wq->q_ptr;

	mutex_lock(&km->mutex, PPCKM);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		/* byte 0 -> reinit mouse/keyboard 
		 *	1 -> set leds
		 */
		if (bp->b_rptr < bp->b_wptr) {
			if (*bp->b_rptr & 0x01)
				pckm_reinit(MOUSE_PORT);
			if (*bp->b_rptr & 0x02)
				pckm_reinit(KEYBOARD_PORT);
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
		mutex_unlock(&km->mutex);
		sdrv_error(wq,bp);
		return(0);
	}

	mutex_unlock(&km->mutex);
	return(0);
}

/* get a new buffer.  should only be called when safe from interrupts.
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
			if (bp = allocb(RMSG_LEN, pri))
				break;
			if (pri == BPRI_HI)
				continue;
			if (!(km->km_state & KM_SE_PENDING)) {
				bp = str_allocb(RMSG_LEN, km->km_rq, BPRI_HI);
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
static void
pckm_rsrv_timer(kmport_t *km)
{

	if ((km->km_state & KM_ISOPEN) == 0) {
		km->km_state &= ~KM_TR_PENDING;
		return;			/* close should have done free */
	}

	if (canenable(km->km_rq)) {
		queue_t *q = km->km_rq;

		km->km_state &= ~KM_TR_PENDING;
		(void)streams_interrupt((strintrfunc_t)qenable, q, 0, 0);
	}
	else {
		km->km_tid = dtimeout(pckm_rsrv_timer, (void *)km, FWDDELAY,
				      plbase, cpuid());
	}

}

static int
pckm_rsrv(queue_t *rq)
{
	kmport_t *km = (kmport_t *)rq->q_ptr;
	mblk_t *bp;

	mutex_lock(&km->mutex,PPCKM);
	if (!canput(rq->q_next)) {	/* quit if upstream congested */
		noenable(rq);
		mutex_unlock(&km->mutex);
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
		mutex_unlock(&km->mutex);
		putnext(rq,bp);		/* send the message w/o blocking */
		mutex_lock(&km->mutex,PPCKM);
	}

	/* get a buffer now, rather than waiting for an interrupt */
	if (!km->km_rbp)
		(void)pckm_getbp(km, BPRI_LO);

	mutex_unlock(&km->mutex);
	return(0);
}

/* When pckm_reinit is called, it is already under mutex_lock */
static void
pckm_reinit(int device)
{

	if (device == KEYBOARD_PORT) {
		/* no need to disable mouse since its logic is different */

		(void)pckm_pollcmd(device,CMD_DISABLE);
		(void)pckm_pollcmd(device,CMD_SETLEDS);
		(void)pckm_pollcmd(device,kbd_ledstate);
		(void)pckm_pollcmd(device,CMD_SELSCAN);
		(void)pckm_pollcmd(device,0x03);
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
		(void)pckm_pollcmd(device,CMD_DISABLE);
		(void)pckm_pollcmd(MOUSE_PORT,CMD_MSRES);
		(void)pckm_pollcmd(MOUSE_PORT,0x03);
		(void)pckm_pollcmd(device,CMD_ENABLE);
	}

	/* enable interrupts */
	pckm_enable_interrupt();

}

void
pckm_rx(kmport_t *km, int data)
{
	mblk_t *bp;

	/*  Check if we get a BAT success or BAT failure data.
	 * This data implies that the keyboard/mouse was just
	 * plugged so it needs to be re-initialized.  Only
	 * look for KBD_BATOK on mouse since the failure case
	 * looks like mouse data.
	 */
	if ((data == KBD_BATOK) || (data == KBD_BATFAIL)) {
		if (km->km_state & KM_MOUSE) {
			if (data == KBD_BATOK) {
				(void)dtimeout(pckm_reinit, (void *)MOUSE_PORT,
					       1, plbase, cpuid());
				return;
			}
		}
		else {
			(void)dtimeout(pckm_reinit, (void *)KEYBOARD_PORT,
				       1, plbase, cpuid());
			return;
		}
	}
			
	/* If not open (normal or textport) drop character.
	 */
	if ((km->km_state & KM_ISOPEN) == 0 && km->km_porthandler == NULL)
		return;

	/* textport should unlock before getting the data */
	if ( (km->km_state & KM_ISOPEN) == 0 && km->km_porthandler) {
		mutex_unlock(&km->mutex);
		(*km->km_porthandler)(data);
		mutex_lock(&km->mutex, PPCKM);
		return;
	}

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
			pckm_rsrv_timer(km);
		}
		else if ((km->km_state & KM_TR_PENDING) == 0) {
			km->km_state |= KM_TR_PENDING;
			km->km_tid = dtimeout(pckm_rsrv_timer, (void *)km,
					     FWDDELAY, plbase, cpuid());
		}
	}
	else
		pckm_nobuf++;
}



/*
 * pckm_intr -- common interrupt handler for keyboard and mouse
 *
 */


#define PORT_0_INTR	0x200
#define PORT_1_INTR	0x800

/*ARGSUSED*/
void
pckm_intr(struct eframe_s *ep, __psint_t pisr)
{
	__uint64_t data;
	int status;
	kmport_t *km;

	/* figure out if its a keyboard or mouse interrupt and get km initialized */
	if (pisr & PORT_0_INTR) {
		if (KEYBOARD_PORT == 0){
			km = &kmports[KEYBOARD_PORT];
		} else {
			km = &kmports[MOUSE_PORT];
		}
	}else {
		if (pisr & PORT_1_INTR) {
			if (KEYBOARD_PORT == 1){
				km = &kmports[KEYBOARD_PORT];
			} else {
				km = &kmports[MOUSE_PORT];
			}
		} else {
			ASSERT(0);
			/* NOTREACHED */
		}
	}

	mutex_lock(&km->mutex, PPCKM);

	while ((status = READ_ISA(status)) & PS2_SR_RBF) {
		data = READ_ISA(rx_buf);

		SYSINFO.rcvint++;
		if (status & (PS2_SR_PARITY|PS2_SR_FRAMING)){
			pckm_parity++;
			switch(mhkm_reset){
			case 1:
				/* Manufacturing needs this */
				pckm_reinit(KEYBOARD_PORT);
				pckm_reinit(MOUSE_PORT);
				break;
			case 0:
				/* normal folk like this */
				pckm_sendok(km);
				outp(km,CMD_RESEND);
				break;
			default:
				/* for debugging mouse errors - do nothing */
				break;
			}

			if (mhkm_print){
				if (mhkm_reset == 1){
				    printf("%s error - status 0x%x - resetting\n", 
					km->km_state & KM_MOUSE ? "mouse" : "keyboard", 
					status);
				}
				if (mhkm_reset == 0){
				    printf("%s error - status 0x%x \n", 
					km->km_state & KM_MOUSE ? "mouse" : "keyboard", 
					status);
				}
			}
		} else
			pckm_rx(km, data);
	}

	mutex_unlock(&km->mutex);

	/*
         * Done servicing a kbd/ms interrupt. 
         * Re-enable interrupt bits in the MACE-ISA mask register.
         */

        mace_mask_enable((_macereg_t)PCKM_MACEMASK);
}

/* 
 * send a byte out to port 
 * if clkinh is set, inhibit clock after transmission. This is used
 * for back to back transmission.
 */
static void
outb(kmport_t *km, int data, int clkinh)
{
	__uint64_t cntrl = READ_ISA(control);
	cntrl |= (PS2_CMD_TxEN | PS2_CMD_CLKASS | clkinh);

	/* 
	 * to send a byte out, we must do the following
	 *
	 * 1) wait for xmission/receive to finish
	 * 2) inhibit clock
	 * 3) write data to transmit buffer 
	 * 4) assert clock and enable transmit to start transfer
	 */

	/* first make sure it is ok to send out */
	pckm_sendok(km);
	
	WRITE_ISA(control, 0);	/* De-assert clken */
	DELAY(100);
	WRITE_ISA(tx_buf, data); /* write data to buffer */
	WRITE_ISA(control, cntrl);
}


/* send a 1-byte command out */
int
_pckm_pollcmd(int port, int cmd)
{
	int sr,timeout;
	unsigned char data;
	int rcnt=2;		/* try up to three times */
	kmport_t *km;

	km = &kmports[port];

again:

	/* send the cmd byte out */
	outb(km, cmd, 0);

	/* wait for report */
	for (timeout=400; timeout ; timeout--) {
		sr = READ_ISA(status);

		if (mace_rev){
			if ((sr & PS2_SR_PARITY) && rcnt) {
				(void) READ_ISA(rx_buf);  /* flush buffer */
				rcnt--;
				goto again;
			}
		}

		if (sr & PS2_SR_RBF) {
			DELAY(2);
			data = READ_ISA(rx_buf);
			if (mace_rev == 0){
			    /* mace interprets the line control bit as start bit of
			     * next character. This causes the actual start bit to 
			     * be interpreted as the first data bit and msb to be 
			     * dropped. If the last data bit (msb) was 1, that also
			     * cause parity bit to be set in the status byte.
			     */
			    data >>= 1;	/* throw away the first bit */
			    if (sr & PS2_SR_PARITY)
				data |= 0x80;	/* fix the msb which shows up as parity */
			}
			if (data == KBD_RESEND && rcnt) {
				rcnt--;
				goto again;
			}
			if (data != KBD_OVERRUN)
				return(data);
		}
		DELAY(250);
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

void
pckm_setleds(int newvalue)
{
	__uint64_t data;

	if (kbd_ledstate == newvalue)
		return;

	kbd_ledstate = newvalue;

	data = pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS);
	if (data != KBD_ACK)
		cmn_err(CE_WARN, "!Unable to get keyboard LED status\n");
	(void)pckm_pollcmd(KEYBOARD_PORT,kbd_ledstate);

	/* re-enable interrupts */
	pckm_enable_interrupt();

}

static void
pckm_sendok(kmport_t *km)
{
	int sr, retry = 20834;			/* ~1/4 second */

	/* wait while xmission/reception is in progress */
	do {
		DELAY(2);
		sr = READ_ISA(status);
	} while ( (sr & (PS2_SR_TIP | PS2_SR_RIP)) && --retry );
	DELAY(2);
}


static void
pckm_flushinp(kmport_t *km)
{
	int retry = 25000;			/* ~1/4 second */

	DELAY(2);
	while ((READ_ISA(status) & PS2_SR_RBF) && --retry) {
		DELAY(2);
		(void) READ_ISA(rx_buf);
		DELAY(2);
	}
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

/* send a command in polled mode */
int
pckm_pollcmd(int port, int cmd)		
{
	int data; 

	data = _pckm_pollcmd(port, cmd);
	return data;
}


/* enable keybd/mouse interrupts.
 * since keybd and mouse are always enables together, we provide
 * all the functionality here, hardcoded. Otherwise there probably
 * is need for specific fields (like intmask) in km structure
 * for setting/clearing bits 
 */

static void
pckm_enable_interrupt()
{
	__uint64_t data, pier;
	kmport_t *km = &kmports[KEYBOARD_PORT];

	/* pciio_pio_* calls are part of CRIME 1.1 PIO read war */
	pier = pciio_pio_read64((volatile uint64_t *)PERIPHERAL_INTMSK_REGISTER);
	pciio_pio_write64((pier | PCKM_MACEMASK), (volatile uint64_t *)PERIPHERAL_INTMSK_REGISTER);

	data = READ_ISA(control); WRITE_ISA(control, (data | PS2_CMD_RxIEN));
	km = &kmports[MOUSE_PORT];
	data = READ_ISA(control); WRITE_ISA(control, (data | PS2_CMD_RxIEN));
}
