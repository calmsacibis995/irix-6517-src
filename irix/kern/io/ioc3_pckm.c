/* Copyright 1996, Silicon Graphics Inc., Mountain View, CA. */
/*
 * streams driver for the PC keyboard mouse controller.  Supported are:
 *	- IOC3 SGI interface controller
 *
 * $Revision: 1.101 $
 */

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/stream.h>
#include <sys/strids.h>
#include <sys/stropts.h>
#include <sys/strmp.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/capability.h>
#include <sys/mac_label.h>
#include <sys/kmem.h>
#include <sys/atomic_ops.h>
#include <sys/idbgentry.h>
#include <sys/ddi.h>
#include <sys/invent.h>
#include <sys/PCI/ioc3.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/pckm.h>
#include <sys/hwgraph.h>

#define IOC3PORT	1

#define PPCKM	PZERO

/* #define PCKM_DEBUG	1 */		/* Help debug the driver */
/* #define BOARD_ADDRESS	1 */	/* Board address(s) can be used with pckm_board */
/*#define KBDHIST		1 */	/* Capture both mouse/keboard events */

#define LOOP_VALUE 	250
#define WAIT_TIME       50
#define SENDOK_OFF	-2

#define PORT0_READ(ioc3)        (((__psunsigned_t)(ioc3))+IOC3_K_RD)
#define PORT1_READ(ioc3)        (((__psunsigned_t)(ioc3))+IOC3_M_RD)
#define PORT0_WRITE(ioc3)       (((__psunsigned_t)(ioc3))+IOC3_K_WD)
#define PORT1_WRITE(ioc3)       (((__psunsigned_t)(ioc3))+IOC3_M_WD)
#define KBMS_STATUS(ioc3)       (((__psunsigned_t)(ioc3))+IOC3_KM_CSR)
#define SIO_IR(ioc3) 		(((__psunsigned_t)(ioc3))+IOC3_SIO_IR)

#define PCI_READ(reg)           (*(volatile __uint32_t *)(reg))
#define PCI_WRITE(reg,val)      *(volatile __uint32_t *)(reg) = (val)
#define PCI_OR(reg,val)		PCI_WRITE(reg,(PCI_READ(reg) |= (val)))
#define PCI_AND(reg,val)	PCI_WRITE(reg,(PCI_READ(reg) &= (val)))

#define KEYBOARD_PORT	0
#define MOUSE_PORT	1L
#define MOUSE_MINOR	2L

/*
 * Delay times for both keyboard/mouse
 *  3 buffered keystrokes for keyboard
 *  1 mouse movement, latency sensitive
 */
#define KEYBOARD_BYTE   12
#define KEYBOARD_DELAY  2
#define MOUSE_BYTE      3
#define MOUSE_DELAY     1

/*
 * Ports on the IOC3 ..
 */
#define PORT0           0
#define PORT1           1

/*
 * _kbd_strat sets the device values ...
 */
#define KEYBOARD_DEVICE 0
#define MOUSE_DEVICE    1

/*
 * From the hardware prospective ... PORT0 is always the keyboard
 * while PORT1 is always the mouse.  For software ... it is not
 * always true.  It is possible that a MOUSE is plugged into
 * PORT0 and a keyboard is plugged into PORT1.
 *
 * The "keyboard_port" and "mouse_port" variables allow have
 * been set with respect to which PORT has which device ....
 *
 */
#define PORT_WRITE(kmp)   \
        ((kmp)->port_no == PORT0)?PORT0_WRITE(kmp->km_board->ioc3_base):  \
        ((kmp)->port_no == PORT1)?PORT1_WRITE(kmp->km_board->ioc3_base):NULL

#define PORT_READ(kmp)    \
        ((kmp)->port_no == PORT0)?PORT0_READ((kmp)->km_board->ioc3_base):   \
        ((kmp)->port_no == PORT1)?PORT1_READ((kmp)->km_board->ioc3_base):NULL

#define DEFAULT_SETTING	0x0
#define ENABLE_OFF	0x1

static const struct pckm_data_info pckm_data_bits [] = {
	KM_RD_VALID_0, KM_RD_DATA_0, KM_RD_DATA_0_SHIFT, KM_RD_FRAME_ERR_0,
	KM_RD_VALID_1, KM_RD_DATA_1, KM_RD_DATA_1_SHIFT, KM_RD_FRAME_ERR_1,
	KM_RD_VALID_2, KM_RD_DATA_2, KM_RD_DATA_2_SHIFT, KM_RD_FRAME_ERR_2,
	0, 0, 0, 0,
};

typedef struct kmboard_s	kmboard_t;
typedef struct kmport_s		kmport_t;

struct kmboard_s {
	vertex_hdl_t		conn_vhdl;	/* connection to pci services */
	vertex_hdl_t		pckb_vhdl;	/* keyboard port vertex */
	vertex_hdl_t		pcms_vhdl;	/* mouse port vertex */

	ioc3_mem_t	       *ioc3_base;	/* PIO pointer to IOC3 chip */
	struct kmport_s	       *ports[2];	/* keyboard and mouse data */

	/*
	 * These are the defined on of following ways :
	 *      ENODEV - no device
	 *      PORT0/PORT1 (see above)
	 * This is for the keyboard/mouse devices to
	 * plugged into any PORT.
	 */
	int keyboard_port;
	int mouse_port;
	int default_key_mouse;	/* Both keyboard/mouse were ENODEV. */
				/* Assume default ports key=0, mouse=1 */

	int kbd_ledstate;
	time_t kbd_lbolt;

	int pckm_nobuf;		/* dropped char count from buffer shortfall */
	int pckm_mparity;	/* count of parity errors for mouse */
	int pckm_kparity;	/* count of parity errors for keyboard */

	mutex_t mutex;		/* protect access to struct */
	int keyboard_id;	/* id returned by keyboard */

	void *ioc3_soft;
#ifdef PCKM_DEBUG
	int		functionkeys;
#endif
};

struct kmport_s {
	vertex_hdl_t	vhdl;			/* this port's vertex */
	int		km_state;		/* status bits */
	int             km_bsize;               /* streams buffer size */
	int             km_fdelay;              /* delay in forwarding packets */
	toid_t		km_tid;			/* timeout to buffer upstream flow */
	int	(*km_porthandler)(unsigned char);/* textport hook */
	queue_t        *km_rq,*km_wq;		/* our queues */
	mblk_t	       *km_rmsg, *km_rmsge;	/* current input message head/tail */
	mblk_t	       *km_rbp;			/* current input buffer */
	kmboard_t      *km_board;
	int		port_no;
#ifdef PCKM_DEBUG
	int		misc_counter;
#endif
};

#define	kmport_set(v,i)	(hwgraph_fastinfo_set((v),(arbitrary_info_t)(i)))
#define	kmport_get(v)	((kmport_t *)hwgraph_fastinfo_get((v)))

