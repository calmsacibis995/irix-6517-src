/*  PC keyboard driver for Moosehead.
 */

#if IP32

#ident	"$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/cpu.h>

#include <saio.h>
#include <saioctl.h>
#include <sys/sbd.h>
#include <arcs/folder.h>
#include <arcs/hinv.h>
#include <arcs/errno.h> 
#include <libsc.h>
#include <libsk.h>
#include <tty.h>
#include <sys/mace.h>
#include <sys/pckm.h>

#if 0
/* mace interprets the line control bit as start bit of
 * next character. This causes the actual start bit to 
 * be interpreted as the first data bit and msb to be 
 * dropped. If the last data bit (msb) was 1, that also
 * cause parity bit to be set in the status byte.
 */
#define MHPCKM_WAR

#else
/* Here, we define a variable which is equivalent to the MHPCKM_WAR
 * macro.  This allows us to do runtime checking of the MACE revision
 * in order to do the right thing for the keyboard.
 */
#define MHPCKM_WAR_ADDR	((volatile uint64_t *) 0xbf310000)

#define MACE_ID_REV1	0x00000000
#define MACE_ID_REV2	0x00000010
#define MACE_ID_MASK	0x00000010

int mhpckm_war = -1;
#endif

#define KEYBOARD_PORT	0
#define MOUSE_PORT		1
#define CTRL_PORT		2

#define PERIPHERAL_STATUS_REGISTER      PHYS_TO_K1(ISA_INT_STS_REG)
#define PERIPHERAL_INTMSK_REGISTER      PHYS_TO_K1(ISA_INT_MSK_REG)

#define offset(x)              ((int) &((struct ps2if *) 0)->x)
#define WRITE_ISA(reg, val) WRITE_REG64(val, (ulong)km->ifp + offset(reg), long long)
#define READ_ISA(reg)       READ_REG64((ulong)km->ifp + offset(reg), long long)

/* undefine some definition from sys/pckm.h */
#undef 	inp
#undef	outp

/* I force the read from a input port to be a byte. Note that the
 * receive buffer register acutally returns status also along with
 * the data. For now I read the status separately from mace. Thus
 * it is safe for "inp" to return a byte here. If this becomes a
 * problem in the future, "inp" can be modified to return the whole
 * 64 bits and the caller will fix it according to its needs.
 */
#define	inp(port, reg) \
	((port == KEYBOARD_PORT) ? \
		READ_REG64(KEYBOARD_PORT_ADDR + offset(reg), long long) & 0xff: \
		READ_REG64(MOUSE_PORT_ADDR + offset(reg), long long) & 0xff)
#define	outp(port, reg, val) \
	((port == KEYBOARD_PORT) ? \
		WRITE_REG64(val, KEYBOARD_PORT_ADDR + offset(reg), long long): \
		WRITE_REG64(val, MOUSE_PORT_ADDR + offset(reg), long long))


/* keyboard/mouse buffer full
 *
 * Earlier definitions for 8048:
 * #define K_OBF  (SR_OBF) 
 * #define M_OBF  (SR_OBF|SR_MSFULL)
 * #define M_MASK (SR_OBF|SR_MSFULL)
 */

#define K_OBF           (PS2_SR_RBF)
#define M_OBF           (PS2_SR_RBF)
#define M_MASK          (PS2_SR_RBF)

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
int kbdgetc(int);
static int kbd_translate(int, char *);
static int kbd_functionkey(char *, int, int);
static int reset_kbd(void);
static int reset_pcms(void);

static void pckm_setleds(int);
static void pckm_flushinp(int);
static void pckm_sendok(int);
void pckm_setupmap(void);

uint pckm_pollcmd(int, int);

#ifdef PCDEBUG
#define PROBEDEBUG
#define ttyprintf	printf
#endif

static int
_pcmsinit(IOBLOCK *iob)
{
	volatile uint64_t readVal;

	/* mouse is probed in pckbd_install() */
	if (!pcms_exists) {
		iob->ErrorNumber = ENODEV;
		return(-1);
	}

	ms_install();
	reinit_pcms();
 
	if (mhpckm_war == -1)
	{
		readVal = *((volatile uint64_t *) MHPCKM_WAR_ADDR);
		readVal &= MACE_ID_MASK;

		if (readVal == MACE_ID_REV1)
			mhpckm_war = 1;
		else if (readVal == MACE_ID_REV2)
			mhpckm_war = 0;
	}

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

	return (0);
}

