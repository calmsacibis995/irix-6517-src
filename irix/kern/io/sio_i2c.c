/*
 * Copyright (C) 1986, 1992, 1993, 1994, Silicon Graphics, Inc.
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
 * This is a lower level module for the modular serial i/o driver. This
 * module implements I2C entry points.
 */

#include <ksys/serialio.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/hwgraph.h>
#include <sys/kmem.h>
#include <ksys/elsc.h>
#include <sys/cmn_err.h>
#include <sys/nodepda.h>
#include <sys/iograph.h>

#include "sys/ddi.h"
#include "sys/pda.h"

#include <sys/SN/module.h>

#ifdef DEBUG
#include <sys/idbgentry.h>
#endif

#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

/* local port info for the I2C serial ports. This contains as its
 * first member the global sio port private data.
 */

typedef struct i2cport {
    sioport_t	sioport;	/* must be first struct entry! */
    int		notify;
    toid_t	heartbeat_toid;
    int		flags;
    module_t	*module;	/* containing module, for ELSC struct and lock */
} i2cport_t;

/* flags */
#define I2C_HEARTBEAT_ENB	1
#define I2C_HWERR		2

/* get local port type from global sio port type */
#define LPORT(port) ((i2cport_t*)(port))

/* get global port from local port type */
#define GPORT(port) ((sioport_t*)(port))

/* local functions: */
static int i2c_open		(sioport_t *port);
static int i2c_write		(sioport_t *port, char *buf, int len);
static void i2c_wrflush		(sioport_t *port);
static int i2c_read		(sioport_t *port, char *buf, int len);
static int i2c_notification	(sioport_t *port, int mask, int on);
static int i2c_set_extclk	(sioport_t *port, int clock_factor);

static int i2c_set_proto	(sioport_t *port, enum sio_proto proto);

static int return_0		(void){return 0;}
static int return_1		(void){return 1;}

static void enable_heartbeat(i2cport_t *);
static void disable_heartbeat(i2cport_t *);
static void heartbeat(i2cport_t *);
#ifdef DEBUG
int heartbeat_cnt;
#endif

/* Entry points to this module. Most do nothing in the case
 * of the I2C
 */
static struct serial_calldown i2c_calldown = {
    i2c_open,
    (int(*)(sioport_t*,int,int,int,int,int))return_0,	/* config */
    (int(*)(sioport_t*,int))	return_1,	/* enable HFC */
    i2c_set_extclk,
    i2c_write,
    i2c_write,
    i2c_wrflush,
    (int(*)(sioport_t*,int))	return_0,	/* break */
    (int(*)(sioport_t*,int))	return_0,	/* enable TX */
    i2c_read,
    i2c_notification,
    (int(*)(sioport_t*,int))	return_1,	/* RX timeout */
    (int(*)(sioport_t*,int))	return_0,	/* set DTR */
    (int(*)(sioport_t*,int))	return_0,	/* set RTS */
    (int(*)(sioport_t*))	return_1,	/* query DCD */
    (int(*)(sioport_t*))	return_1,	/* query CTS */
    i2c_set_proto,
};

/*
 * Boot time initialization of this module
 */