#define IS_MOUSE(kmp)			( (kmp)->port_no == (kmp)->km_board->mouse_port )
#define IS_KEY(kmp)			( (kmp)->port_no == (kmp)->km_board->keyboard_port )
#define IS_MOUSE_ENODEV(kmp)		( ENODEV == (kmp)->km_board->mouse_port )
#define IS_KEY_ENODEV(kmp)		( ENODEV == (kmp)->km_board->keyboard_port )
#define DOESNT_EXIST(kmp)		( !((kmp)->km_state & KM_EXISTS) )
#define DOES_EXIST(kmp)			( (kmp)->km_state & KM_EXISTS )

#define KM_EXISTS	0x0001
#define KM_ISOPEN	0x0002
#define KM_SE_PENDING	0x0008
#define KM_TR_PENDING	0x0010
#define KM_IOCONFIG	0x0020		        /* ioconfig has labelled us */

static int		pckm_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp);
static int		pckm_close(queue_t *rq, int flag, struct cred *crp);
static int		pckm_wput(queue_t *wq, mblk_t *bp);
static int		pckm_rsrv(queue_t *rq);

static struct module_info pckm_minfo = {
	0, "PCKM", 0, 1024, 128, 16
};
static struct qinit pckm_rinit = {
	NULL, pckm_rsrv, pckm_open, pckm_close, NULL, &pckm_minfo, NULL
};
static struct qinit pckm_winit = {
	pckm_wput, NULL, NULL, NULL, NULL, &pckm_minfo, NULL
};
struct streamtab pckminfo = {
	&pckm_rinit, &pckm_winit, NULL, NULL
};

int pckmdevflag = D_MP;

extern ioc3_mem_t *console_mem;			       /* in ioc3.c */
static kmboard_t *the_console_km;

/* function table of contents */

static mblk_t	       *pckm_getbp(kmport_t *km, u_int pri);
static void		pckm_rsrv_timer(kmport_t *km);
static void 		pckm_rsrv_timer_lock(kmport_t *km);
static void		pckm_reinit(kmport_t *kmp);
static void		pckm_reinit_lock(kmport_t *kmp);
static void		pckm_reinit_core(kmport_t *kmp);

static void		default_kbd_setting(kmport_t *kmp, int value, int ledstate);
static void		default_ms_setting(kmport_t *kmp);

static void		pckm_rx(kmport_t *km, int data);
static void		pckm_intr(intr_arg_t arg);
static void		pckm_intr_lock(intr_arg_t arg);
static int		pckm_pollcmd(kmport_t *kmp, int cmd);
int			pckm_kbdexists(void);
void			pckm_setleds(int newvalue);
void			pckm_bell(void);

void			gl_setporthandler(int port, int (*fp)(unsigned char));
int			du_keyboard_port(void);

static void		ioc3_pckm_setleds(kmport_t *kmp, int newvalue);
static void		ioc3_pckm_setleds_lock(kmport_t *kmp, int newvalue);
static void		pckm_sendok(kmport_t *kmp);
static void		pckm_flushinp(kmport_t *kmp);

static int		data_check(uint data, int cmd);
static void		valid_data(kmport_t *kmp);
static void		set_ports(kmboard_t *kmb);
static int		check_port(kmport_t *kmp, int port_no);
static void		pckm_restart(kmport_t *kmp);

#ifdef DEBUG
extern void idbg_graph_handles(vertex_hdl_t);
extern void idbg_dump_port(__psunsigned_t);
#endif

#ifdef KBDHIST
static void		updatekbdhist(kmport_t *km, int data);
#endif


/*
 * called by gfx textport init code (pckeyboard.c)
 */
void
early_pckminit(void)
{
}

void
pckminit(void)
{

#ifdef DEBUG
    {
	/* XXX add/delete these upon load/unload */
	extern void idbg_dump_port(__psunsigned_t);
	extern void idbg_dump_board(__psunsigned_t);

	idbg_addfunc("pckm_port", idbg_dump_port);
	idbg_addfunc("pckm_board", idbg_dump_board);
    }
#endif
}

/*
 * pckm_attach: found an IOC3 on PCI that
 * might be carrying pckm/pcms ports.
 */
