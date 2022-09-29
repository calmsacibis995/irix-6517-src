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
 * Definitions for the modular serial i/o driver.
 *
 * The modular serial i/o driver is a driver which has the hardware
 * dependent and hardware independent parts separated into separate
 * modules. The upper half is responsible for all hardware independent
 * operations, specifically the interface to the kernel. An upper half
 * may implement a streams interface, character interface, or whatever
 * interface it wishes to the kernel. The same upper half may access any
 * physical hardware through a set of standardized entry points into the
 * lower level, which deals directly with the hardware. Whereas a
 * separate upper layer exists for each kernel interface type (streams,
 * character, polling etc), a separate lower level exists for each
 * hardware type supported. Any upper and lower layer pair may be
 * connected to form a complete driver. This file defines the interface
 * between the two
 */
#ifndef __KSYS_SERIALIO_H__
#define __KSYS_SERIALIO_H__

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/termio.h>
#include <sys/stream.h>
#if DEBUG
#include <sys/kthread.h>
#endif
#include <ksys/ddmap.h>

enum sio_lock_levels {
    SIO_LOCK_UNUSED,	/* Must be first */
    SIO_LOCK_MUTEX,
    SIO_LOCK_SPLHI,
    SIO_LOCK_SPL7
};

struct sio_lock {
    lock_t spinlock;
    mutex_t mutex;
    int token;
    char flags;
    char lock_levels[3];
    char max_level;
#ifdef DEBUG
    cpuid_t lock_cpu;
    kthread_t *lock_kptr;
    caddr_t lock_ra;
#endif
};

/* Definitions needed per port by both layers. Each lower layer
 * declares a set of per-port private data areas describing each
 * physical port, and by definition the first member of that private
 * data is the following structure. Thus a pointer to the lower
 * layer's private data is interchangeable with a pointer to the
 * common private data, and the upper layer does not allocate anything
 * so it does need to know anything about the physical configuration
 * of the machine. This structure may also contain any hardware
 * independent info that must be persistent across device closes.
 */
typedef struct sioport {
    /* calling vectors */
    struct serial_calldown	*sio_calldown;
    struct serial_callup	*sio_callup;

    void	*sio_upper;	/* upper layer's private data area */

    vertex_hdl_t sio_vhdl;	/* vertex handle of the hardware independent
				 * portion of this port (e.g. tty/1 without
				 * the d,m,f, etc)
				 */
    struct sio_lock *sio_lock;

    int		sio_flags;
#if DEBUG
    int		sio_lockcalls;	/* bitmask of which downcalls require lock */
#endif
} sioport_t;

/* bits for sio_flags */
#define SIO_HWGRAPH_INITED	0x1
#define SIO_SPINLOCK_HELD	0x2
#define SIO_MUTEX_HELD		0x4
#define SIO_LOCKS_MASK (SIO_SPINLOCK_HELD | SIO_MUTEX_HELD)

#if DEBUG
/* bits for sio_lockcalls, one per downcall except du_write which is
 * not called by an upper layer.
 */
#define L_OPEN		0x0001
#define L_CONFIG	0x0002
#define L_ENABLE_HFC	0x0004
#define L_SET_EXTCLK	0x0008
#define L_WRITE		0x0010
#define L_BREAK		0x0020
#define L_READ		0x0040
#define L_NOTIFICATION	0x0080
#define L_RX_TIMEOUT	0x0100
#define L_SET_DTR	0x0200
#define L_SET_RTS	0x0400
#define L_QUERY_DCD	0x0800
#define L_QUERY_CTS	0x1000
#define L_SET_PROTOCOL	0x2000
#define L_ENABLE_TX	0x4000

#define L_LOCK_ALL	(~0)

/* debug lock assertion: each lower layer entry point does an
 * assertion with the following macro, passing in the port passed to
 * the entry point and the bit corresponding to which entry point it
 * is. If the upper layer has turned on the bit for that entry point,
 * sio_port_islocked is called, thus an upper layer may specify that
 * it is ok for a particular downcall to be made without the port lock
 * held.
 */