static int
_kbdopen(IOBLOCK *iob, int device)
{
	if (device == KEYBOARD_PORT) {
		(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ENABLE);
		flags = (iob->Flags & F_ECONS) ? 0 : TRANSLATE;
		CIRC_FLUSH(&port.buf);
	} else 
		(void)pckm_pollcmd(MOUSE_PORT,CMD_ENABLE);

	iob->Flags |= F_SCAN;

	return(0);
}

static int
_kbdread(IOBLOCK *iob, int device)
{
	register struct device_buf *db;
	char kbdbuf[48], *p = (char *)iob->Address;
	int i, cnt = iob->Count;
	register int c,cc;

	if (device == MOUSE_PORT) 
		return(iob->Count);

	db = &port.buf; 

	while (iob->Count > 0) {
		while (c = kbdgetc(device)) {
			if (flags & TRANSLATE) {
				cc = kbd_translate(c, kbdbuf);
#if IP22
				if (!is_fullhouse()) {
				    cc = kbd_functionkey(
					kbdbuf, cc, sizeof(kbdbuf));
				}
#endif
				for (i = 0; i < cc; i++)
					_ttyinput(db, kbdbuf[i]); 
			}
			else 
				_circ_putc(c,db);
		}
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
	register int i, c, cc;
	char kbdbuf[48];

	while  (c = kbdgetc(device)) {
		if (device == MOUSE_PORT)
			continue;
		else if (flags & TRANSLATE) {
			cc = kbd_translate(c, kbdbuf);
#if IP22
			if (!is_fullhouse()) {
			    cc = kbd_functionkey(
				kbdbuf, cc, sizeof(kbdbuf));
			}
#endif
			for (i = 0; i < cc; i++)
				_ttyinput(db, kbdbuf[i]); 
		}
		else 
			_circ_putc(c,db);
	}
	/* _kbdpoll(0) is used to clear input from inside driver */
	if (iob) iob->ErrorNumber = ESUCCESS;
}

static int
_kbdioctl(IOBLOCK *iob, int device)
{
	register struct device_buf  *db = &port.buf; 

	if (device == MOUSE_PORT)
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

	if (device == MOUSE_PORT)
		return(iob->ErrorNumber = EAGAIN);

	iob->Count = _circ_nread(db);
	return(iob->Count ? ESUCCESS : (iob->ErrorNumber = EAGAIN));
}

int
_kbd_strat(COMPONENT *self, IOBLOCK *iob)
{
	int device = self->Type == PointerPeripheral;

	switch (iob->FunctionCode) {
	case FC_INITIALIZE:
		return(device?_pcmsinit(iob):_kbdinit(iob));

	case FC_OPEN:
		return(_kbdopen(iob,device));

	case FC_READ:
		return(_kbdread(iob,device)); 

	case FC_CLOSE:
		return(0);

	case FC_IOCTL:
		return(_kbdioctl(iob,device));

	case FC_GETREADSTATUS:
		return(_kbdreadstat(iob,device));

	case FC_POLL:
		_kbdpoll(iob,device);

	default:
		return(iob->ErrorNumber = EINVAL);
	}
}

int
kbdgetc(device)
{
	int i;
	__uint64_t state; 
	__uint64_t data, rc;

	/*  If any mouse data is ready, it will be in rx_buf, so
	 * we must remove if we wish to get keyboard data.  We must also
	 * service the mouse quickly or we will drop data.
	 */
	state = inp(MOUSE_PORT, status);
	if ((state & PS2_SR_PARITY) || (state & PS2_SR_FRAMING))
	{
		reinit_pcms();
		return(0);
	}


	while (state & M_OBF) {
		us_delay(4);
		data = inp(MOUSE_PORT, rx_buf);

		/* cannot check KBD_BATFAIL since it looks like a delta */
		if (data == KBD_BATOK)
			reinit_pcms();
		else
			ms_input(data);
		state = inp(MOUSE_PORT, status);
		if ((state & PS2_SR_PARITY) || (state & PS2_SR_FRAMING))
		{
		   reinit_pcms();
		   return(0);
		}

	}

	if (device == KEYBOARD_PORT) {
		state = inp(KEYBOARD_PORT, status);

		if ((state & PS2_SR_PARITY) || (state & PS2_SR_FRAMING))
		{
/* ttyprintf("Kbd Parity Error\n"); */
		   for (i = 0; i < (1 << 20); i++);
		   reinit_kbd();
		   for (i = 0; i < (1 << 20); i++);
		}

		if (state & K_OBF) {
			data = inp(KEYBOARD_PORT, rx_buf);
			if ((data == KBD_BATOK) || (data == KBD_BATFAIL)) {
				reinit_kbd();
				return(0);
			}
			return(data);
		}
		return(0);
	}
	else
		rc = 0;
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

#if IP22
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

        int data, retry = 1;

        do {
				/* no need to disable keyboard since they are different ports */
                pckm_flushinp(MOUSE_PORT);

                /* probe mouse with by seeing if default command works */
                data = pckm_pollcmd(MOUSE_PORT,CMD_DEFAULT);

        } while ((retry--) && data != KBD_ACK);

	if (data != KBD_ACK) {
#ifdef PROBEDEBUG
		ttyprintf("mouse disable failed %x\n",data);
#endif
		return(ENODEV);
	}

	/* default sample rate is 100 -- lower to 40 */
	pckm_flushinp(MOUSE_PORT);
	(void)pckm_pollcmd(MOUSE_PORT,CMD_MSSAMPL);
	(void)pckm_pollcmd(MOUSE_PORT,MS_SAMPL_40SEC);

	return(ESUCCESS);
}

static int
reset_kbd(void) 
{
	extern int _prom;
	int data;

#ifdef PCDEBUG
	ttyprintf("reset_kbd()\n");
#endif

	/* no need to disable mouse since they are different ports */
	pckm_flushinp(KEYBOARD_PORT);

	/* probe for keyboard with default diable command.  If we
	 * still have bat info around, try again.
	 */
	do {
		data = pckm_pollcmd(KEYBOARD_PORT,CMD_DISABLE);
	} while (data == KBD_BATOK);

	/* If probe timed out, then assume there is no keyboard.
	 */
	if (data == -1) {
#ifdef PROBEDEBUG
		ttyprintf("keyboard disable failed %x\n",data);
#endif
		return(ENODEV);
	}

	if (_prom)		/* ring bell to acknowledge keyboard init */
		bell();

	/* set mode 3 */
	pckm_flushinp(KEYBOARD_PORT);
	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SELSCAN);
	(void)pckm_pollcmd(KEYBOARD_PORT,3);
	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMBTYPE);
	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
	(void)pckm_pollcmd(KEYBOARD_PORT,SCN_CAPSLOCK);
	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
	(void)pckm_pollcmd(KEYBOARD_PORT,SCN_NUMLOCK);

	return(ESUCCESS);
}


