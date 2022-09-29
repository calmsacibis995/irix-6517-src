/* Copyright 1989, Silicon Graphics Inc., Mountain View, CA. */

/* cn - console driver (graphics/serial/pseudo)
 *
 * NOTE: This module was previously known as console.c.
 *
 * Notes on console MAC strategy:
 * Reads from, and writes to /dev/console are expected to succeed,
 * as are opens for write. If there's an alternate console then
 * only messages which the process is cleared for should appear there.
 * If a writing to the console would violate the MAC policy it's silently
 * discarded.
 */

#ident "$Revision: 3.121 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/invent.h>
#include <sys/hwgraph.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/kopt.h>
#include <sys/major.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stream.h>
#include <sys/strids.h>
#include <sys/strmp.h>
#include <sys/strsubr.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <fs/specfs/spec_lsnode.h>
#include <sys/ddi.h>
#include <sys/hwgraph.h>
#ifdef DEBUG
#include <sys/idbgentry.h>
#endif

#if IP22 || IP26 || IP28 || IP32 || IPMHSIM
/* Support for console=d2 is in prom/kernel */
#define CONSOLE_D2	1
#endif

#ifdef DEBUG
static void idbg_cn(int);
#endif

static int cn_subdev_open(int);

extern int spec_fixgen(vnode_t *);

extern void gfx_earlyinit(void);
extern void tpcons_write(char *, int);
extern void ducons_write(char *, int);
extern void ducons_flush(void);
extern int  tp_init(void);
extern void du_init(void);

#define CONS_TPORT	makedev(TPORT_MAJOR,0)
extern dev_t get_cons_dev(int);
#define	CONS_DUART(n)	get_cons_dev(n)

/*
 * For IP20/IP22/IP26/IP28/IP32 ^A on ttyd1
 *
 * XXXmdf on IP27/IP30, allow console writes to textport even though
 * serial port hasn't been discovered yet
 */
int console_is_tport;

/* cn_mode */
#define CN_GFX		0			/* graphics console */
#ifdef CONSOLE_D2
#define CN_UART1	1			/* uart console 1 */
#define CN_UART2	2			/* uart console 2 */
#else
#define CN_UART		1			/* uart console */
#endif

struct console {
	struct vnode	*cn_rvp;	/* console file pointer */
	struct vnode	*cn_vp;		/* vnode pointer to alt console */
	struct stdata	*cn_stream;	/* stream for console privy device */
	char		*cn_in;		/* input pointer for console output */
	char		*cn_out;	/* output pointer for console output */
	mutex_t		cn_mutex;
	mac_label	*cn_rmac;	/* MAC Label for real console */
	mac_label	*cn_mac;	/* MAC Label for alt console */
	short		cn_initted;	/* obvious initted flag */
	short		cn_mode;	/* console mode gfx/uart */
	int		cn_openflag;	/* flag passed by first open */
	dev_t		cn_rdev;	/* device of console privy device */
	lock_t		cn_lock;	/* lock for handling kernel printfs */
#define CN_BUF_SIZE	1024
	char		cn_buf[CN_BUF_SIZE];
} cn;

#define	CON_LOCK()	mutex_lock(&cn.cn_mutex, PZERO|PLTWAIT)
#define	CON_UNLOCK()	mutex_unlock(&cn.cn_mutex)

/*
 * cnopen:
 *	on first open, build a stream to the appropriate console
 *	device.  this is ultimately determined from the CPU
 *	prom by doing:
 *
 *	>> setenv console [dgG]
 *		d: duart (ttyd1)
#ifdef CONSOLE_D2
 *		d1: duart (ttyd1)
 *		d2: duart (ttyd2)
#endif
 *		g: graphics textport
 *		G: graphics textport [compat with old proms]
 *		possibly more options, but i'm not sure
 */
/* ARGSUSED */
int
cnopen(dev_t *devp, int flag)
{
	int error;

	error = cn_subdev_open(flag);

	return error;
}

/*
 * cnclose:
 *	close and clear the primary console file pointer.
 */
int
cnclose(void)
{
	vnode_t *rvp;
	/* REFERENCED */
	int unused;

	CON_LOCK();

	/* clean up and dealloc stuff */
	rvp = cn.cn_rvp;
	ASSERT(rvp);
	cn.cn_rvp = NULL;
	VOP_CLOSE(rvp, FREAD|FWRITE, L_TRUE, sys_cred, unused);
	cn.cn_rmac = NULL;
	VN_RELE(rvp);
	cn.cn_openflag = 0;
	CON_UNLOCK();
	return 0;
}