int
pckm_attach(vertex_hdl_t conn_vhdl)
{
	/*REFERENCED*/
	graph_error_t	rc;
	vertex_hdl_t	pckb_vhdl;
	vertex_hdl_t	pcms_vhdl;
	vertex_hdl_t	ioc3_vhdl;
	vertex_hdl_t	vhdl;
	ioc3_mem_t     *ioc3_base;
	void	       *ioc3_soft;

	kmboard_t *kmb;
	kmport_t *kp;
	kmport_t *mp;

	uint status_reg = 0;
	int data;

	int		isconsole;

	if (!ioc3_subdev_enabled(conn_vhdl, ioc3_subdev_kbms))
		return -1;	/* not brought out to connectors */

#if DEBUG && ATTACH_DEBUG
	cmn_err(CE_CONT,"pckm_attach : %v\n",conn_vhdl);
#endif

	/* when ioc3 takes two connections,
	 * we use the "guest" one. If there
	 * is no such edge, use the one
	 * presented, of course.
	 */
	(void)hwgraph_traverse(conn_vhdl, EDGE_LBL_GUEST, &conn_vhdl);
	    
	/* get back pointer to the ioc3 soft area */
	rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC3, &ioc3_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	ioc3_soft = (void*)hwgraph_fastinfo_get(ioc3_vhdl);

	IOC3_WRITE_IEC(ioc3_soft, SIO_IR_KBD_INT); /* disable intr. */

	ioc3_base = ioc3_mem_ptr(ioc3_soft);

	isconsole = ioc3_is_console(conn_vhdl);

	/*
	 * allocate and set up soft state storage areas
	 */
	kmb = (kmboard_t *)kmem_zalloc(sizeof(kmboard_t), KM_SLEEP);

#ifdef BOARD_ADDRESS
	cmn_err(CE_WARN,"***** pckm_attach board address : 0x%x",kmb);
#endif
	kmb->ioc3_soft = ioc3_soft;
	kmb->ioc3_base = ioc3_base;
	kmb->conn_vhdl = conn_vhdl;
	kmb->ports[0] = (kmport_t *)kmem_zalloc(sizeof(kmport_t), KM_SLEEP);
	kmb->ports[1] = (kmport_t *)kmem_zalloc(sizeof(kmport_t), KM_SLEEP);

	init_mutex(&kmb->mutex, MUTEX_DEFAULT, "pckmb", 0);
	
	kmb->ports[0]->km_state = 0;
	kmb->ports[0]->km_board = kmb;

	kmb->ports[1]->km_state = 0;
	kmb->ports[1]->km_board = kmb;

	kmb->kbd_lbolt = 0;
#ifdef PCKM_DEBUG
	kmb->functionkeys = 0;
#endif
	
	status_reg = KM_CSR_K_CLAMP_THREE | KM_CSR_M_CLAMP_THREE;
	PCI_WRITE(KBMS_STATUS(ioc3_base), status_reg);

	initkbdtype();

	set_ports(kmb);

	/* set up the HWG for keyboard */
	if (kmb->keyboard_port != ENODEV)
		kp = kmb->ports[kmb->keyboard_port];
	else if (kmb->mouse_port == PORT0)
		kp = kmb->ports[PORT1];
	     else
		kp = kmb->ports[PORT0];
		
	rc = hwgraph_char_device_add(conn_vhdl, "pckb", "pckm", &pckb_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(pckb_vhdl, HWGRAPH_PERM_KBD);
	kmb->pckb_vhdl = kp->vhdl = pckb_vhdl;
	kp->km_bsize = KEYBOARD_BYTE;
	kp->km_fdelay = KEYBOARD_DELAY;
#ifdef PCKM_DEBUG
	kp->misc_counter = 0;
#endif
	kmport_set(pckb_vhdl, kp);
	device_inventory_add(pckb_vhdl, INV_MISC, INV_MISC_PCKM, isconsole ? 1 : -1, 0, 1);

	/* probe keyboard with default disable command, then re-enable */
	if (kmb->keyboard_port != ENODEV) {
	    pckm_flushinp(kp);
	    data = pckm_pollcmd(kp, CMD_DISABLE);
	    if (data_check(data,KBD_ACK)) {
		/* init keyboard, leds off, scan mode 3, make/break */
		default_kbd_setting(kp, ENABLE_OFF, 0); 
		
		/* re-enable keyboard */
		data = pckm_pollcmd(kp,CMD_ENABLE);
		if (data_check(data,KBD_ACK)) {
		    kp->km_state |= KM_EXISTS;
		    /* found keyboard! */
		    if (showconfig) {
#if !DEBUG || !ATTACH_DEBUG
			if (isconsole)
#endif
			    cmn_err(CE_CONT,"pckm_attach : %v keyboard enabled\n",pckb_vhdl);
		    }
		}
	    }
	}

	if (showconfig) {
#if !DEBUG || !ATTACH_DEBUG
	  if (isconsole)
#endif
	      if ((kmb->keyboard_port == ENODEV) || 
		  !(kmb->ports[kmb->keyboard_port]->km_state & KM_EXISTS))
		  cmn_err(CE_CONT,"pckm_attach : %v no keyboard found\n",conn_vhdl);
	}

	/* set up the HWG for mouse */
	if (kmb->mouse_port != ENODEV)
		mp = kmb->ports[kmb->mouse_port];
	else if (kmb->keyboard_port == PORT1)
		mp = kmb->ports[PORT0];
	     else
		mp = kmb->ports[PORT1];

	rc = hwgraph_char_device_add(conn_vhdl, "pcms", "pckm", &pcms_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	hwgraph_chmod(pcms_vhdl, HWGRAPH_PERM_MOUSE);
	kmb->pcms_vhdl = mp->vhdl = pcms_vhdl;
	mp->km_bsize = MOUSE_BYTE;
	mp->km_fdelay = MOUSE_DELAY;
#ifdef PCKM_DEBUG
	mp->misc_counter = 0;
#endif
	kmport_set(pcms_vhdl, mp);
	device_inventory_add(pcms_vhdl, INV_MISC, INV_MISC_PCKM, isconsole ? 1 : -1, 0, 0);

	/* probe mouse with defaults, then make sure it is enabled */
	if (kmb->mouse_port != ENODEV) {
	    kmport_t *mp = kmb->ports[kmb->mouse_port];
	    pckm_flushinp(mp);
	    data = pckm_pollcmd(mp,CMD_DEFAULT);
	    if (data_check(data,KBD_ACK)) {
		default_ms_setting(mp);
		mp->km_state |= KM_EXISTS;
		/* found mouse! */
		if (showconfig) {
#if !DEBUG || !ATTACH_DEBUG
		    if (isconsole)
#endif
			cmn_err(CE_CONT,"pckm_attach : %v mouse enabled\n",pcms_vhdl);
		}
	    }
	}

	if (showconfig) {
#if !DEBUG || !ATTACH_DEBUG
	  if (isconsole)
#endif
	      if ((kmb->mouse_port == ENODEV) || 
		  !(kmb->ports[kmb->mouse_port]->km_state & KM_EXISTS))
		  cmn_err(CE_CONT, "pckm_attach : %v no mouse found\n",conn_vhdl);
	}

	if (isconsole) {
#if DEBUG && ATTACH_DEBUG
	    cmn_err(CE_CONT,"pckm_attach : %v is the console keyboard/mouse\n",conn_vhdl);
#endif
	    the_console_km = kmb;
	}

	vhdl = (kmb->keyboard_port != ENODEV) ? pckb_vhdl : pcms_vhdl;
	/*
	 * arrange to receive interrupt calls
	 */
	ioc3_intr_connect(conn_vhdl, SIO_IR_KBD_INT,
			  (ioc3_intr_func_t)pckm_intr,
			  (intr_arg_t)kmb,
			  vhdl,
			  vhdl,
			  0);
	/*
	 * clear and enable the interrupt
	 */
	PCI_WRITE(SIO_IR(kmb->ioc3_base), SIO_IR_KBD_INT);	/* clear intr. */
	PCI_OR(KBMS_STATUS(kmb->ioc3_base), 
	         (kmb->ports[0]->km_state & KM_EXISTS ? KM_CSR_K_TO_EN : 0)
	       | (kmb->ports[1]->km_state & KM_EXISTS ? KM_CSR_M_TO_EN : 0));
	IOC3_WRITE_IES(ioc3_soft, SIO_IR_KBD_INT);	/* enable intr.*/

	return 0;
}

/*ARGSUSED*/
static int
pckm_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	queue_t *wq = WR(rq);
	kmport_t *km;
	vertex_hdl_t vhdl;

	if (sflag)		/* only a simple streams driver */
		return(ENXIO);

	if (dev_is_vertex(*devp)) {
	    vhdl = dev_to_vhdl(*devp);
	    if (vhdl == GRAPH_VERTEX_NONE)
		return ENODEV;
	    
	    km = kmport_get(vhdl);
	    if (km == NULL)
		return ENODEV;

	    if (km->port_no == ENODEV)
		return ENODEV;

	    if (!(km->km_state & KM_IOCONFIG)) {
		/* make hwgraph nodes */
		vertex_hdl_t input_vhdl;
		vertex_hdl_t other_vhdl;
		int unit;
		int other_unit;
		int rc;
		int is_kb;
		char name[20];

		atomicSetInt(&km->km_state, KM_IOCONFIG);
		is_kb = km->port_no == km->km_board->keyboard_port;

		/*
		 * - If our counterpart exists, and he already has a unit
		 * number, use it.
		 * - If he exists, but has none, assign him ours, and use
		 * the one ioconfig gave us.
		 * - If he does not exist, just use the one ioconfig gave us.
		 *
		 * This so that keyboard/mouse on same board have same number.
		 */
		unit = device_controller_num_get(vhdl);
		rc = hwgraph_traverse(vhdl, is_kb ? "../pcms" : "../pckb", &other_vhdl);
		ASSERT(rc == GRAPH_SUCCESS || rc == GRAPH_NOT_FOUND);
		if (rc == GRAPH_SUCCESS) {
		    other_unit = device_controller_num_get(other_vhdl);
		    if (other_unit == -1) {
			device_controller_num_set(other_vhdl, unit);
#if DEBUG && ATTACH_DEBUG
			cmn_err(CE_CONT, "%v : setting unit on %v to %d\n",
				  vhdl, other_vhdl, unit);
#endif
		    }
		}
		if (rc == GRAPH_SUCCESS && other_unit != -1) {
		    unit = other_unit;
#if DEBUG && ATTACH_DEBUG
		    cmn_err(CE_CONT, "%v : using unit %d from %v\n",
			      vhdl, unit, other_vhdl);
#endif
		}
		if (unit <= 0) {
		    cmn_err(CE_WARN, 
			    "%v: unit number %d from ioconfig is invalid",
			    vhdl, unit);
		    return EIO;
		}
		/*
		 * the X folks want the /hw/input* directories to be called
		 * /hw/input, /hw/input1, ... to match Everest precedent.
		 */
		if (unit == 1) {
		    strcpy(name,"input");
		} else {
		    sprintf(name,"input%d",unit-1);
		}
		rc = hwgraph_path_add(hwgraph_root, name, &input_vhdl);
		ASSERT(rc == GRAPH_SUCCESS);

		if (vhdl == km->km_board->pckb_vhdl)
			rc = hwgraph_edge_add(input_vhdl, vhdl, "keyboard");
		else if (vhdl == km->km_board->pcms_vhdl)
			rc = hwgraph_edge_add(input_vhdl, vhdl, "mouse");
			
		ASSERT(rc == GRAPH_SUCCESS);
	    }
	} else {
	    return ENODEV;
	}

	/*
	 * Disallow multiple opens.
	 */
	if (km->km_state & KM_ISOPEN) {
		return(EBUSY);
	}

	/* connect device to stream */
	rq->q_ptr = (caddr_t)km;
	wq->q_ptr = (caddr_t)km;

	/* save the wq/rq in the port structure */
	km->km_wq = wq;
	km->km_rq = rq;

	mutex_lock(&km->km_board->mutex, PPCKM);

	km->km_board->kbd_ledstate = 0;

	if (IS_KEY(km)) {
		/* reset led state */
		(void)pckm_pollcmd(km,CMD_SETLEDS);
		(void)pckm_pollcmd(km,km->km_board->kbd_ledstate);
		km->km_bsize = KEYBOARD_BYTE;
		km->km_fdelay = KEYBOARD_DELAY;

		/* turn off autorepeat */
		(void)pckm_pollcmd(km,CMD_ALLMB);
	}

	if (IS_MOUSE(km)) {
		km->km_bsize = MOUSE_BYTE;
		km->km_fdelay = MOUSE_DELAY;
		default_ms_setting(km);
	}

	km->km_state |= KM_ISOPEN;
	mutex_unlock(&km->km_board->mutex);

	return(0);
}

/*ARGSUSED*/
static int
pckm_close(queue_t *rq, int flag, struct cred *crp)
{
	kmport_t *km = (kmport_t *)rq->q_ptr;

	mutex_lock(&km->km_board->mutex, PPCKM);

	if (km->km_state & KM_TR_PENDING)
		untimeout(km->km_tid);

	km->km_state &= ~(KM_ISOPEN|KM_TR_PENDING);

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

	if (!IS_MOUSE(km)) {
		/* reset led state */
		km->km_board->kbd_ledstate = 0;
		default_kbd_setting(km, DEFAULT_SETTING, km->km_board->kbd_ledstate);
	}

	mutex_unlock(&km->km_board->mutex);

	return(0);
}

static int
pckm_wput(queue_t *wq, mblk_t *bp)
{
        kmport_t *kmp = (kmport_t *)wq->q_ptr;

	mutex_lock(&kmp->km_board->mutex, PPCKM);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		/* byte 0 -> reinit mouse/keyboard 
		 *	1 -> set leds
		 */
		if (bp->b_rptr < bp->b_wptr) {
      if (   *bp->b_rptr == 0xfc) {
           /* toss 0xfc cmds and the data */
        if((bp->b_wptr - bp->b_rptr) > 1) bp->b_rptr++;
      }
      else if (   *bp->b_rptr == 0xfa) { /* he has asked for key repeat */
        (void)pckm_pollcmd(kmp,CMD_ALLMBTYPE);
        (void)pckm_pollcmd(kmp,CMD_KEYMB);
        (void)pckm_pollcmd(kmp,SCN_CAPSLOCK);
        (void)pckm_pollcmd(kmp,CMD_KEYMB);
        (void)pckm_pollcmd(kmp,SCN_NUMLOCK);
        (void)pckm_pollcmd(kmp,CMD_ENABLE);
      } else {

			if (   *bp->b_rptr & 0x01
			    && kmp->km_board->keyboard_port != ENODEV) {
				pckm_reinit_lock(kmp->km_board->ports[kmp->km_board->keyboard_port]);
			}
			if (   *bp->b_rptr & 0x02
			    && kmp->km_board->mouse_port != ENODEV)
				pckm_reinit_lock(kmp->km_board->ports[kmp->km_board->mouse_port]);
			bp->b_rptr++;
		}
		}
		if (bp->b_rptr < bp->b_wptr
		    && kmp->km_board->keyboard_port != ENODEV
		    && !IS_MOUSE(kmp)) {
			ioc3_pckm_setleds_lock(kmp, *bp->b_rptr);
			bp->b_rptr++;
		}

		freemsg(bp);
		break;
	case M_IOCTL:
		/* only should get a TCSETA ioctl, which we just eat */
		freemsg(bp);
		break;
	case M_CTL:
		if (*bp->b_rptr == 0xde ) {
			/*
		 	 * it's a query for keyboard type.  Byte 0 of 
			 * data remains 0xde, byte 1 is id.
			 */
			if (IS_KEY_ENODEV(kmp))
				*bp->b_wptr++ = ENODEV;
			else
				*bp->b_wptr++ = kmp->km_board->keyboard_id;

			qreply(wq, bp);
		}
		break;
	default:
		mutex_unlock(&kmp->km_board->mutex);
		sdrv_error(wq,bp);
		return(0);
	}

	mutex_unlock(&kmp->km_board->mutex);
	return(0);
}