static void
reinit_pcms() 
{
	int i;

#ifdef PCDEBUG
	ttyprintf("pckm: reinit mouse.\n");
#endif

	us_delay(250);
	pckm_flushinp(MOUSE_PORT);

	(void)pckm_pollcmd(MOUSE_PORT,CMD_MSSAMPL);
	(void)pckm_pollcmd(MOUSE_PORT,MS_SAMPL_40SEC);

	us_delay(250);
	pckm_flushinp(MOUSE_PORT);

	(void)pckm_pollcmd(MOUSE_PORT,CMD_ENABLE);

	/* hard flush since ctrl seems to get hozed sometimes */
	for (i=0; i < 20; i++) {
		us_delay(250);
		(void) inp(MOUSE_PORT, rx_buf); 
	}
}

static void
reinit_kbd() 
{
#ifdef PCDEBUG
	ttyprintf("pckm: reinit keyboard.\n");
#endif

	us_delay(250);
	pckm_flushinp(KEYBOARD_PORT);

	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_SELSCAN);
	(void)pckm_pollcmd(KEYBOARD_PORT,3);
	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ALLMBTYPE);
	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
	(void)pckm_pollcmd(KEYBOARD_PORT,SCN_CAPSLOCK);
	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_KEYMB);
	(void)pckm_pollcmd(KEYBOARD_PORT,SCN_NUMLOCK);

	us_delay(250);
	pckm_flushinp(KEYBOARD_PORT);

	(void)pckm_pollcmd(KEYBOARD_PORT,CMD_ENABLE);
}