/*
 * cnwrite:
 *	write the data to the alternate console if it exists.
 *	otherwise, write to the console stream we built in cnopen().
 */
#define FLAGS (STWRERR|STRDERR|STRHUP|STPLEX)

/* ARGSUSED */
int
cnwrite(dev_t dev, struct uio *uio, struct cred *crp)
{
	struct vnode *vp;
	int error = 0;

	/*
	 * We have a tricky race with streams here. We have a reference
	 * on cn_vp but that doesn't mean that the stream can't be
	 * closed since we do NOT have a file reference. So all these
	 * checks don't really do much good since the v_stream can go away
	 * anywhere during these checks (and more likely while in
	 * strwrite as called from VOP_WRITE waiting for the streams
	 * monitor. The solution involves 2 parts:
	 * 1) protect these checks from deferencing a stale v_stream
	 *	by interlocking this and cn_close. If cn_close
	 *	can't execute then the stream can't finish closing
	 *	so these checks are valid.
	 * 2) in strwrite permit a NULL v_stream for the console case
	 */
	CON_LOCK();
	if ((vp = cn.cn_vp) != NULL && !_MAC_ACCESS(cn.cn_mac,crp,MACWRITEUP)) {
		if ((vp->v_type == VCHR) &&
		    (vp->v_rdev == cn.cn_rdev) &&
		    (vp->v_stream == cn.cn_stream) &&
		    !(vp->v_stream->sd_flag & FLAGS)) {
			VOP_WRITE(vp,uio,0,crp,NULL,error);

			if (error == 0) {
				CON_UNLOCK();
				return 0;
			}
			/* attempt to write to other 'real' console */
		} else {
			cn.cn_vp = NULL;
			VN_RELE(vp);
		}
	}
	CON_UNLOCK();

	ASSERT(cn.cn_rvp != NULL);

	if (_MAC_ACCESS(cn.cn_rmac, crp, MACWRITEUP))
		return 0;

	VOP_WRITE(cn.cn_rvp, uio, 0, crp, NULL, error);
	while (error == EIO) {

		if (spec_fixgen(cn.cn_rvp))
			break;

		VOP_WRITE(cn.cn_rvp, uio, 0, crp, NULL, error);
	}

	return error;
}

/*
 * cnread:
 *	read from the console stream we built in cnopen().
 */
/* ARGSUSED */
int
cnread(dev_t dev, struct uio *uio, struct cred *crp)
{
	int error;

	ASSERT(cn.cn_rvp != NULL);

	if (_MAC_ACCESS(cn.cn_rmac, crp, VREAD))
		return 0;

	VOP_READ(cn.cn_rvp, uio, 0, crp, NULL, error);
	while (error == EIO) {

		if (spec_fixgen(cn.cn_rvp))
			break;

		VOP_READ(cn.cn_rvp, uio, 0, crp, NULL, error);
	}

	return error;
}

/*
 * cnioctl:
 *	send the ioctl to the console stream we built in cnopen().
 */
/* ARGSUSED */
int
cnioctl(dev_t dev, int cmd, void *arg, int flag, struct cred *crp, int *rval,
	struct vopbd *vbds)
{
	int error;

	ASSERT(cn.cn_rvp != NULL);

	if (_MAC_ACCESS(cn.cn_rmac, crp, VREAD))
		return 0;

	VOP_IOCTL(cn.cn_rvp, cmd, arg, flag, crp, rval, vbds, error);
	while (error == EIO) {

		if (spec_fixgen(cn.cn_rvp))
			break;

		VOP_IOCTL(cn.cn_rvp, cmd, arg, flag, crp, rval, vbds, error);
	}

	return error;
}

/*
 * cnpoll:
 *	poll for input data on the stream we built in cnopen().
 */
/* ARGSUSED */
int
cnpoll(dev_t dev, short events, int anyyet, short *reventsp,
       struct pollhead **phpp, unsigned int *genp)
{
	int error;

	ASSERT(cn.cn_rvp != NULL);
	ASSERT(cn.cn_rvp->v_stream != NULL);