void
i2cinit()
{
    i2cport_t *port, *lastport = 0;
    graph_error_t rc;
    vertex_hdl_t vhdl;
    moduleid_t lastmodule = INVALID_MODULE;
    nodepda_t *nodepda_p;
    cpuid_t cpu;
    char name[256];
    struct sio_lock *lockp;

#ifdef DEBUG
    if ((caddr_t)0 != (caddr_t)&(((i2cport_t*)0)->sioport))
	panic("sioport is not first member of i2cport struct\n");
#endif

    for(cpu = 0; cpu < MAXCPUS; cpu++) {
	if (pdaindr[cpu].CpuId != cpu)
	    continue;
	
	nodepda_p = pdaindr[cpu].pda->p_nodepda;

	/* if this is the same as the last module, this is the same
	 * elsc port, just attach it. Otherwise we need to scan the
	 * hwgraph to see if there is already an elsc under this
	 * module, and create one if there isn't
	 */
	if (nodepda_p->module_id == lastmodule) {
	    pdaindr[cpu].pda->p_elsc_portinfo = lastport;
	    continue;
	}

	sprintf(name, EDGE_LBL_MODULE "/%d/" EDGE_LBL_ELSC "/tty",
		nodepda_p->module_id);
	
	/* if path already exists, grab the port info from it and stuff
	 * it into the pda, then continue.
	 */
	if (hwgraph_traverse(hwgraph_root, name, &vhdl) == GRAPH_SUCCESS) {
	    pdaindr[cpu].pda->p_elsc_portinfo = device_info_get(vhdl);
	    continue;
	}

	/* create a port structure */
	port = (i2cport_t*)kmem_zalloc(sizeof(i2cport_t), KM_NOSLEEP);
	pdaindr[cpu].pda->p_elsc_portinfo = port;

	/* attach the calldown hooks so upper layer can call our
	 * routines.
	 */
	port->sioport.sio_calldown = &i2c_calldown;
	
	/* create the containing directory */
	rc = hwgraph_path_add(hwgraph_root, name, &vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	rc = rc;
	
	hwgraph_fastinfo_set(vhdl, (arbitrary_info_t)port);
	
	/* perform upper layer initialization */
	sio_initport(GPORT(port), vhdl, NODE_TYPE_D | NODE_TYPE_CHAR, 0);

	lockp = (struct sio_lock*)kmem_zalloc(sizeof(struct sio_lock), 0);
	ASSERT(lockp);
	/* i2c uart may dump kernel panic messages, so we must lock at spl7 */
	lockp->lock_levels[0] = SIO_LOCK_SPL7;
	sio_init_locks(lockp);
	GPORT(port)->sio_lock = lockp;

	port->module = nodepda_p->module;

	lastmodule = nodepda_p->module_id;
	lastport = port;
    }
}

/* Start heartbeat if needed
 */
static void
enable_heartbeat(i2cport_t *p)
{
    void *tmp;

    ASSERT(sio_port_islocked(GPORT(p)));

    if (p->heartbeat_toid == 0) {
	/* start heartbeat */
        p->heartbeat_toid = dtimeout(heartbeat, p, HZ, plbase, cpuid());

	/* KLUDGE! we can't do a cmn_err here that will result in i2c
	 * uart output since that would recurse. So we'll temporarily
	 * clear and then restore the pda pointer so the output will
	 * go to the normal console only.
	 */
	tmp = private.p_elsc_portinfo;
	private.p_elsc_portinfo = 0;
	cmn_err(CE_WARN, "I2C uart port in use. "
		"System performance will be degraded");
	private.p_elsc_portinfo = tmp;
    }

    p->flags |= I2C_HEARTBEAT_ENB;
}

/* disable heartbeat */
static void
disable_heartbeat(i2cport_t *p)
{
    ASSERT(sio_port_islocked(GPORT(p)));

    p->flags &= ~I2C_HEARTBEAT_ENB;
}

/* polling heartbeat. Since we don't have interrupts to check if
 * output is flushed or input is available, we poll once per second when
 * the upper layer has asked us to via a notification request.  When
 * all notification requests are removed, the heartbeat is disabled
 * and we stop rearming the heartbeat
 */
static void
heartbeat(i2cport_t *p)
{
    sioport_t *gp = GPORT(p);
    int s;

    SIO_LOCK_PORT(gp);

#ifdef DEBUG
    heartbeat_cnt++;
#endif

    if (p->flags & I2C_HEARTBEAT_ENB)
        p->heartbeat_toid = dtimeout(heartbeat, p, HZ, plbase, cpuid());
    else
	p->heartbeat_toid = 0;

    s = mutex_spinlock_spl(&p->module->elsclock, splerr);
    elsc_process(&p->module->elsc);
    mutex_spinunlock(&p->module->elsclock, s);

    /* invoke upper layer to produce output bytes */
    if (p->notify & N_OUTPUT_LOWAT)
	UP_OUTPUT_LOWAT(gp);

    /* invoke upper layer to deal with any input bytes */
    if (p->notify & N_DATA_READY)
	UP_DATA_READY(gp);

    SIO_UNLOCK_PORT(gp);
}

static int
i2c_open(sioport_t *port)
{
    ASSERT(L_LOCKED(port, L_OPEN));
    if (LPORT(port)->flags & I2C_HWERR)
	return(-1);
    return(0);
}

/*ARGSUSED*/
static int
i2c_write(sioport_t *port, char *buf, int len)
{
    int olen = len, s;
    i2cport_t *p = LPORT(port);

    ASSERT(L_LOCKED(port, L_WRITE));

    if (p->flags & I2C_HWERR)
	return(-1);

    s = mutex_spinlock_spl(&p->module->elsclock, splerr);

    while (len > 0) {
	if (_elscuart_putc(&p->module->elsc, *buf) < 0) {
	    mutex_spinunlock(&p->module->elsclock, s);
	    p->flags |= I2C_HWERR;
	    return(-1);
	}
	len--;
	buf++;
    }

    mutex_spinunlock(&p->module->elsclock, s);
    return(olen - len);
}

/*ARGSUSED*/
static void
i2c_wrflush(sioport_t *port)
{
    int s;
    i2cport_t *p = LPORT(port);

    ASSERT(L_LOCKED(port, L_WRITE));

    if (p->flags & I2C_HWERR)
	return;

    s = mutex_spinlock_spl(&p->module->elsclock, splerr);

    if (_elscuart_flush(&p->module->elsc) < 0)
	p->flags |= I2C_HWERR;

    mutex_spinunlock(&p->module->elsclock, s);
}

/*
 * read in bytes from the hardware. Return the number of bytes
 * actually read
 */
static int
i2c_read(sioport_t *port, char *buf, int len)
{
    int olen = len, c, s;
    i2cport_t *p = LPORT(port);

    ASSERT(L_LOCKED(port, L_READ));

    if (p->flags & I2C_HWERR)
	return(-1);

    s = mutex_spinlock(&p->module->elsclock);

    while(len > 0 &&
	  _elscuart_poll(&p->module->elsc) &&
	  (c = _elscuart_getc(&p->module->elsc)) != -1) {
	*buf++ = c;
	len--;
    }

    mutex_spinunlock(&p->module->elsclock, s);
    return(olen - len);
}

/*
 * set event notification
 */
static int
i2c_notification(sioport_t *port, int mask, int on)
{
    i2cport_t *p = LPORT(port);

    ASSERT(L_LOCKED(port, L_NOTIFICATION));

    ASSERT(mask);

    if (on)
	p->notify |= mask;
    else
	p->notify &= ~mask;

    if (p->notify & (N_OUTPUT_LOWAT | N_DATA_READY))
	enable_heartbeat(p);
    else
	disable_heartbeat(p);

    return(0);
}

/*
 * set external clock
 */
/*ARGSUSED*/
static int
i2c_set_extclk(sioport_t *port, int clock_factor)
{
    ASSERT(L_LOCKED(port, L_SET_EXTCLK));
    return(clock_factor);
}

/* ARGSUSED */
static int
i2c_set_proto(sioport_t *port, enum sio_proto proto)
{
    ASSERT(L_LOCKED(port, L_SET_PROTOCOL));
    /* only support rs232 */
    return(proto != PROTO_RS232);
}