/* get a new buffer.  should only be called when safe from interrupts.
 */
static mblk_t *
pckm_getbp(kmport_t *km, u_int pri)
{
	mblk_t *bp, *rbp;

	rbp = km->km_rbp;
	/* if current buffer exists and is empty, do not need another buffer */
	ASSERT(rbp ? (rbp->b_rptr <= rbp->b_wptr) : 1);
	if (rbp && rbp->b_rptr == rbp->b_wptr) {
		bp = NULL;
	} else {
		/* else if no current buffer exists, or exists and not empty,
		   allocate new buffer */
		while (1) {
			if (bp = allocb(km->km_bsize, pri))
				break;
			if (pri == BPRI_HI)
				continue;
			if (!(km->km_state & KM_SE_PENDING)) {
				bp = str_allocb(km->km_bsize, km->km_rq, BPRI_HI);
				if (!bp)
					km->km_state |= KM_SE_PENDING;
			}
			break;
		}
	}

	if (!rbp)
		/* if no current buffer exists, make the new one current */
		km->km_rbp = bp;
	else if (bp || rbp->b_wptr >= rbp->b_datap->db_lim) {
		/* else if we have a new buffer (and thus a current one), or
		 * the current buffer is full, save current buffer and make
		 * new one current.  In the latter case,
		 * new one (bp) may be 0 if pri != BPRI_HI.
		 */
		str_conmsg(&km->km_rmsg, &km->km_rmsge, rbp);
		km->km_rbp = bp;
	}

	ASSERT( (pri == BPRI_HI) ? km->km_rbp != 0 : 1 );
	return(bp);
}