	VOP_POLL(cn.cn_rvp, events, anyyet, reventsp, phpp, genp, error);
	while (error == EIO) {

		if (spec_fixgen(cn.cn_rvp))
			break;

		VOP_POLL(cn.cn_rvp, events, anyyet, reventsp,
						phpp, genp, error);
	}

	return error;
}

static void cn_write_stream(void);		/* streams synched cn_write */

/*
 * cn_write:
 *	called by internal kernel routines for output to
 * 	the console.  this can be called at any time regardless
 *	of interrupt level or the mode of the console (duart, gfx, pty).
 *	this is typically used for kernel printfs.
 */
void
cn_write(char *buf, int len)
{
	char *in, *out, *pin;
	int s;

	if (len == 0)
		return;

	/*
	 * if a tty device in the system is privy to console
	 * output (tiocons), package the data up and
	 * and send it to that device.
	 */
	if (cn.cn_vp != NULL) {
		/*
		 * copy all of the data to our our buffer since
		 * we can't depend on the buffer changing before we
		 * have a chance to print it.
		 */
		s = io_splock(cn.cn_lock);
		in = cn.cn_in;
		out = cn.cn_out;
		if (in == out) {	/* only on transition from empty */
			/* we must synchronize with the streams mechanism */
			STREAMS_TIMEOUT((strtimeoutfunc_t)cn_write_stream, 0,
								TIMEPOKE_NOW);
		}
		while (len--) {
			pin = in++;
			if (in == &cn.cn_buf[CN_BUF_SIZE])
				in = &cn.cn_buf[0];
			if (in == out) {
				in = pin;
				break;
			}
			*pin = *buf++;
		}
		cn.cn_in = in;
		io_spunlock(cn.cn_lock,s);
	} else {
		/* check for console mode (UART/GFX) */
		switch (cn.cn_mode) {

		/* send data to graphics textport */
		case CN_GFX:
			tpcons_write(buf, len);
			break;

		/* send data to duart (usually ttyd1) */
#ifdef CONSOLE_D2
		case CN_UART1:
		case CN_UART2:
#else
		case CN_UART:
#endif
			ducons_write(buf, len);
			break;
		}
	}
}

/*
 * this is the STREAMS synchronized version of the cn_write().
 * only here is it safe to do streamish things... not cn_write().
 */
static void
cn_write_stream(void)
{
	queue_t *wq;
	mblk_t *bp;
	struct vnode *vp;
	char *in, *out, *start, *end;
	int s, len;

	vp = cn.cn_vp;
	if ((vp != NULL) &&
	    (vp->v_type == VCHR) &&
	    (vp->v_rdev == cn.cn_rdev) &&
	    (vp->v_stream == cn.cn_stream) &&
	    !(vp->v_stream->sd_flag & (STWRERR|STRDERR|STRHUP|STPLEX))) {
		ASSERT(vp->v_stream->sd_wrq != NULL);
		wq = vp->v_stream->sd_wrq;
		start = &cn.cn_buf[0];
		end = &cn.cn_buf[CN_BUF_SIZE];
		s = io_splock(cn.cn_lock);
		in = cn.cn_in;
		out = cn.cn_out;
		if (in >= out)
			len = in - out;
		else
			len =  ((end - out) + (in - start));
		if (len && canput(wq->q_next)) {
			if ((bp = allocb(len, BPRI_HI)) != NULL) {
				while (out != in) {
					*bp->b_wptr++ = *out++;
					if (out == end)
						out = start;
				}
				cn.cn_out = out;
				io_spunlock(cn.cn_lock,s);
				putnext(wq, bp);
				return;
			}
		}
		cn.cn_out = in;
		io_spunlock(cn.cn_lock,s);
		/* drop it we can't get a buffer or send it upstream... */
	}
}

void
cn_flush(void)
{
	/* If a tty device in the system is privy to console
	 * output (tiocons), skip the flush.
	 */
	if (cn.cn_vp != NULL)
		return;

	/* If the console is on a uart, call it's flush routine. */
	switch (cn.cn_mode) {
#ifdef CONSOLE_D2
	case CN_UART1:
	case CN_UART2:
#else
	case CN_UART:
#endif
		ducons_flush();
		break;
	}
}

/*
 * cn_init:
 *	called to perform console initialization.  this must be
 *	called before the first kernel printf or any console
 *	activity can be invoked.
 */
