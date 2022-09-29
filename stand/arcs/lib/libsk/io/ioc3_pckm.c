/*  PC keyboard driver for Godzilla (SpeedRacer, SN0)
 *  This only supports one keyboard/mouse set.
 */

#ident	"$Revision: 1.116 $"

#include <sys/types.h>
#include <sys/cpu.h>

#ifdef SN0
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <promgraph.h>
#include <sys/graph.h>
#include <sys/hwgraph.h>
#include <arcs/cfgdata.h>
#include <pgdrv.h>
#endif
#include <saio.h>
#include <saioctl.h>
#include <sys/sbd.h>
#include <arcs/folder.h>
#include <arcs/hinv.h>
#include <arcs/errno.h> 
#include <libsc.h>
#include <libsk.h>
#include <tty.h>

/* DEBUGGING STUFF ... */
#define PCKM_WAR 1		/* Turn off when P2 boards show up */
/* #define PCKM_DEBUG 1 */	/* Turn on extra debugging code */
/* #define DUMP_ROUTINE 1 */	/* Turn on dump_routines */

#define IOC3_PCKM 1
#include <sys/PCI/PCI_defs.h>
#include <sys/pckm.h>

#if SN0
static __psunsigned_t ioc3_base;
#define IOC3_MEM_BASE           ioc3_base
#define KERNEL_ADDRESS
#else
#define IOC3_MEM_BASE           IOC3_PCI_DEVIO_BASE
#define KERNEL_ADDRESS		PHYS_TO_K1
#endif

#define PORT0_READ		KERNEL_ADDRESS(IOC3_MEM_BASE+IOC3_K_RD)
#define PORT1_READ		KERNEL_ADDRESS(IOC3_MEM_BASE+IOC3_M_RD)
#define PORT0_WRITE		KERNEL_ADDRESS(IOC3_MEM_BASE+IOC3_K_WD)
#define PORT1_WRITE 		KERNEL_ADDRESS(IOC3_MEM_BASE+IOC3_M_WD)
#define KBMS_STATUS       	KERNEL_ADDRESS(IOC3_MEM_BASE+IOC3_KM_CSR)

#define RETRY	 		25	/* was 100 */
#define WAIT_TIME		1000	/* was 2500 */
#define IOC3_DELAY		us_delay(1800)

#define PCI_READ(x,y) 		y=*(volatile __uint32_t *)(x)
#define PCI_WRITE(x,y)		*(volatile __uint32_t *)(x) = y

/* 
 * Ports on the IOC3 ..
 */
#define PORT0		0
#define PORT1		1

/*
 * _kbd_strat sets the device values ...
 */
#define KEYBOARD_DEVICE	0
#define MOUSE_DEVICE	1

/*
 * These are the defined on of following ways :
 * 	ENODEV - no device
 *	PORT0/PORT1 (see above)
 * This is for the keyboard/mouse devices to 
 * plugged into any PORT.
 */
static int keyboard_port;
static int mouse_port;
static int valid_kbd_data;
static int valid_mouse_data;

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
#define PORT_WRITE(X)	\
	(X == PORT0)?PORT0_WRITE:				\
	(X == PORT1)?PORT1_WRITE:NULL

#define PORT_READ(X)	\
	(X == PORT0)?PORT0_READ:				\
	(X == PORT1)?PORT1_READ:NULL

static struct pckm_data_info pckm_data_bits[] = {
	KM_RD_VALID_0, KM_RD_DATA_0, KM_RD_DATA_0_SHIFT, KM_RD_FRAME_ERR_0,
	KM_RD_VALID_1, KM_RD_DATA_1, KM_RD_DATA_1_SHIFT, KM_RD_FRAME_ERR_1,
	KM_RD_VALID_2, KM_RD_DATA_2, KM_RD_DATA_2_SHIFT, KM_RD_FRAME_ERR_2,
	0, 0, 0, 0,
};

struct keyinfo {
	struct device_buf buf;
	volatile unsigned char *pcntrl;
	volatile unsigned char *pcdata;
};

struct keyinfo port;
static int keystate;
static int led_bits;
static int kbd_exists;
static int pcms_exists;
static int pckm_flags;
#define flags pckm_flags

static void reinit_kbd(void), reinit_pcms(void);

/* flags */
#define TRANSLATE	0x01		/* translate key data */