/* heartbeat to send input upstream.  used to reduce the large cost of
 * trying to send each input byte upstream by itself.
 */
static void
pckm_rsrv_timer(kmport_t *km)
{
	mutex_lock(&km->km_board->mutex,PPCKM);
	pckm_rsrv_timer_lock(km);
	mutex_unlock(&km->km_board->mutex);
}

static void
pckm_rsrv_timer_lock(kmport_t *km)
{
	if ((km->km_state & KM_ISOPEN) == 0) {
		km->km_state &= ~KM_TR_PENDING;
		return;			/* close should have done free */
	}

	if (canenable(km->km_rq)) {
		km->km_state &= ~KM_TR_PENDING;
		qenable(km->km_rq);
	} else {
		km->km_tid = STREAMS_TIMEOUT(pckm_rsrv_timer,(void *)km,km->km_fdelay);
	}
}

static int
pckm_rsrv(queue_t *rq)
{
	kmport_t *km = (kmport_t *)rq->q_ptr;
	mblk_t *bp;

	mutex_lock(&km->km_board->mutex,PPCKM);

	if (!canput(rq->q_next)) {	/* quit if upstream congested */
		noenable(rq);
		mutex_unlock(&km->km_board->mutex);
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
		mutex_unlock(&km->km_board->mutex);
		putnext(rq,bp);		/* send the message w/o blocking */
		mutex_lock(&km->km_board->mutex,PPCKM);
	}

	/* get a buffer now, rather than waiting for an interrupt */
	if (!km->km_rbp)
		(void)pckm_getbp(km, BPRI_LO);

	mutex_unlock(&km->km_board->mutex);

	return(0);
}

static void
pckm_reinit(kmport_t *kmp)
{
	mutex_lock(&kmp->km_board->mutex,PPCKM);
	pckm_reinit_lock(kmp);
	mutex_unlock(&kmp->km_board->mutex);
}

static void
pckm_reinit_lock(kmport_t *kmp)
{
	/* disable intr. */
	IOC3_WRITE_IEC(kmp->km_board->ioc3_soft, SIO_IR_KBD_INT);

	pckm_reinit_core(kmp);

	/* clear intr. */
	PCI_WRITE(SIO_IR(kmp->km_board->ioc3_base), SIO_IR_KBD_INT);
	/* enable intr. */
	IOC3_WRITE_IES(kmp->km_board->ioc3_soft, SIO_IR_KBD_INT);
}

static void
pckm_reinit_core(kmport_t *kmp)
{
	if (kmp->port_no == kmp->km_board->keyboard_port) {
		(void)pckm_pollcmd(kmp,CMD_DISABLE);
		default_kbd_setting(kmp, DEFAULT_SETTING, 
				    kmp->km_board->kbd_ledstate);

		/* XXXwtk return if a beeping keyboard ... keyboards switched 
		 * hot plugged. Do here???
		 */

		if (kmp->km_state & KM_ISOPEN)
			(void)pckm_pollcmd(kmp,CMD_ALLMB);	/* turn off autorepeat */
	}
	else {
		default_ms_setting(kmp);
	}
}

static void
default_kbd_setting(kmport_t *kmp, int value, int ledstate)
{	
	(void)pckm_pollcmd(kmp,CMD_SETLEDS);
	(void)pckm_pollcmd(kmp,ledstate);	/* should be 0 all leds off */
	(void)pckm_pollcmd(kmp,CMD_SELSCAN);
	(void)pckm_pollcmd(kmp,3);

	/* turnon autorepeat */
	(void)pckm_pollcmd(kmp,CMD_ALLMBTYPE);
	(void)pckm_pollcmd(kmp,CMD_KEYMB);
	(void)pckm_pollcmd(kmp,SCN_CAPSLOCK);
	(void)pckm_pollcmd(kmp,CMD_KEYMB);
	(void)pckm_pollcmd(kmp,SCN_NUMLOCK);
	if (!(value & ENABLE_OFF))
		(void)pckm_pollcmd(kmp,CMD_ENABLE);
}

static void
default_ms_setting(kmport_t *kmp)
{
	(void)pckm_pollcmd(kmp,CMD_MSRES);
	(void)pckm_pollcmd(kmp,0x03);
	(void)pckm_pollcmd(kmp,CMD_ENABLE);
}

static void
pckm_rx(kmport_t *km, int data)
{
	mblk_t 	*bp;
	int 	local_data;

	/* Check if we get a BAT success or BAT failure data.
	 * This data implies that the keyboard/mouse was just
	 * plugged so it needs to be re-initialized.  Only
	 * look for KBD_BATOK on mouse since the failure case
	 * looks like mouse data.
	 */

	if (IS_MOUSE(km) && (data == KBD_BATOK)) {
		local_data = pckm_pollcmd(km, CMD_MSSTAT);
		if (data_check(local_data,KBD_ACK)) {
			local_data = pckm_pollcmd(km, SENDOK_OFF);
			if (data_check(local_data, MOUSE_ID)) { 	
				dtimeout(pckm_reinit,(void *)km,1,plbase,cpuid());
			}
		} else {
			default_ms_setting(km);
		}
	}

	if (IS_KEY(km) && ((data == KBD_BATOK) || (data == KBD_BATFAIL))) {
		local_data = pckm_pollcmd(km, CMD_SELSCAN);
		if (data_check(local_data,KBD_ACK)) 
			local_data = pckm_pollcmd(km, MOUSE_ID); /* What scan code type? */

		if (data_check(local_data,KBD_ACK))
			local_data = pckm_pollcmd(km, SENDOK_OFF);

		if (data_check(local_data, 0x2)) {
			dtimeout(pckm_reinit,(void *)km,1,plbase,cpuid());
			return;
		}

		default_kbd_setting(km, DEFAULT_SETTING, km->km_board->kbd_ledstate);
		if (km->km_state & KM_ISOPEN)
		    (void)pckm_pollcmd(km,CMD_ALLMB); 	/* turn off autorepeat */
	}
			
	/*
	 * If the device is open and km->km_porthandler is
	 * non-NULL, we should set it to NULL.  This detaches the
	 * textport as soon as we receive a keystroke after having
	 * been opened by a process.  This will achieve the desired
	 * result (textport stays up until someone else really needs
	 * the keyboard) if two assumptions are met.  The first
	 * assumption is that, even though we may be opened by
	 * ioconfig several times during boot (which should not cancel
	 * the textport), we will never actually receive a keystroke
	 * while we are open.  This is a scary assumption but probably
	 * valid (and luckily, the worst failure mode for this case is
	 * that kernel textport does not respond to the keyboard and
	 * the user reboots).  The other assumption is that nobody
	 * other than ioconfig will open the keyboard and then close
	 * it again before receiving any keystrokes.  (Or if they do,
	 * it's OK for the textport to continue receiving keystrokes.)
	 */
	if (km->km_porthandler && (km->km_state & KM_ISOPEN))
		km->km_porthandler = NULL;	 
		
	/* 
	 * If not open (normal or textport) drop character.
	 */
	if ((km->km_state & KM_ISOPEN) == 0 && km->km_porthandler == NULL)
		return;

	/* textport should unlock before getting the data */
	if (km->km_porthandler) {
		mutex_unlock(&km->km_board->mutex);
		(*km->km_porthandler) (data);
		mutex_lock(&km->km_board->mutex, PPCKM);
		return;
	}

	/* We require getting a buffer.  */
	bp = km->km_rbp;
	if (bp == 0 || bp->b_wptr == bp->b_datap->db_lim) {
	    atomicAddInt(&km->km_board->pckm_nobuf, 1);
	    (void)pckm_getbp(km, BPRI_HI);
	    bp = km->km_rbp;
	}

	ASSERT(bp != 0);

	ASSERT(bp->b_wptr < bp->b_datap->db_lim);

	*bp->b_wptr = data;

	/* next to last buffer ... go out an try and get another one. */
	if ((++bp->b_wptr) == bp->b_datap->db_lim)
		(void)pckm_getbp(km, BPRI_LO);

#ifdef KBDHIST
	updatekbdhist(km, data);
#endif

	/* 
	 * Forward caps lock and numlock post haste since LEDs
	 * must be tripped at interrupt level, try and turn LED
	 * request quickly.
	 */
	if ((data == SCN_CAPSLOCK) || (data == SCN_NUMLOCK)) {
	    if (km->km_state & KM_TR_PENDING)
		untimeout(km->km_tid);
	    STREAMS_INTERRUPT(pckm_rsrv_timer,km,0,0);
	} else if ((km->km_state & KM_TR_PENDING) == 0) {
	    km->km_state |= KM_TR_PENDING;
	    km->km_tid = STREAMS_TIMEOUT(pckm_rsrv_timer,(void *)km,
					 km->km_fdelay);
	}
}