void
cn_init(int panicflag, int onlygfx)
{
	static char c;

	/*
	 * This is an early initialization of graphics before
	 * the console and, potentially, the textport are
	 * initialized.
	 *
	 * Currently this leaves you in a state where the textport
	 * can run (e.g. textport ucode downloaded).  That's why
	 * it is called from cn_init() instead of mlsetup(), for
	 * example.
	 */
	gfx_earlyinit();

	if (!cn.cn_initted) {
		initnlock(&cn.cn_lock, "CONSOLE");
		mutex_init(&cn.cn_mutex, MUTEX_DEFAULT, "console");
		cn.cn_initted = 1;
		cn.cn_in = cn.cn_out = &cn.cn_buf[0];

		if (is_specified(arg_console)) {
		     if ((arg_console[0] == 'g') || (arg_console[0] == 'G')) {
			  c = CN_GFX;
		     }
#ifdef CONSOLE_D2
		     else if (((arg_console[0] == 'd') || (arg_console[0] == 'D'))
				&& (arg_console[1] == '2')
				&& (arg_console[2] == '\0')) {
		  		c = CN_UART2;
		     }
#endif
		     else {
#ifdef CONSOLE_D2
				c = CN_UART1;
#else
				c = CN_UART;
#endif
			}
		} else {
			c = CN_GFX;
		}
	}

	if (panicflag) {
		if (cn.cn_vp)
			VN_RELE(cn.cn_vp);
		cn.cn_vp = NULL;
	}

	/* If the console environ var is set and graphics is OK, use it */
	switch (c) {
#ifdef CONSOLE_D2
	case CN_UART2 :
		cn.cn_mode = CN_UART2;
		/*  cn_init() is called when closing graphics (when Xsgi
		 * dies), and we do not want to reset all the ttys!
		 */
		if (!onlygfx)
			du_init();
		break ;
#endif

	case CN_GFX :
		cn.cn_mode = CN_GFX;
		if (tp_init()) {
			/*
			 * for IP20/IP22/IP26/IP28/IP32 ^A on ttyd1
			 *
			 * XXXmdf on IP27/IP30, allow console writes
			 * to textport even though serial port hasn't
			 * been discovered yet
			 */
			console_is_tport = 1;
			
			cn.cn_initted = 1;
			return;
		}
		/* ELSE FALL THROUGH... GRAPHICS IS HOSED */
	default :
#ifdef CONSOLE_D2
		cn.cn_mode = CN_UART1;
#else
		cn.cn_mode = CN_UART;
#endif
		/*  cn_init() is called when closing graphics (when Xsgi
		 * dies), and we do not want to reset all the ttys!
		 */
		if (!onlygfx)
			du_init();
		break ;
	}

	cn.cn_initted = 1;

#ifdef DEBUG
	idbg_addfunc("cn", idbg_cn);
#endif
}

int
cnreg(void)
{
#if 0
	/* 
	 * Since the cn driver maintains only a single "cn" structure,
	 * it gets confused if there are multiple specfs nodes simultaneously
	 * in use for the console.  We could end up calling cnclose -- which 
	 * destroys the cn structure -- when one special file is closed, but 
	 * the other is still open.  For this reason, the driver can't handle 
	 * simultaneous use of both /dev/console and /hw/console.
	 *
	 * We could make /dev/console a symlink to /hw/console, but there's
	 * not much point to it.  We'll just remove /hw/console for now and
	 * continue to use /dev/console as an old-style major/minor spec file.
	 *
	 * (Note that if we start a "sleep 100000 < /hw/console &", we're
	 * able to use /dev/console and /hw/console simultaneously, since
	 * cnclose isn't called.)
	 */
	vertex_hdl_t vhdl;
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "console", "cn", &vhdl);
	hwgraph_chmod(vhdl, HWGRAPH_PERM_CONSOLE);
#endif
	return(0);
}

/* Disallow unregister. */
cnunregister()
{
	return(-1);
}

int
cn_is_inited(void)
{
	return cn.cn_initted;
}


/*
 * the following streams module is the console redirection module.
 * the open establishes the redirection and the close cancels it.
 * the put procedures are just straight passthrus.
 */

/*
 * stream module definition
 */
static int cn_open(queue_t *, dev_t *, int, int, struct cred *);
static int cn_close(queue_t *, int, struct cred *);
static int cn_put(queue_t *, mblk_t *bp);
static struct module_info cn_mod_info = {
	STRID_CONSOLE,			/* module ID */
	"CONSOLE",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size */
	256,				/* hi-water mark */
	16,				/* lo-water mark */
};