#define L_LOCKED(port, flag) (((port)->sio_lockcalls & (flag)) == 0 || \
			      sio_port_islocked(port))
#endif

extern void *the_console_port;
#define IS_CONSOLE(p) ((void*)(p) == the_console_port)

/* flags for next_char_state */
#define NCS_BREAK 1
#define NCS_PARITY 2
#define NCS_FRAMING 4
#define NCS_OVERRUN 8

/* calldown vector. This is a set of entry points into a lower layer
 * module, providing black-box access to the hardware by the upper
 * layer
 */
struct serial_calldown {

    /* hardware configuration */
    int (*down_open)		(sioport_t *port);
    int (*down_config)		(sioport_t *port, int baud, int byte_size,
				 int stop_bits, int parenb, int parodd);
    int (*down_enable_hfc)	(sioport_t *port, int enable);
    int (*down_set_extclk)	(sioport_t *port, int clock_factor);

    /* data transmission */
    int (*down_write)		(sioport_t *port, char *buf, int len);
    int (*down_du_write)	(sioport_t *port, char *buf, int len);
    void (*down_du_flush)	(sioport_t *port);
    int (*down_break)		(sioport_t *port, int brk);
    int (*down_enable_tx)	(sioport_t *port, int enb);

    /* data reception */
    int (*down_read)		(sioport_t *port, char *buf, int len);
    
    /* event notification */
    int (*down_notification)	(sioport_t *port, int mask, int on);
    int (*down_rx_timeout)	(sioport_t *port, int timeout);

    /* modem control */
    int (*down_set_DTR)		(sioport_t *port, int dtr);
    int (*down_set_RTS)		(sioport_t *port, int rts);
    int (*down_query_DCD)	(sioport_t *port);
    int (*down_query_CTS)	(sioport_t *port);

    /* transmission protocol */
    int (*down_set_protocol)	(sioport_t *port, int protocol);

    /* memory mapped user driver support */
    int (*down_mapid)		(sioport_t *port, void *arg);
    int (*down_map)		(sioport_t *port, vhandl_t *vt, off_t off);
    void (*down_unmap)		(sioport_t *port);
    int (*down_set_sscr)	(sioport_t *port, int arg, int flag);
};

/*
 * Macros used by the upper layer to access the lower layer. Unless
 * otherwise noted, all integer functions should return 0 on success
 * or 1 if the hardware does not support the requested operation. In
 * the case of non-support, the upper layer may work around the problem
 * where appropriate or just notify the user.
 * For hardware which supports detaching, these functions should
 * return -1 if the hardware is no longer present.
 */

/* open a device. Do whatever initialization/resetting necessary */
#define DOWN_OPEN(p) \
    ((p)->sio_calldown->down_open(p))

/* configure the hardware with the given baud rate, number of stop
 * bits, byte size and parity
 */
#define DOWN_CONFIG(p, a, b, c, d, e) \
    ((p)->sio_calldown->down_config(p, a, b, c, d, e))

/* Enable hardware flow control. If the hardware does not support
 * this, the upper layer will emulate HFC by manipulating RTS and CTS
 */
#define DOWN_ENABLE_HFC(p, enb) \
    ((p)->sio_calldown->down_enable_hfc(p, enb))

/* Set external clock to the given clock factor. If cf is zero,
 * internal clock is used. If cf is non-zero external clock is used
 * and the clock is cf times the baud.
 */
#define DOWN_SET_EXTCLK(p, cf) \
    ((p)->sio_calldown->down_set_extclk(p, cf))

/* Write bytes to the device. The number of bytes actually written is
 * returned. The upper layer will continue to call this function until
 * it has no more data to send or until 0 is returned, indicating that
 * no more bytes may be sent until some have drained.
 */
#define DOWN_WRITE(p, buf, len) \
    ((p)->sio_calldown->down_write(p, buf, len))