static void
pckm_intr(intr_arg_t arg)
{
	kmboard_t *kmb = (kmboard_t *)arg;

	mutex_lock(&kmb->mutex, PPCKM);
	pckm_intr_lock(arg);
	mutex_unlock(&kmb->mutex);
	
}

static void
pckm_intr_lock(intr_arg_t arg)
{
	kmboard_t *kmb = (kmboard_t *)arg;
	uint data;
	int  port;

	/* 
	 * Check for other error flags and if data is "VALID" :
	 * 	FRAME_ERR_* or OFLO
	 * In either case ... ask for a resend of char ...
	 *
	 * Upon transmit timeout, print message, and then disable
	 * further timeout interrupts on that port. XXXwtw - best way?
	 *
	 * For now don't worry about parity problems ... later wtk :
	 * 	pckm_mparity,
	 * 	pckm_kparity, 
	 */

	/* XXX check for what the status is from this ioc3 
	   we get and intr when plugging back any device ...
	   problem is that we need to know when to call set_ports
	   or re-init a individual port
	 */
	data = PCI_READ(KBMS_STATUS(kmb->ioc3_base));

	if ((data & KM_CSR_K_TO_EN) && (data & KM_CSR_K_TO)) {
	    cmn_err(CE_WARN, "ioc3_pckm: port 0 stuck");
	    PCI_OR(KBMS_STATUS(kmb->ioc3_base), KM_CSR_K_PULL_CLK);
	    us_delay(1600);
	    PCI_AND(KBMS_STATUS(kmb->ioc3_base), 
			~(KM_CSR_K_PULL_CLK | KM_CSR_K_TO_EN));
	}

	if ((data & KM_CSR_M_TO_EN) && (data & KM_CSR_M_TO)) {
	    cmn_err(CE_WARN, "ioc3_pckm: port 1 stuck");
	    PCI_OR(KBMS_STATUS(kmb->ioc3_base), KM_CSR_M_PULL_CLK);
	    us_delay(1600);
	    PCI_AND(KBMS_STATUS(kmb->ioc3_base), 
			~(KM_CSR_M_PULL_CLK | KM_CSR_M_TO_EN));
	}

	/* Check both ports for data ... maybe need to reinit device */
	for (port = PORT0; port <= PORT1 ; port++)	
		valid_data(kmb->ports[port]);

	PCI_WRITE(SIO_IR(kmb->ioc3_base), SIO_IR_KBD_INT);  /* ack intr.  */
        IOC3_WRITE_IES(kmb->ioc3_soft, SIO_IR_KBD_INT); /* enable intr. */
}

static int
pckm_pollcmd(kmport_t *kmp, int cmd)
{
        uint read_data;
	int retry;

	retry = LOOP_VALUE;
	if (cmd != SENDOK_OFF) {
        	pckm_sendok(kmp);
        	PCI_WRITE(PORT_WRITE(kmp),cmd);
	}

	do {
	    us_delay(WAIT_TIME);
	    read_data = PCI_READ(PORT_READ(kmp));
	} while(((read_data & KM_RD_VALID_ALL) == 0) && --retry);

        return(read_data);
}

/******** begin textport/console routines ********
 *
 */
int
pckm_kbdexists(void)
{
    if (the_console_km == 0 || the_console_km->keyboard_port == ENODEV)
	return 0;
    else 
	return ((the_console_km->ports[the_console_km->keyboard_port]->km_state
		 & KM_EXISTS) != 0);
}

void
pckm_setleds(int newvalue)
{

    if (the_console_km == 0 || the_console_km->keyboard_port == ENODEV)
	return;

    ioc3_pckm_setleds(the_console_km->ports[the_console_km->keyboard_port],
		      newvalue);
}

void
pckm_bell(void)
{
    if (the_console_km == 0 || the_console_km->keyboard_port == ENODEV)
	return;

    ioc3_pckm_setleds(the_console_km->ports[the_console_km->keyboard_port],
		      the_console_km->kbd_ledstate | 0x8);
}

/*ARGSUSED*/
void
gl_setporthandler(int port, int (*fp)(unsigned char))
{
    if (the_console_km == 0 || the_console_km->keyboard_port == ENODEV)
	return;
    
    the_console_km->ports[the_console_km->keyboard_port]->km_porthandler = fp;
}

int
du_keyboard_port(void)
{
    return -1;			/* value is passed to gl_setporthandler,
				   and is ignored */
}
/*
 *
 ******** end textport/console routines ********/
static void
ioc3_pckm_setleds(kmport_t *kmp, int newvalue)
{
	mutex_lock(&kmp->km_board->mutex,PPCKM);
	ioc3_pckm_setleds_lock(kmp, newvalue);
	mutex_unlock(&kmp->km_board->mutex);	
}