static struct qinit cn_rinit = {
	cn_put, NULL, cn_open, cn_close,
	NULL, &cn_mod_info, NULL
};

static struct qinit cn_winit = {
	cn_put, NULL, NULL, NULL,
	NULL, &cn_mod_info, NULL
} ;

int cndevflag = D_MP;

struct streamtab cninfo = {
	&cn_rinit, &cn_winit, NULL, NULL
} ;

/*
 * cn_open:
 *	check to see if the requesting stream has permission
 *	to be privy to console output.
 */
/* ARGSUSED */
static int
cn_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	struct stdata *stp;
	struct vnode *vp;

	if (!(sflag & MODOPEN))
		return ENXIO;

	if (*devp)
		return 0; /* REOPEN */

	if (!_CAP_CRABLE(crp, CAP_STREAMS_MGT)) /* only MGR can link */
		return EACCES;

	/* get queue that is the head */
	for (; rq->q_next; rq = rq->q_next)
		;
	stp = rq->q_ptr;

	vp = stp->sd_vnode;
	if (vp == cn.cn_rvp) {		/* can't be console device */
		return EINVAL;
	}
	/* do not wait for the stream to drain on close to avoid races */
	stp->sd_closetime = 0;
	VN_HOLD(vp);
	cn.cn_vp = vp;
	cn.cn_mac = crp->cr_mac;
	cn.cn_rdev = stp->sd_vnode->v_rdev;
	cn.cn_stream = stp->sd_vnode->v_stream;
#ifdef	DEBUG_MAC_TERMIO
	cmn_err(CE_NOTE, "console set by %s console MAC=%c%c",
		get_current_name(), cn.cn_mac->ml_msen_type,
		cn.cn_mac->ml_mint_type);
#endif	/* DEBUG_MAC_TERMIO */
	return 0;
}

/*
 * cn_close:
 *	clean up after the stream that used to be privy to
 *	console output.  this whole driver is really nothing
 *	but a wrapper.
 */
/* ARGSUSED1 */
static int
cn_close(queue_t *rq, int flag, struct cred *crp)
{
	struct stdata *stp;

	/* get queue that is the head */
	for (; rq->q_next; rq = rq->q_next)
		;
	stp = rq->q_ptr;

	CON_LOCK();

	if (cn.cn_vp == stp->sd_vnode) {
		VN_RELE(cn.cn_vp);
		cn.cn_vp = NULL;
		cn.cn_mac = NULL;
	}

	CON_UNLOCK();

	return 0;
}

/*
 * pass data straight through
 */
static int
cn_put(queue_t *qp, mblk_t *bp)
{
	putnext(qp,bp);
	return(0);
}

/*
 * cn_link:
 *	calling this routine causes us to route console
 *	data to an alternate stream.  any streams based tty can
 *	be privy to console output by doing a TIOCCONS ioctl.
 */
int
cn_link(struct vnode *vp, int on)
{
	auto dev_t dev = 0;
	register queue_t *rq;
	int error;

	if (!_CAP_ABLE(CAP_STREAMS_MGT))
		return EACCES;
	if (on) {
		if (vp == cn.cn_vp)
			return 0;	/* This device is already linked
					 * to the console - no need to
					 * do so again.
					 */

		CON_LOCK();
		if (cn.cn_vp) {
			/* Some other stream is linked to the console,
			 * and we are forcefully taking it away.
			 */
			VN_RELE(cn.cn_vp);
			cn.cn_vp = NULL;
			cn.cn_mac = NULL;
		}
		CON_UNLOCK();

		/* Since there is no 'strdrv_pop', once this module
		 * is pushed, it stays pushed. Thus, search for this
		 * module on the stream to ensure that multiple
		 * "TIOCCONS on" to the same device don't result in
		 * multiple pushes of the module.
		 * (The line above 'if (vp == cn.cn_vp)' prevents 2
		 * sequential "TIOCCONS on" to the same device. This
		 * loop prevents scenarios such as "TIOCCONS on,
		 * TIOCCONS off, TIOCCONS on" resulting in multiple
		 * pushes.
		 */
		/* descend the list of write queues */
		rq = vp->v_stream->sd_wrq;
		for (; rq; rq = rq->q_next) {
			if (rq->q_qinfo == &cn_winit){
				VN_HOLD(vp);
				CON_LOCK();
				cn.cn_vp = vp;
				cn.cn_mac = get_current_cred()->cr_mac;
				cn.cn_rdev = vp->v_rdev;
				cn.cn_stream = vp->v_stream;
				CON_UNLOCK();
				return 0;
			}
		}

		error = strdrv_push(RD(vp->v_stream->sd_wrq),
				    "cn", &dev, get_current_cred());
#ifdef DEBUG
		if (!error)
			ASSERT(vp == cn.cn_vp);
#endif
		return error;
	} else {
		CON_LOCK();
		if (vp == cn.cn_vp) {		/* check ownership */
			cn.cn_vp = NULL;	/* take it away */
			cn.cn_mac = NULL;
			VN_RELE(vp);
		}
		CON_UNLOCK();
		return 0;
	}
}