static void
pckm_setleds(int bits)
{
	if (pckm_pollcmd(KEYBOARD_PORT,CMD_SETLEDS) != KBD_ACK)
		return;
	(void)pckm_pollcmd(KEYBOARD_PORT,bits&7);
}

static void
pckm_sendok(int port)
{
	int data, sr, retry = 20834;		/* ~1/4 second */
	char kbdbuf[32];

	do {
		us_delay(2);
		sr = inp(port, status); 
		if (sr & M_MASK) {
			us_delay(4);
			data = inp(port, rx_buf);
			if (port == KEYBOARD_PORT)
				kbd_translate(data,kbdbuf);
			us_delay(4);
			continue;
		}
	} while ( (!(sr & PS2_SR_TBE)) && --retry) ;
	us_delay(2);
}

/*
 * send a byte out to port
 * if clkinh is set, inhibit clock after transmission. This is used
 * for back to back transmission.
 */
outb(int port, int data, int clkinh)
{
    /*
     * to send a byte out, we must do the following
     *
     * 1) wait for xmission/receive to finish
     * 2) inhibit clock
     * 3) write data to transmit buffer
     * 4) assert clock and enable transmit to start transfer
     */

    /* first make sure it is ok to send out */
    pckm_sendok(port);

	outp(port, control, 0);	/* De-assert clken */
	us_delay(100);
	outp(port, tx_buf, data);	/* write data to buffer */
	outp(port, control, (PS2_CMD_TxEN | PS2_CMD_CLKASS | clkinh));
}

uint
pckm_pollcmd(int port, int cmd)
{
	int timeout,data,sr;
	int rcnt=3;		/* try up to four times */

again:
	/* send the cmd byte out */
	outb(port, cmd, 0);

	/* wait for report */
	for (timeout=400; timeout ; timeout--) {
		sr = inp(port, status);

		if (mhpckm_war == 0) {
			if ((sr & PS2_SR_PARITY) && rcnt) {
				(void)inp(port, rx_buf);	/* flush buffer */
				rcnt--;
				goto again;
			}
		}

		if (sr & PS2_SR_RBF) {
			us_delay(2);
			data = inp(port, rx_buf);

			if (mhpckm_war == 1) {
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
		us_delay(250);
	}

	/* return the command for safety's sake */
	if (rcnt) {
		rcnt--;
		goto again;
	}

#ifdef PROBEDEBUG
	ttyprintf("poll failed port %d cmd %x\n",port,cmd);
#endif

	return(-1);
}

static void
pckm_flushinp(int port)
{
	int data, sr, retry = 25000;		/* ~1/4 second */
	char kbdbuf[32];

	us_delay(2);
	while (((sr = inp(port, status)) & M_MASK) && --retry) {
		us_delay(2);
		data = inp(port, rx_buf);
		if (port == KEYBOARD_PORT)
			kbd_translate(data,kbdbuf);
		us_delay(2);
	}
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


int
pckbd_install(COMPONENT *parent)
{
	COMPONENT *ctr;
	char *nogfxkbd;

#if defined(DEBUG) && 0		/* KB_REG_64 not defined for IP32 -wsm8/7/97 */
	if (badaddr((void *)KB_REG_64,1) || (inp(KB_REG_64) == 0xff))
		return(-1);
#endif

	ctr = AddChild(parent,&kbdctrltmpl,(void *)NULL);
	if (ctr == (COMPONENT *)NULL) cpu_acpanic("pckbd ctrl");

	if (kbd_exists || !reset_kbd() ||
	    ((nogfxkbd=getenv("nogfxkbd")) && *nogfxkbd == '1')) {
		ctr = AddChild(ctr,&kbdtempl,(void *)NULL);
		if (ctr == (COMPONENT *)NULL) cpu_acpanic("pckbd");
		RegisterDriverStrategy(ctr,_kbd_strat);
		pckm_setupmap();
		kbd_exists = 1;
	}

	ctr = AddChild(parent,&pcmsctrltmpl,(void *)NULL);
	if (ctr == (COMPONENT *)NULL) cpu_acpanic("pcms ctrl");

	if (pcms_exists || !reset_pcms()) {
		ctr = AddChild(ctr,&pcms,(void *)NULL);
		RegisterDriverStrategy(ctr,_kbd_strat);
		pcms_exists = 1;
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
#else
extern int _prom;
#endif /* cpu check */