/* Same as DOWN_WRITE, but called only from synchronous du output
 * routines. Allows lower layer the option of implementing kernel
 * printfs differently than ordinary console output.
 */
#define DOWN_DU_WRITE(p, buf, len) \
    ((p)->sio_calldown->down_du_write(p, buf, len))

/* Flushes previous down_du_write() calls.  Needed on serial controllers
 * that can heavily buffer output like IOC3 for conbuf_flush().
 */
#define DOWN_DU_FLUSH(p) \
     ((p)->sio_calldown->down_du_flush(p))

/* Set the output break condition high or low */
#define DOWN_BREAK(p, brk) \
    ((p)->sio_calldown->down_break(p, brk))

/* Enable/disable TX for soft flow control */
#define DOWN_ENABLE_TX(p) \
    ((p)->sio_calldown->down_enable_tx(p, 1))
#define DOWN_DISABLE_TX(p) \
    ((p)->sio_calldown->down_enable_tx(p, 0))

/* Read bytes from the device. The number of bytes actually read is
 * returned. All bytes returned by a single call have the same error
 * status. Thus if the device has 10 bytes queued for input and byte 5
 * has a parity error, the first call to DOWN_READ will return bytes 0-4
 * only. A subsequent call to DOWN_READ will first cause a call to
 * UP_PARITY_ERROR to notify the upper layer that the next byte has an
 * error, and then the call to DOWN_READ returns byte 5 alone. A
 * subsequent call to DOWN_READ returns bytes 6-9. The upper layer
 * continues to call DOWN_READ until 0 is returned, or until it runs out
 * of buffer space to receive the chars.
 */
#define DOWN_READ(p, buf, len) \
    ((p)->sio_calldown->down_read(p, buf, len))

/* Turn on/off event notification for the specified events. Notification
 * status is unchanged for those events not specified.
 */
#define DOWN_NOTIFICATION(p, mask, on) \
    ((p)->sio_calldown->down_notification(p, mask, on))

/* Notification types. 1 per upcall. The upper layer can specify
 * exactly which upcalls it wishes to receive. UP_DETACH is mandatory
 * when applicable and cannot be enabled/disabled.
 */
#define N_DATA_READY	0x01
#define N_OUTPUT_LOWAT	0x02
#define N_BREAK		0x04
#define N_PARITY_ERROR	0x08
#define N_FRAMING_ERROR	0x10
#define N_OVERRUN_ERROR	0x20
#define N_DDCD		0x40
#define N_DCTS		0x80

#define N_ALL_INPUT	(N_DATA_READY | N_BREAK |			\
			 N_PARITY_ERROR | N_FRAMING_ERROR |		\
			 N_OVERRUN_ERROR | N_DDCD | N_DCTS)

#define N_ALL_OUTPUT	N_OUTPUT_LOWAT

#define N_ALL_ERRORS	(N_PARITY_ERROR | N_FRAMING_ERROR | N_OVERRUN_ERROR)

#define N_ALL		(N_DATA_READY | N_OUTPUT_LOWAT | N_BREAK |	\
			 N_PARITY_ERROR | N_FRAMING_ERROR |		\
			 N_OVERRUN_ERROR | N_DDCD | N_DCTS)

/* Instruct the lower layer that the upper layer would like to be
 * notified every t ticks when data is being received. If data is
 * streaming in, the lower layer should buffer enough data that
 * notification is not required more often than requested, and set a
 * timeout so that notification does not occur less often than
 * requested. If the lower layer does not support such operations, it
 * should return 1, indicating that the upper layer should emulate these
 * functions in software.
 */
#define DOWN_RX_TIMEOUT(p, t) \
    ((p)->sio_calldown->down_rx_timeout(p, t))

/* Set the output value of DTR */
#define DOWN_SET_DTR(p, dtr) \
    ((p)->sio_calldown->down_set_DTR(p, dtr))

/* Set the output value of RTS */
#define DOWN_SET_RTS(p, rts) \
    ((p)->sio_calldown->down_set_RTS(p, rts))