/* The 'subdevice' associated with /dev/console (/dev/ttyd1 if console
 * is set to 'd', /dev/tport if console is set to 'g' or 'G') can
 * get clobbered if a session leader associated with the subdevice exits.
 * e.g. suppose someone logs in to /dev/ttyd1, and subsequently exits.
 * Then vn_kill, called from exit, will clear the vnode associated with
 * /dev/ttyd1. Also, the spec file system looks for all 'aliases' of
 * /dev/ttyd1, and finds the vnode opened here. So it gets vn_killed
 * also.
 * The ad-hoc sematics of /dev/console say this shouldn't happen, so
 * we check for this condition before read/write/ioctl, etc and reopen
 * the device as necessary.
 */
static int
cn_subdev_open(int flag)
{
	auto struct vnode *rvp, *openvp;
	dev_t rdev;
	int error;

	CON_LOCK();
	rvp = cn.cn_rvp;
	if (rvp) {
		CON_UNLOCK();
		return 0;
	}

	if (cn.cn_openflag == 0)
		cn.cn_openflag = flag;
	/* choose appropriate driver for console stream */
	switch (cn.cn_mode) {
	case CN_GFX:
		rdev = CONS_TPORT;
		break;

#ifdef CONSOLE_D2
	case CN_UART1:
		rdev = CONS_DUART(1);
		break;

	case CN_UART2:
		rdev = CONS_DUART(2);
		break;
#else /* !CONSOLE_D2 */
	case CN_UART:
		rdev = CONS_DUART(1);
		break;
#endif
	}

	openvp = rvp = make_specvp(rdev, VCHR);

	ASSERT(rvp);

	VOP_OPEN(openvp, &rvp, cn.cn_openflag, get_current_cred(), error);
	if (error) {
		cmn_err(CE_WARN, "no file for console (error %d)", error);
		VN_RELE(rvp);
		CON_UNLOCK();
		return error;
	}

	cn.cn_rvp = rvp;
	cn.cn_rmac = get_current_cred()->cr_mac;
	CON_UNLOCK();
	return 0;
}

int
iscn(vnode_t *vp)
{
	return vp == cn.cn_vp;
}

#ifdef DEBUG
/* ARGSUSED */
static void
idbg_cn(int x)
{
	qprintf("cn_initted = %d, cn_openflag = %d, ",
				cn.cn_initted, cn.cn_openflag);

	switch (cn.cn_mode) {
	case CN_GFX:
		qprintf("cn_mode = CN_GFX\n");
		break;

#ifdef	CONSOLE_D2
	case CN_UART1:
		qprintf("cn_mode = CN_UART1\n");
		break;

	case CN_UART2:
		qprintf("cn_mode = CN_UART2\n");
		break;

#else
	case CN_UART:
		qprintf("cn_mode = CN_UART\n");
		break;

#endif
	default:
		qprintf("cn_mode = <%d>\n", cn.cn_mode);
		break;
	}

	qprintf("cn_rvp = 0x%x, cn_vp = 0x%x, cn_rdev = 0x%x (%d, %d)\n",
				cn.cn_rvp, cn.cn_vp, cn.cn_rdev,
				emajor(cn.cn_rdev), minor(cn.cn_rdev));

	qprintf("cn_stream = 0x%x, cn_in = 0x%x, cn_out = 0x%x\n",
					cn.cn_stream, cn.cn_in, cn.cn_out);
}
#endif	/* DEBUG */