#define pressed(X)	((keystate & (X)) != 0)
#define getkey(data)	(&keys[sc2kti[(unsigned char)((data)&0xff)]])

extern struct key_type keys[];
extern char *keys_ext[];
short sc2kti[142];

static int _pcmsinit(IOBLOCK *);
static int _kbdinit(IOBLOCK *);
static int _kbdopen(IOBLOCK *, int);
static int _kbdread(IOBLOCK *, int);
static int _kbdioctl(IOBLOCK *, int);
static int kbdgetc(int, char *, struct device_buf *);
static int kbd_translate(int, char *);
static int reset_kbd(void);
static int reset_pcms(void);
static void default_kbd_setting(void);
static void default_ms_setting(void);

static void pckm_setleds(int);
static void pckm_sendok(int);
void pckm_flushinp(int);
void pckm_setupmap(void);

static int check_port(int);
static void set_ports(void);
static int data_check(uint data, int cmd);

#if IP30
static int kbd_functionkey(char *, int, int);
static char pckbd_installed;
#endif

#ifdef PCKM_DEBUG
#define PROBEDEBUG(arg)		printf arg
#else
#define PROBEDEBUG(arg)
#endif

#ifdef DUMP_ROUTINE
static void print_data(uint);
static void print_km_csr(uint);
#endif

static int
_pcmsinit(IOBLOCK *iob)
{
	/* mouse is probed in pckbd_install() */
	if (!pcms_exists) {
		iob->ErrorNumber = ENODEV;
		return(-1);
	}

	ms_install();

	pckm_flushinp(mouse_port);
 
	return (0);
}

static int
_kbdinit(IOBLOCK *iob)
{
	/* keyboard is probed in pckbd_install() */
	if (!kbd_exists) {
		iob->ErrorNumber = ENODEV;
		return(-1);
	}

	pckm_setleds(led_bits = 0);
	keystate = 0;

	pckm_flushinp(keyboard_port);

	return (0);
}

static int
_kbdopen(IOBLOCK *iob, int device)
{
	if (device == KEYBOARD_DEVICE) {
		(void)pckm_pollcmd(keyboard_port,CMD_ENABLE);
		flags = (iob->Flags & F_ECONS) ? 0 : TRANSLATE;
		CIRC_FLUSH(&port.buf);
	} else {
		(void)pckm_pollcmd(mouse_port,CMD_ENABLE);
	}

	iob->Flags |= F_SCAN;

	return(0);
}

static int
_kbdread(IOBLOCK *iob, int device)
{
	register struct device_buf *db;
	char kbdbuf[48], *p = (char *)iob->Address;
	int cnt = iob->Count;

	if (device == MOUSE_DEVICE) 
		return(iob->Count);

	db = &port.buf; 

	while (iob->Count > 0) {
		kbdgetc(device, kbdbuf, db);
		if ((iob->Flags & F_NBLOCK) == 0) {
			while (CIRC_EMPTY(db))
				_scandevs();
		}
		if (CIRC_EMPTY(db))
			return (cnt - iob->Count);

		*p++ = _circ_getc(db);
		iob->Count--;
	}
	iob->Count = cnt;
	return(ESUCCESS);
}


static void
_kbdpoll(IOBLOCK *iob, int device)
{
	register struct device_buf *db = &port.buf; 
	char kbdbuf[48];

	kbdgetc(device, kbdbuf, db);

	/* _kbdpoll(0) is used to clear input from inside driver */
	if (iob) iob->ErrorNumber = ESUCCESS;
}

static int
_kbdioctl(IOBLOCK *iob, int device)
{
	register struct device_buf  *db = &port.buf; 

	if (device == MOUSE_DEVICE)
		return(iob->ErrorNumber = EINVAL);

	switch ((__psint_t)iob->IoctlCmd) {
	case TIOCFLUSH:
		CIRC_FLUSH(db);
		break;
	case TIOCCHECKSTOP:
		while (CIRC_STOPPED(db))
			_scandevs();
		break;

	default:
		return(iob->ErrorNumber = EINVAL);
	}

	return(ESUCCESS);
}

static int
_kbdreadstat(IOBLOCK *iob, int device)
{
	register struct device_buf *db = &port.buf; 

	if (device == MOUSE_DEVICE)
		return(iob->ErrorNumber = EAGAIN);

	iob->Count = _circ_nread(db);
	return(iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN));
}

