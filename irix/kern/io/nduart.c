#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/stream.h"
#include "sys/strids.h"
#include "sys/strmp.h"
#include "sys/stropts.h"
#include "sys/stty_ld.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/termio.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sema.h"
#include "sys/z8530.h"
#include "sys/ddi.h"
#include "sys/capability.h"


/* stream stuff */
static struct module_info dum_info = {
	STRID_DUART,			/* module ID */
	"DUART",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* maximum packet size--infinite */
	128,				/* hi-water mark */
	16,				/* lo-water mark */
};


static void du_rsrv(queue_t *);
struct cred;
static int du_open(queue_t *, dev_t *, int, int, struct cred *);
static int du_close(queue_t *, int, struct cred *);

static struct qinit du_rinit = {
	NULL, (int (*)())du_rsrv, du_open, du_close, NULL, &dum_info, NULL
};

static int du_wput(queue_t *, mblk_t *);
static struct qinit du_winit = {
	du_wput, NULL, NULL, NULL, NULL, &dum_info, NULL
};

int dudevflag = 0;
struct streamtab duinfo = {
	&du_rinit, &du_winit, NULL, NULL
};


duedtinit()
{
}

void
du_conpoll(void)
{
}

int
ducons_write(char *buf, int len)
{
}

void
ducons_flush(void)
{
}

void
du_init(void)
{
}

static int
du_wput(queue_t *wq, mblk_t *bp)
{
}

static int
du_open(queue_t *rq, dev_t *devp, int flag, int sflag, struct cred *crp)
{
}

static int
du_close(queue_t *rq, int flag, struct cred *crp)
{
}

static void
du_rsrv(queue_t *rq)
{
}

tcgeta(queue_t *wq, register mblk_t *bp, struct termio *p)
{
	return 0;
}

int
du_getchar(int port)
{
	return '!';
}

void
du_putchar(int port, unsigned char c)
{
}

/* -------------------- DEBUGGING SUPPORT -------------------- */

void
dump_dport_info(__psint_t n)
{
	qprintf("Not supported.\n");
}

void
reset_dport_info(__psint_t n)
{
	qprintf("Not supported.\n");
}

#ifdef DEBUG
void
dump_dport_actlog(__psint_t n)
{
	qprintf("Not supported.\n");
}
#endif /* DEBUG */