/* Query current input value of DCD */
#define DOWN_QUERY_DCD(p) \
    ((p)->sio_calldown->down_query_DCD(p))

/* Query current input value of CTS */
#define DOWN_QUERY_CTS(p) \
    ((p)->sio_calldown->down_query_CTS(p))

/* Set transmission protocol */
#define DOWN_SET_PROTOCOL(p, proto) \
    ((p)->sio_calldown->down_set_protocol(p, proto))

/* Query mapped interface type */
#define DOWN_GET_MAPID(p, arg) \
    ((p)->sio_calldown->down_mapid(p, arg))

/* Perform mapping to user address space */
#define DOWN_MAP(p, vt, off) \
    ((p)->sio_calldown->down_map(p, vt, off))

/* Cleanup after mapped port is closed */
#define DOWN_UNMAP(p) \
    ((p)->sio_calldown->down_unmap(p))

/* Set/Reset ioc3 sscr register */
#define DOWN_SET_SSCR(p, arg, flag) \
    ((p)->sio_calldown->down_set_sscr(p, arg, flag))

/* protocol types for DOWN_SET_PROTOCOL */
enum sio_proto {
    PROTO_RS232,
    PROTO_RS422
};

/* The callup struct. This is a set of entry points providing
 * black-box access to the upper level kernel interface by the
 * hardware handling code. These entry points are used for event
 * notification 
 */
struct serial_callup {
    void (*up_data_ready)	(sioport_t *port);
    void (*up_output_lowat)	(sioport_t *port);
    void (*up_ncs)		(sioport_t *port, int ncs);
    void (*up_dDCD)		(sioport_t *port, int dcd);
    void (*up_dCTS)		(sioport_t *port, int cts);
    void (*up_detach)		(sioport_t *port);
};

/*
 * Macros used by the lower layer to access the upper layer for event
 * notificaiton. These functions are generally called in response to
 * an interrupt. Since the port lock may be released across UP calls,
 * we must check the callup vector each time. However since the port
 * lock is held during DOWN calls (from which these UP calls are made)
 * there is no danger of the sio_callup vector being cleared between
 * where it is checked and where it is used in the macro
 */

/* Notify the upper layer that there are input bytes available and
 * DOWN_READ may now be called
 */
#define UP_DATA_READY(p) \
    ((p)->sio_callup ? (p)->sio_callup->up_data_ready(p):(void)0)

/* Notify the upper layer that the lower layer has freed up some
 * output buffer space and DOWN_WRITE may now be called
 */
#define UP_OUTPUT_LOWAT(p) \
    ((p)->sio_callup ? (p)->sio_callup->up_output_lowat(p):(void)0)

/* Notify the upper layer that the next char returned by DOWN_READ
 * has the indicated special status. (see NCS_* above)
 */
#define UP_NCS(p, ncs) \
    ((p)->sio_callup ? (p)->sio_callup->up_ncs(p, ncs):(void)0)

/* Notify the upper layer of the new DCD input value */
#define UP_DDCD(p, dcd) \
    ((p)->sio_callup ? (p)->sio_callup->up_dDCD(p, dcd):(void)0)

/* Notify the upper layer of the new CTS input value */
#define UP_DCTS(p, cts) \
    ((p)->sio_callup ? (p)->sio_callup->up_dCTS(p, cts):(void)0)

/* notify the upper layer that the lower layer hardware has been detached 
 * Since the port lock is NOT held when this macro is executed, we must
 * guard against the sio_callup vector being cleared between when we check
 * it and when we make the upcall, so we use a local copy.
 */
#define UP_DETACH(p) \
{ \
    struct serial_callup *up; \
    if (up = (p)->sio_callup) \
	up->up_detach(p); \
}