static void
ioc3_pckm_setleds_lock(kmport_t *kmp, int newvalue)
{
	uint data, retry;
	kmboard_t *kmb = kmp->km_board;
	time_t delta;

	if (kmb->kbd_ledstate == (newvalue & 0x1F))
		return;

	if (newvalue & 0x18) {
		delta = lbolt - kmb->kbd_lbolt;
		if ((delta < 14 && (newvalue & 0x8)) || 
		    (delta < 44 && (newvalue & 0x10)))
			return;
		kmb->kbd_lbolt = lbolt;
	}

	kmb->kbd_ledstate = newvalue & 0x1F;

	retry = 0;
	/* 
	 * Send the keyboard a message ..
	 * next byte is a led/beep byte.
	 *
	 * The keyboard will stop scanning an wait
	 * for this byte (yes, forever).
	 * 
	 * When we get a ACK, the keyboard has gone back,
	 * into scan mode.  If we don't find the ACK ...
	 * retry ("retry" times).  Otherwise reinit the
	 * keyboard ...
	 */

again :
	data = pckm_pollcmd(kmp, CMD_SETLEDS);
	if (data_check(data,KBD_ACK)) {
		data = pckm_pollcmd(kmp, kmb->kbd_ledstate);
		if (!data_check(data,KBD_ACK)) {
			data = pckm_pollcmd(kmp, kmb->kbd_ledstate);  /* retry once */
			if (!data_check(data,KBD_ACK)) {
				pckm_reinit_core(kmp); 
			}
		}
	} else {
		retry++;
		if (retry <= 1) /* loop only twice */
			goto again;

		/* 
		 * Don't want to get caught with keyboard hung 
		 * trying to enable the keyboard.
		 */
		data = pckm_pollcmd(kmp,CMD_ENABLE);
		if (!data_check(data,KBD_ACK)) 
			default_kbd_setting(kmp, DEFAULT_SETTING, kmb->kbd_ledstate);
	}

	/* turn off beep bits */
	if (newvalue & 0x18)
		kmb->kbd_ledstate &= 0x7;
}

static void
pckm_sendok(kmport_t *kmp)
{
	uint status_data;
	uint write_bit;
	int retry;

	retry = LOOP_VALUE;
	write_bit = (kmp->port_no == PORT0) ? KM_CSR_K_WRT_PEND : KM_CSR_M_WRT_PEND;
	status_data = PCI_READ(KBMS_STATUS(kmp->km_board->ioc3_base));
	while ((status_data & write_bit) && --retry) {
		us_delay(WAIT_TIME);
		status_data = PCI_READ(KBMS_STATUS(kmp->km_board->ioc3_base));
	}
}

static void
pckm_flushinp(kmport_t *kmp)
{
	PCI_READ(PORT_READ(kmp));
}

static int 
data_check(uint data, int cmd)
{
	const struct pckm_data_info *p;
	int ack = 0;

	for (p = pckm_data_bits; p->valid_bit && !ack; p++) {
		if (PCKM_DATA_VALID(data))
			if (PCKM_DATA_SHIFT(data) == cmd)
				ack++;
	}

	if (ack) 
		return(1);

	return(0);
}

/* Called only from pckm_intr ... already under mutex lock */
static void
valid_data(kmport_t *kmp)
{
	const struct pckm_data_info *p;
	uint data, retry;

	/* Get data and check for any valid bits ... else return  */
	while ((data = PCI_READ(PORT_READ(kmp))) && 
		((KM_RD_VALID_0 & data) || 
		 (KM_RD_VALID_1 & data) || 
		 (KM_RD_VALID_2 & data))) {
		retry = 0;

skip :

#ifdef PCKM_DEBUG
	if (KM_RD_OFLO & data) 
		cmn_err(CE_WARN,"OVERFLOW ... ");
#endif

	/*
	 * If we get data that has a framing error ...
	 * ask the device to resend the last byte.
	 * 
	 * The reason for doing this is because 
	 * when plugging in the keyboard/mouse,
	 * sometimes the BAT data looks like a "framing
	 * error" ... asking the device to resend this
	 * data will allow us to reinit the device correctly.
	 */
	        for (p = pckm_data_bits; p->valid_bit; p++) {
	                if (PCKM_DATA_VALID(data) && DOES_EXIST(kmp)) {
				SYSINFO.rcvint++;
				pckm_rx(kmp, PCKM_DATA_SHIFT(data));
			} else if (PCKM_GOT_DATA(data)) {
				if (DOESNT_EXIST(kmp))
					pckm_restart(kmp);
				else if (PCKM_FRAME_ERR(data)) {
					retry++;
					data = pckm_pollcmd(kmp,KBD_RESEND);
					if (retry > 2) /* escape the loop/drop data */
						break;
					goto skip;
				}
			}
       		}
	} /* end of while */
}

static void
set_ports(kmboard_t *kmb)
{
	int port_no;
	int id;

	kmb->default_key_mouse = 0;
	kmb->keyboard_port = kmb->mouse_port = ENODEV;
	kmb->ports[0]->port_no = kmb->ports[1]->port_no = ENODEV;
	for (port_no = PORT0; port_no <= PORT1; port_no++) {
		id = check_port(kmb->ports[port_no], port_no);
		switch(id) {
		case KEYBD_ID :
		case KEYBD_BEEP_ID2 :
	    		if (kmb->keyboard_port != ENODEV)
				cmn_err(CE_ALERT,"%v only one keyboard allowed",kmb->conn_vhdl);
	    		kmb->keyboard_port = port_no;
	    		kmb->keyboard_id = id;
	    	break;
		case MOUSE_ID :
	    		if (kmb->mouse_port != ENODEV)
				cmn_err(CE_ALERT,"%v only one mouse allowed",kmb->conn_vhdl);
	    		kmb->mouse_port = port_no;
	    	break;
		}
		kmb->ports[port_no]->port_no = port_no;
    	}

	if ((kmb->keyboard_port == ENODEV) && (kmb->mouse_port == ENODEV))
		kmb->default_key_mouse = 1;
}

static int
check_port(kmport_t *kmp, int port_no)
{
        const struct pckm_data_info *p;
        int ack, id_resp, device_id, beep_id, retry;
        uint data;

	kmp->port_no = port_no;
        beep_id = ack = id_resp = retry = 0;
        device_id = ENODEV;

        pckm_flushinp(kmp);
        data = pckm_pollcmd(kmp, CMD_ID);

	while ((device_id == ENODEV) && (retry < 3)) {
		retry++;
        	for (p = pckm_data_bits; p->valid_bit; p++) {
                	if (PCKM_DATA_VALID(data) && (device_id == ENODEV)) {
                        	switch(PCKM_DATA_SHIFT(data)) {
                        	case KBD_ACK :
                                	ack++;
                                	break;
                        	case CMD_ID_RESP :
                                	id_resp++;
                                	break;
                        	case KEYBD_ID :
					if (ack && id_resp)
						device_id = KEYBD_ID;
					break;
				case KEYBD_BEEP_ID2 :
					beep_id++;
					break;
				case KEYBD_BEEP_ID1 :
					if (ack && beep_id)
						device_id = KEYBD_BEEP_ID2;
					break;
                        	case MOUSE_ID :
					if (ack)
                                		device_id = MOUSE_ID;
                                	break;
                        	}
			}
        	}

		if (device_id == ENODEV) {
			if (ack) {
				us_delay(300000); /* delay 300,000 micro seconds */
				data = PCI_READ(PORT_READ(kmp));
			} else {
        			pckm_flushinp(kmp);
				data = pckm_pollcmd(kmp, CMD_ID);
			}
		}

	} /* End of while */

	return(device_id);
}