int
_kbd_strat(COMPONENT *self, IOBLOCK *iob)
{
	int device = self->Type == PointerPeripheral;

	switch (iob->FunctionCode) {
	case FC_INITIALIZE:		/* value 0 */
		return(device?_pcmsinit(iob):_kbdinit(iob));

	case FC_OPEN:			/* value 1 */
		return(_kbdopen(iob,device));

	case FC_READ:			/* value 3 */
		return(_kbdread(iob,device)); 

	case FC_CLOSE:			/* value 2 */
		return(0);

	case FC_IOCTL:
		return(_kbdioctl(iob,device));

	case FC_GETREADSTATUS:
		return(_kbdreadstat(iob,device));

	case FC_POLL:			/* value 7 */
		_kbdpoll(iob,device);

	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

static int
kbdgetc(int device, char *kbdbuf, struct device_buf *db)
{
	struct pckm_data_info *p;
	uint data, local_data, status_reg;
	int rc;
	int i, c, cc;

	/* 
	 * Check for hot plug or real device character.
	 */
	rc = 0;
	switch (device) {
	case KEYBOARD_DEVICE :
		PCI_READ(PORT_READ(keyboard_port),data);
		for (p = pckm_data_bits; p->valid_bit; p++) {
			if (PCKM_DATA_VALID(data)) {
				if ((PCKM_DATA_SHIFT(data) == KBD_BATOK) || 
#ifdef PCKM_WAR
				    (PCKM_DATA_SHIFT(data) == CMD_MSENABLE) ||  /* 0xa8 */
#endif
				    (PCKM_DATA_SHIFT(data) == KBD_BATFAIL) || 
				    (!valid_kbd_data)) {
					valid_kbd_data = 1;
					local_data = pckm_pollcmd(keyboard_port, CMD_SELSCAN);
					if (data_check(local_data,KBD_ACK)) {
						local_data = pckm_pollcmd(keyboard_port, 
							MOUSE_ID);
					}
					if (data_check(local_data,KBD_ACK)) {
						PCI_READ(PORT_READ(keyboard_port),local_data);
					}
					if (data_check(local_data, 0x2)) {
						reinit_kbd();
						rc = 0; /* return code */
						break;
					}
					default_kbd_setting();
					(void)pckm_pollcmd(keyboard_port,CMD_ENABLE);
				}
				if (flags & TRANSLATE) {
					cc = kbd_translate(PCKM_DATA_SHIFT(data), kbdbuf);
#if IP30
					cc = kbd_functionkey(kbdbuf, cc, sizeof(kbdbuf));
#endif
					for (i = 0; i < cc; i++)
						_ttyinput(db, kbdbuf[i]);
				} else {
					_circ_putc(c,db);
				}
			} else if (PCKM_FRAME_ERR(data)) {
				(void)pckm_pollcmd(keyboard_port,CMD_DISABLE);
				default_kbd_setting();
				(void)pckm_pollcmd(keyboard_port,CMD_ENABLE);
				rc = 0; /* return code */
			}
		}
		break;
	case MOUSE_DEVICE :
		PCI_READ(PORT_READ(mouse_port),data);
		for (p = pckm_data_bits; p->valid_bit; p++) {
			if (PCKM_DATA_VALID(data)) {
#ifdef PCKM_DEBUG
				if (KM_RD_OFLO & PCKM_DATA_VALID(data))
					PROBEDEBUG(("Mouse valid OFLOW\n"));
#endif
				/*
				 * We think the port now has a device in it ...
				 * We only really want to do this once and
				 * real data can look like MOUSE_ID (0x0).
			  	 * So we check if we have seen valid data
				 * before or if the mouse has been re-inited.
				 */
				if (PCKM_DATA_SHIFT(data) == MOUSE_ID && !valid_mouse_data) {
					reinit_pcms();
					valid_mouse_data = 1;
				}

				if (PCKM_DATA_SHIFT(data) != KBD_BATOK) {
					ms_input(PCKM_DATA_SHIFT(data));
				} else {
					valid_mouse_data = 1;
					local_data = pckm_pollcmd(mouse_port, CMD_MSSTAT);
					if (data_check(local_data,KBD_ACK)) {
						PCI_READ(PORT_READ(mouse_port),local_data);
						if (!data_check(local_data, MS_SAMPL_40SEC)) {
							default_ms_setting();
							(void)pckm_pollcmd(mouse_port,CMD_ENABLE);
						} else {
							ms_input(PCKM_DATA_SHIFT(data));
						}
					} else {
						ms_input(PCKM_DATA_SHIFT(data));
						default_ms_setting();
						(void)pckm_pollcmd(mouse_port,CMD_ENABLE);
					}
				}
#ifdef PCKM_DEBUG
			} else {
				if (KM_RD_OFLO & PCKM_DATA_VALID(data))
					PROBEDEBUG(("Mouse OFLOW\n"));
				if (PCKM_GOT_DATA(data) && PCKM_FRAME_ERR(data))
					PROBEDEBUG(("Mouse Frame error\n"));
#endif
			}
		}
		break;
	default :
		rc = -1;
		break;
	}

	return(rc);
}

static int
kbd_translate(register int scan_data, register char *kbd_buf)
{
	static int last_scan;
	struct key_type *k;
	char *rstr = 0;
	char tmp[2];
	int i;
	
	if (scan_data == KBD_BREAK) {
		last_scan = scan_data;
		return(0);
	}

	if (scan_data > 141)		/* only this many scan codes */
		return(0);

	k = getkey(scan_data);

	if (k->k_flags & KEY_CHLED) {
		/* Toggle keys suchs as caps lock ignore key-up */
		if (last_scan != KBD_BREAK) {
			led_bits ^= k->k_svalue[0];
			keystate ^= k->k_nvalue[0];
			pckm_setleds(led_bits); 
		}
	}
	else if (k->k_flags & KEY_CHSTATE) {
		/* key down -> state on, key up -> state off */
		if (last_scan == KBD_BREAK)
			keystate &= ~k->k_nvalue[0];
		else
			keystate |= k->k_nvalue[0];
	}
	else if (last_scan == KBD_BREAK)
		;
	else if (pressed(S_ALLALT))
		rstr = k->k_avalue;
	else if (pressed(S_ALLCONTROL)) {
		rstr = k->k_cvalue;
	}
	else {
		rstr = k->k_nvalue;			/* default */
		tmp[1] = '\0';

		/* certain keys have been placed into a second table */
		/* those in this table are strings greater than 1 char */
		/* ie f1-f12, up, down, left, right arrow keys */
		if (rstr[0] == KBD_EXT_TABLE)
			rstr = keys_ext[rstr[1]];

		if (k->k_flags & KEY_NUMLOCK) {
			/*  If shift + numlock then we are normal, or
			 * double shifted.  Otherwise we are shifted
			 * and get the numpad value.
			 */
			if (pressed(S_NUMLOCK) != pressed(S_ALLSHIFT))
				rstr = k->k_svalue;
		}
		else if (pressed(S_CAPSLOCK)) {
			if (k->k_flags & KEY_CAPLOCK)
				rstr = k->k_svalue;
			else if (pressed(S_ALLSHIFT) == 0) {
				if (k->k_flags & KEY_CAPNORM) {
					tmp[0] = (*k->k_nvalue) - 040;
					rstr = tmp;
				}
			}
			else {
				if (k->k_flags & KEY_CAPSHFT) {
					tmp[0] = (*k->k_svalue) - 040;
					rstr = tmp;
				}
				else
					rstr = k->k_svalue;
			}
		}
		else if (pressed(S_ALLSHIFT))
			rstr = k->k_svalue;
        }

	/* Fill kbd_buf if we have any data.
	 */
	i = 0;
	if (rstr) while(*kbd_buf++ = *rstr++) i++;

	last_scan = scan_data;

	return(i);
}

#if IP30
/*
 * Very simplistic routine to allow aliasing function keys
 * to data strings via environment variables.
 * For example, use:
 *
 *       setenv F001 bootp()ide.IP24
 *
 * to assign that command to the F1 key.  Function key numbers
 * must have 3 digits with leading zeroes.  (Note: use "-p"
 * option to setenv to retain settings between machine resets.)
 */

static int
kbd_functionkey(char *kbdbuf, int datalen, int maxlen)
{
	char *p = kbdbuf, *q;
	char funckey[5];

	if (datalen == 6)
	    if (p[0] == '\033')
		if (p[1] == '[')
		    if (p[5] == 'q') {
			/* we've got a function key */
			funckey[0] = 'F';
			funckey[1] = p[2];
			funckey[2] = p[3];
			funckey[3] = p[4];
			funckey[4] = '\0';
			if (q = getenv(funckey))
			    if (strlen(q) <= maxlen) {
				datalen = strlen(q);
				p = kbdbuf;
				while (*p++ = *q++)
				    ;
			    }
		    }

	return datalen;
}
#endif

static int
reset_pcms(void)
{
	struct pckm_data_info *p;
	int ack = 0;
        uint data;

	if (mouse_port == ENODEV) {
		PROBEDEBUG(("No mouse device\n"));
		return(ENODEV);
	}

	/* default sample rate is 100 -- lower to 40 */
	default_ms_setting();

	return(ESUCCESS);
}

static int
reset_kbd(void) 
{
	extern int _prom;

	if (keyboard_port == ENODEV) {
		PROBEDEBUG(("No keyboard device\n"));
		return(ENODEV);
	}
	
	if (_prom)		/* ring bell to acknowledge keyboard init */
		bell();

	default_kbd_setting();

	return(ESUCCESS);
}


static void
reinit_pcms(void)
{
	/*REFERENCED*/
	int notused;
	int i, device_id;

	PROBEDEBUG(("pckm: reinit mouse.\n"));

	device_id = check_port(mouse_port);
	if (device_id != MOUSE_ID) {
		set_ports();
		device_id = check_port(keyboard_port);
		if (device_id == KEYBD_ID || device_id == KEYBD_BEEP_ID2)	
			reinit_kbd();

		device_id = check_port(mouse_port);
	}

	if (device_id == MOUSE_ID) {
		default_ms_setting();
		pckm_flushinp(mouse_port);

		(void)pckm_pollcmd(mouse_port,CMD_ENABLE);

		PCI_READ(PORT_READ(mouse_port),notused);
	}
}

static void
reinit_kbd(void)
{
	int device_id;

	PROBEDEBUG(("pckm: reinit keyboard.\n"));

	device_id = check_port(keyboard_port);
	if (device_id != KEYBD_ID && device_id != KEYBD_BEEP_ID2) {
		/* Holy cow ... it's not a keyboard.  Is it a mouse? */
		set_ports();
		if (check_port(mouse_port) == MOUSE_ID)	
			reinit_pcms();

		device_id = check_port(keyboard_port);
	}

	if (device_id == KEYBD_ID || device_id == KEYBD_BEEP_ID2) {
		default_kbd_setting();
		(void)pckm_pollcmd(keyboard_port,CMD_ENABLE);
	}
}

static void
default_kbd_setting(void)
{
	pckm_flushinp(keyboard_port);

	(void)pckm_pollcmd(keyboard_port,CMD_SELSCAN);
	(void)pckm_pollcmd(keyboard_port,3);

	(void)pckm_pollcmd(keyboard_port,CMD_ALLMBTYPE);

	(void)pckm_pollcmd(keyboard_port,CMD_KEYMB);
	(void)pckm_pollcmd(keyboard_port,SCN_CAPSLOCK);

	(void)pckm_pollcmd(keyboard_port,CMD_KEYMB);
	(void)pckm_pollcmd(keyboard_port,SCN_NUMLOCK);

	if (led_bits != 0)
		pckm_setleds(led_bits);
}

/* 
 * NOTE : MS_SAMPL_40SEC is not the power up default of the mouse 
 * this is different that the kernel ...
 */
static void
default_ms_setting(void)
{
	pckm_flushinp(mouse_port);

	(void)pckm_pollcmd(mouse_port,CMD_MSSAMPL);
	(void)pckm_pollcmd(mouse_port,MS_SAMPL_40SEC);
}

/* Can be used for beep and leds lighting */

static void
pckm_setleds(int bits)
{
	struct pckm_data_info *p;
	int ack = 0;
	uint data;
	uint status;

	data = pckm_pollcmd(keyboard_port,CMD_SETLEDS);

	for (p = pckm_data_bits; p->valid_bit && !ack; p++) {
		if (PCKM_DATA_VALID(data)) {
			if (PCKM_DATA_SHIFT(data) == KBD_ACK)
				ack++;
		}
	}

	if (!ack) {
		default_kbd_setting();
		(void)pckm_pollcmd(keyboard_port,CMD_ENABLE);
		return;
	}

	(void) pckm_pollcmd(keyboard_port,(bits&0x1F));
}

static void
pckm_sendok(int port_no)
{
	uint status_data, retry = RETRY;
	uint write_bit;

	PCI_READ(KBMS_STATUS,status_data);

	if (port_no == PORT0)
		write_bit = KM_CSR_K_WRT_PEND;
	else 
		write_bit = KM_CSR_M_WRT_PEND;

	while ((status_data & write_bit) && --retry) {
		PCI_READ(KBMS_STATUS,status_data);
		us_delay(WAIT_TIME);
	}
}

uint
pckm_pollcmd(int port_no, int cmd)
{
	uint read_data;
	int retry;
	
	retry = RETRY;
	pckm_sendok(port_no);
	PCI_WRITE(PORT_WRITE(port_no),cmd);
	do {
		us_delay(WAIT_TIME);
		PCI_READ(PORT_READ(port_no),read_data);
	} while (((read_data & KM_RD_VALID_ALL) == 0) && --retry);

	return(read_data);
}

void
pckm_flushinp(int port_no)
{
	/* REFERENCED */
	uint notused;

	PCI_READ(PORT_READ(port_no),notused);
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


static COMPONENT kbdctrltmpl = {
	ControllerClass,		/* Class */
	KeyboardController,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	7,				/* IdentifierLength */
	"pckbd",			/* Identifier */
};
static COMPONENT kbdtempl = {
	PeripheralClass,		/* Class */
	KeyboardPeripheral,		/* Type */
	ReadOnly|ConsoleIn|Input,	/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	0				/* Identifier */
};

static COMPONENT pcmsctrltmpl = {
	ControllerClass,		/* Class */
	PointerController,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	4,				/* IdentifierLength */
	"pcms",				/* Identifier */
};
static COMPONENT pcms = {
        PeripheralClass,                /* Class */
        PointerPeripheral,              /* Peripheral */
        ReadOnly|Input,                 /* Flags */
        SGI_ARCS_VERS,                  /* Version */
        SGI_ARCS_REV,                   /* Revision */
        0,                              /* Key */
        0x01,                           /* Affinity */
        0,                              /* ConfigurationDataSize */
        0,                              /* IdentifierLength */
        0				/* Identifier */
};

#ifdef SN0
#ifdef SN_PDI
int
pckbd_install(pd_inst_hdl_t *pdih, pd_inst_parm_t *pdip)
#else
int
pckbd_install(COMPONENT *parent)
#endif
#else
int
pckbd_install(COMPONENT *parent)
#endif
{
	COMPONENT *ctr;
        uint pci_command_default = 0;
        uint status_reg;

#if IP30
	if (pckbd_installed)
	    return(1);
	pckbd_installed = 1;
#endif
#ifdef SN0
#ifdef SN_PDI
	ioc3_base = GET_PCIBASE_FROM_KEY(pdipDmaParm(pdip)) ;
#else
	ioc3_base = GET_PCIBASE_FROM_KEY(parent->Key);
#endif
	PROBEDEBUG(("pckm: ioc3_base=%llx; status=%llx\n",ioc3_base, KBMS_STATUS));
#endif
	/* Zero this out ... per new doc IOC3 4.5 */
	status_reg = KM_CSR_K_CLAMP_THREE | KM_CSR_M_CLAMP_THREE;
	PCI_WRITE(KBMS_STATUS, status_reg);

	/*
	 * Determine which PORT's have a device and 
 	 * if it does have a device ... what type of
	 * device it is.
	 */
	set_ports();

#ifdef SN0
#ifdef SN_PDI
	if (pdipDevStatus(pdip) = (keyboard_port != ENODEV))
		snAddChild(pdih, pdip) ;
	if (pdipDevStatus(pdip) = ((mouse_port != ENODEV) << 1))
		snAddChild(pdih, pdip) ;
#else
	PUT_INSTALL_STATUS(parent, 
		(keyboard_port != ENODEV) | ((mouse_port != ENODEV) << 1)) ;
#endif
#endif

	if (keyboard_port != ENODEV) {
#ifndef SN0
		ctr = AddChild(parent,&kbdctrltmpl,(void *)NULL);
		if (ctr == (COMPONENT *)NULL) cpu_acpanic("pckbd ctrl");
#endif

		/*
	 	* Check for keyboard or reset keyboard
	 	*/
		if (kbd_exists || !reset_kbd()) {
#ifndef SN0
			ctr = AddChild(ctr,&kbdtempl,(void *)NULL);
			if (ctr == (COMPONENT *)NULL) 
				cpu_acpanic("pckbd");
			RegisterDriverStrategy(ctr,_kbd_strat);
#endif
			pckm_setupmap();
			kbd_exists = 1;
			PROBEDEBUG(("pckbd_install : kbd_exists\n"));
		}
	}

	if (mouse_port != ENODEV) {
#ifndef SN0
		ctr = AddChild(parent,&pcmsctrltmpl,(void *)NULL);
		if (ctr == (COMPONENT *)NULL) cpu_acpanic("pcms ctrl");
#endif
		/*
	 	 * Check for mouse or reset mouse
	 	 */
		if (pcms_exists || !reset_pcms()) {
#ifndef SN0
			ctr = AddChild(ctr,&pcms,(void *)NULL);
			RegisterDriverStrategy(ctr,_kbd_strat);
#endif
			pcms_exists = 1;
			PROBEDEBUG(("pckbd_install : pcms_exists\n"));
		}
	}

	return(-1);
}

extern short delta_german[],delta_french[],delta_itailian[],
	delta_danish[],delta_spanish[],delta_swiss_german[],delta_swedish[],
	delta_united_kingdom[],delta_norwegian[], delta_portuguese[],
	delta_swiss_french[], delta_belgian[], delta_japanese[];

static struct kbd_type {
	char *name;
	short *delta;
} kbd_types[] = {
	{ "US",		0 },
	{ "DE",		delta_german },
	{ "FR",		delta_french },
	{ "IT",		delta_itailian },
	{ "DK",		delta_danish },
	{ "ES",		delta_spanish },
	{ "de_CH",	delta_swiss_german },
	{ "SE",		delta_swedish },
	{ "FI",		delta_swedish },
	{ "GB",		delta_united_kingdom },
	{ "BE",		delta_belgian },
	{ "NO",		delta_norwegian },
	{ "PT",		delta_portuguese },
	{ "JP",		delta_japanese },
	{ "fr_CH",	delta_swiss_french },
	{ "D",		delta_swiss_german },
	{ "d",		delta_swiss_german },
	{ "F",		delta_swiss_french },
	{ "f",		delta_swiss_french },
	{ 0,0 }
};

void
pckm_setupmap(void)
{
	char *keybd = getenv("keybd");
	extern short base_sc2kti[];
	int i,j;

	bcopy(base_sc2kti,sc2kti,sizeof(sc2kti));	/* base USA map */

	if (!keybd) return;				/* default is USA */

	for (i=0; kbd_types[i].name; i++)
		if (!strcmp(kbd_types[i].name,keybd)) {
			if (kbd_types[i].delta) {
				j = kbd_types[i].delta[0];
				while (j <= kbd_types[i].delta[1]) {
					sc2kti[keys[j].k_scan] = j;
					j++;
				}
			}
			break;
		}

	/* if not found use USA map, but X will try to load named map. */

	return;
}

#ifdef DUMP_ROUTINE
static void
print_data(uint data)
{
	if (data == 0) {
		printf("\t*** No data ***\n");
		return;
	}

	printf("\t** Data type %s => %#x\n",
		(KM_RD_KBD_MSE & data)?"Mouse":"Keyboard",data);
	
	if (KM_RD_VALID_2 & data) {
		printf("\tData_2 : %#x\n",(KM_RD_DATA_2 & data));
		if (KM_RD_FRAME_ERR_2 & data)
			printf("\t\tFrame error_2 : %s\n","ERROR");
	} else {
		printf("\tData_2 : %s\n","INVALID");
	}

	if (KM_RD_VALID_1 & data) {
		printf("\tData_1 : %#x\n",
			(KM_RD_DATA_1 & data) >> KM_RD_DATA_1_SHIFT);
		if (KM_RD_FRAME_ERR_1 & data)
			printf("\t\tFrame error_1 %s\n","ERROR");
	} else {
		printf("\tData_1 : %s\n","INVALID");
	}

	if (KM_RD_VALID_0 & data) {
		printf("\tData_0 : %#x\n",
			(KM_RD_DATA_0 & data) >> KM_RD_DATA_0_SHIFT);
		if (KM_RD_FRAME_ERR_0 & data)
			printf("\t\tFrame error_0 %s\n","ERROR");
	} else {
		printf("\tValid_0 : %s\n","INVALID");
	}

	if (KM_RD_OFLO & data)
		printf("\tOverflow %s\n","OVERFLOW");
}

static void
print_km_csr(uint data)
{
	if (KM_CSR_K_WRT_PEND & data) 
		printf("Keyboard write pending\n");
	if (KM_CSR_M_WRT_PEND & data) 
		printf("Mouse write pending\n");
	if (KM_CSR_K_LCB & data) 
		printf("Keyboard Line control bit\n");
	if (KM_CSR_M_LCB & data) 
		printf("Mouse Line control bit\n");
	if (KM_CSR_K_DATA & data) 
		printf("Keyboard data line high\n");
	if (KM_CSR_K_CLK & data) 
		printf("Keyboard clock line high\n");
	if (KM_CSR_K_PULL_DATA & data) 
		printf("Keyboard data line low\n");
	if (KM_CSR_K_PULL_CLK & data) 
		printf("Keyboard clock line low\n");
	if (KM_CSR_M_DATA & data) 
		printf("Mouse data line high\n");
	if (KM_CSR_M_CLK & data) 
		printf("Mouse clock line high\n");
	if (KM_CSR_M_PULL_DATA & data) 
		printf("Mouse data line low\n");
	if (KM_CSR_M_PULL_CLK & data) 
		printf("Mouse clock line low\n");
	if (KM_CSR_K_SM_IDLE & data) 
		printf("Keyboard idle\n");
	if (KM_CSR_M_SM_IDLE & data) 
		printf("Mouse idle\n");
}
#endif /* DUMP_ROUTINE */

/*
 * This function will set the variables keyboard_port and
 * mouse_port depending on the which port the device is
 * plugged into ...
 */
static void
set_ports(void)
{
	int port_no, keyboard, mouse;
	char *nogfxkbd;

	keyboard = mouse = -1;
	valid_kbd_data = valid_mouse_data = 0;
	keyboard_port = mouse_port = ENODEV;
	for (port_no = PORT0; port_no <= PORT1; port_no++) {
		switch(check_port(port_no)) {
			case KEYBD_ID :
			case KEYBD_BEEP_ID2 :
				keyboard_port = port_no;
				keyboard++;
				valid_kbd_data = 1;
				break;
			case MOUSE_ID :
				mouse_port = port_no;
				mouse++;
				valid_mouse_data = 1;
				break;
		}
	}

	if (keyboard > 1 ||  mouse > 1)
		PROBEDEBUG(("Cannot have two of the SAME devices in IOC3\n"));

	if (keyboard_port == ENODEV)
		valid_kbd_data = 0;

	/*
	 * I assume since there is no mouse port and maybe no keyboard port
	 * that this user will plug in the keyboard into port0.
	 * If nogfxkbd is not set and there is no keyboard then,
	 * keyboard_port will be set to ENODEV.
	 */ 
	if ((nogfxkbd=getenv("nogfxkbd")) && *nogfxkbd == '1') {
		if (keyboard_port == ENODEV) {
			if (mouse_port == PORT0)
				keyboard_port = PORT1;
			else 
				keyboard_port = PORT0;  
		}
	}

	/*
	 * If the mouse port is set to ENODEV (ie. no device).
	 * Find out which on the keyboard is plugged into or
	 * take the default port for the mouse (port1).
	 */
	if (mouse_port == ENODEV) {
		valid_mouse_data = 0;
		if (keyboard_port == PORT1)
			mouse_port = PORT0;
		else
			mouse_port = PORT1; 
	}
}

/*
 * Given the port_no (0,1) ... find out what type of
 * device (if any) is plugged into the port.
 */
int
check_port(int port_no)
{
	struct pckm_data_info *p;
	int ack, id_resp, device_id, beep_id, retry;
	uint data;

	beep_id = ack = id_resp = retry = 0;
	device_id = ENODEV;
	
	pckm_flushinp(port_no);
	data = pckm_pollcmd(port_no,CMD_ID);

	while ((device_id == ENODEV) && (retry < 3)) {
		retry++;
		for (p = pckm_data_bits; p->valid_bit; p++) {
			if (PCKM_DATA_VALID(data)) {
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
				PCI_READ(PORT_READ(port_no),data);
			} else {
				pckm_flushinp(port_no);
				data = pckm_pollcmd(port_no, CMD_ID);
			}
		}

	} /* End of while */

	return(device_id);
}