/* Port locking protocol:
 * Any time a DOWN call is made into one of the lower layer entry points,
 * the corresponding port is already locked and remains locked throughout
 * that downcall. When a lower layer routine makes an UP call, the port
 * is assumed to be locked on entry to the upper layer routine, but the
 * upper layer routine may release and reacquire the lock if it wishes.
 * Thus the lower layer routine should not rely on the port lock being
 * held across upcalls. Further, since the port may be disconnected
 * any time the port lock is not held, an UP call may cause subsequent
 * UP calls to become noops since the upcall vector will be zeroed when
 * the port is closed. Thus, any lower layer routine making UP calls must
 * be prepared to deal with the possibility that any UP calls it makes
 * are noops.
 *
 * The only time a lower layer routine should manipulate the port lock
 * is the lower layer interrupt handler, which should acquire the lock
 * during its critical execution.
 * 
 * Any function which assumes that the port is or isn't locked should
 * use the function sio_port_islocked in an ASSERT statement to verify
 * this assumption
 */

#if DEBUG
extern int sio_port_islocked(sioport_t *);
#endif

#define SIO_TRY_LOCK_PORT(port)	sio_lock_port(port, 1)
#define SIO_LOCK_PORT(port)	(void)sio_lock_port(port, 0)
#define SIO_UNLOCK_PORT(port)	sio_unlock_port(port)

extern int sio_lock_port(sioport_t *, int);
extern void sio_unlock_port(sioport_t *);
extern int sio_sleep(sioport_t *, sv_t *, int);
#if DEBUG
extern void sio_debug_unlock_port(sioport_t *);
#endif

/* kernel debugger support */
#ifdef _LANGUAGE_C
extern int console_is_tport;
#define CNTRL_A		'\001'
#if DEBUG
#ifndef DEBUG_CHAR
#define DEBUG_CHAR	CNTRL_A
#endif
#else
#define DEBUG_CHAR	CNTRL_A
#endif
#endif

extern void sio_init_locks(struct sio_lock *);
extern void sio_initport(sioport_t *, vertex_hdl_t, int, int);
extern void sio_make_hwgraph_nodes(sioport_t *);
extern void sio_upgrade_lock(sioport_t *, int);
extern void sio_downgrade_unlock_lock(sioport_t *, int);


/* flags to notify sio_initport() which type of nodes are
 * desired for a particular hardware type
 */
#define NODE_TYPE_D		0x01 /* standard plain streams interface */
#define NODE_TYPE_MODEM		0x02 /* modem streams interface */
#define NODE_TYPE_FLOW_MODEM	0x04 /* modem/flow control streams */
#define NODE_TYPE_CHAR		0x08 /* character interface */
#define NODE_TYPE_MIDI		0x10 /* midi interface */
#define NODE_TYPE_D_RS422	0x20 /* RS422 without flow control */
#define NODE_TYPE_FLOW_RS422	0x40 /* RS422 with flow control */

#define NODE_TYPE_USER		0x80 /* user mapped interface */
#define NODE_TYPE_TIMESTAMPED	0x100 /* user mapped interface */

#define NODE_TYPE_ALL_RS232	(NODE_TYPE_D | NODE_TYPE_MODEM | \
				 NODE_TYPE_FLOW_MODEM | NODE_TYPE_CHAR | \
				 NODE_TYPE_MIDI | NODE_TYPE_TIMESTAMPED)
#define NODE_TYPE_ALL_RS422	(NODE_TYPE_D_RS422 | NODE_TYPE_FLOW_RS422 | \
				 NODE_TYPE_TIMESTAMPED)

/* Flags for devflags field of miditype structure */
#define MIDIDEV_EXTERNAL 0             /* lower half initializes devflags to this for an external device */
#define MIDIDEV_INTERNAL 0x2

#define MIDIDEV_UNREGISTERED -1    /* Initialization for portidx field of miditype structure */

typedef struct miditype_s{
  int devflags;                    /* DEV_EXTERNAL, DEV_INTERNAL */
  int portidx;  
  void *midi_upper;                      
  sioport_t *port;
} miditype_t;

typedef struct tsiotype_s{
  void *tsio_upper;                      
  sioport_t *port;
  int portidx;
  int urbidx;
} tsiotype_t;

#endif /* __SERIALIO_H__ */