static void
pckm_restart(kmport_t *kmp)
{
	int id;

        id = check_port(kmp->km_board->ports[kmp->port_no], kmp->port_no);

        switch(id) {
        case KEYBD_ID :
        case KEYBD_BEEP_ID2 :
		if (kmp->km_board->keyboard_port != ENODEV) {
			cmn_err(CE_ALERT,"%v only one keyboard allowed",
				kmp->km_board->conn_vhdl); 

			/* 
			 * Since we found two of the same devices ... 
		 	 * Turn previous one "OFF" so we can probe it for the correct device
			 * next time.
			 */
			if (kmp->port_no == PORT0)
				kmp->km_board->ports[PORT1]->km_state &= ~KM_EXISTS;
			else
				kmp->km_board->ports[PORT0]->km_state &= ~KM_EXISTS;
					
		}

		/* Check if we need to enforce the default ports */
		if (kmp->km_board->default_key_mouse && 
		    kmp->port_no != PORT0) {
			cmn_err(CE_ALERT,"Keyboard installed in incorrect port!");
			return;
		}
			
		kmp->km_board->keyboard_port = kmp->port_no;
		kmp->km_board->keyboard_id = id;
            break;
        case MOUSE_ID :
		if (kmp->km_board->mouse_port != ENODEV) {
			cmn_err(CE_ALERT,"%v only one mouse allowed",
				kmp->km_board->conn_vhdl);

			/*
			 * See above comment ...
			 */
			if (kmp->port_no == PORT0)
				kmp->km_board->ports[PORT1]->km_state &= ~KM_EXISTS;
			else
				kmp->km_board->ports[PORT0]->km_state &= ~KM_EXISTS;
		}

		/* Check if we need to enforce the default ports */
		if (kmp->km_board->default_key_mouse && 
		    kmp->port_no != PORT1) {
			cmn_err(CE_ALERT,"Mouse installed in incorrect port!");
			return;
		}

		kmp->km_board->mouse_port = kmp->port_no;
            break;
        }
        kmp->km_board->ports[kmp->port_no]->port_no = kmp->port_no;

        kmp->km_state |= KM_IOCONFIG;

        if (IS_KEY(kmp)) {
                default_kbd_setting(kmp, DEFAULT_SETTING, kmp->km_board->kbd_ledstate);
		kmp->km_bsize = KEYBOARD_BYTE;
		kmp->km_fdelay = KEYBOARD_DELAY;
		kmp->km_state |= KM_EXISTS;

		if (kmp->km_state & KM_ISOPEN)
		    (void)pckm_pollcmd(kmp,CMD_ALLMB);	/* turn off autorepeat */
	}

        if (IS_MOUSE(kmp)) {
                default_ms_setting(kmp);
		kmp->km_bsize = MOUSE_BYTE;
		kmp->km_fdelay = MOUSE_DELAY;
		kmp->km_state |= KM_EXISTS;
	}

	PCI_OR(KBMS_STATUS(kmp->km_board->ioc3_base),
		(kmp->port_no == PORT0) ? KM_CSR_K_TO_EN : KM_CSR_M_TO_EN);
}

#ifdef KBDHIST
/* KEYBOARD_BYTE is defined currently to be 12 */
#define MOUSEHIST 100
int kbdhist[KEYBOARD_BYTE];
int mousehist[MOUSEHIST];
static void
updatekbdhist(kmport_t *km, int data)
{
	int i;
	if (IS_KEY(km)) {
		for (i=0; i < KEYBOARD_BYTE-1; i++)
			kbdhist[i] = kbdhist[i+1];
		kbdhist[KEYBOARD_BYTE-1] = data;
#ifdef PCKM_DEBUG
		/* 
		 * Check for function keys F1 - F8 we will see a 	
		 * two of these per real key stroke. Example,
		 * F1 = 07,f0,07 data ...
		 */
		switch (data) {
			case 0x33 : /* H */
			case 0x3b : /* J */
			case 0x42 : /* K */
			case 0x4b : /* L */
				atomicAddInt(&km->km_board->functionkeys, 1);
			break;
		}
#endif
	} else {
		for (i=0; i < MOUSEHIST-1; i++)
			mousehist[i] = mousehist[i+1];
		mousehist[MOUSEHIST-1] = data;
	}
}
#endif

#ifdef DEBUG
void
idbg_dump_board(__psunsigned_t arg)
{
	kmboard_t *kb = (kmboard_t *)arg;
	char devnm[MAXDEVNAME];

	qprintf("IOC3_BASE = 0x%llx, ports[] = 0x%llx, 0x%llx\n",
		kb->ioc3_base, kb->ports[0], kb->ports[1]);
	qprintf("kb_port = %d, mouse_port = %d, kb_id = 0x%x\n",
		kb->keyboard_port, kb->mouse_port, kb->keyboard_id);
	qprintf("#nobuf = %d\n", kb->pckm_nobuf);
	dev_to_name(kb->conn_vhdl,devnm,MAXDEVNAME);
	qprintf("conn_vhdl = %x %s\n",kb->conn_vhdl, devnm);
	dev_to_name(kb->pckb_vhdl,devnm,MAXDEVNAME);
	qprintf("pckb_vhdl = %x %s\n",kb->pckb_vhdl, devnm);
	dev_to_name(kb->pcms_vhdl,devnm,MAXDEVNAME);
	qprintf("pcms_vhdl = %x %s\n",kb->pcms_vhdl, devnm);
	qprintf("ioc3_soft = 0x%x\n",kb->ioc3_soft);
	qprintf("kbd_ledstate : 0x%x\n",kb->kbd_ledstate);
#ifdef PCKM_DEBUG
	qprintf("functionkeys : %d\n", kb->functionkeys);
#endif

	idbg_dump_port((__psunsigned_t)kb->ports[0]);
	idbg_dump_port((__psunsigned_t)kb->ports[1]);
}

void
idbg_dump_port(__psunsigned_t arg)
{	
	kmport_t *kp = (kmport_t *)arg;	
	char devnm[MAXDEVNAME];

	qprintf("\nKM_STATE=0x%x, port_no = %d\n",
		kp->km_state, kp->port_no);
	qprintf("km_bsize=%d, km_fdelay = %d\n",
		kp->km_bsize, kp->km_fdelay);
	dev_to_name(kp->vhdl,devnm,MAXDEVNAME);
	qprintf("vhdl=0x%x %s\n",kp->vhdl,devnm);
	qprintf("km_rq=0x%x, km_wq = 0x%x\n",
		kp->km_rq,kp->km_wq);
	qprintf("km_rbp = 0x%x\n",kp->km_rbp);
#ifdef PCKM_DEBUG
	qprintf("misc_counter = %d\n",kp->misc_counter);
#endif
}
#endif

