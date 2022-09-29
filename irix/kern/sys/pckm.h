/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_PCKM_H__
#define __SYS_PCKM_H__

/*
 * defines for Award 8242WB PC (PS/2 compat) kbd/ms controller.
 */
#ident "$Revision: 1.28 $"

#if IP22 || IP26 || IP28
#define KB_REG_60	PHYS_TO_K1(HPC3_KBD_MOUSE0+3)	/* cntrl regs */
#define KB_REG_64	PHYS_TO_K1(HPC3_KBD_MOUSE1+3)
#endif

#if SN || IP30
#define IOC3_PCKM	1
#endif

#if defined(IP32) || defined(IPMHSIM)
#define MACE_PCKM	1
#endif

#define KBD_NOERR	0x55			/* kbd return codes */
#define KBD_BATOK	0xaa
#define KBD_BREAK	0xf0
#define KBD_ACK		0xfa
#define KBD_BATFAIL	0xfc
#define KBD_RESEND	0xfe
#define KBD_OVERRUN	0xff

#define MOUSE_ID        0x00			/* Mouse/Keyboard id numbers */
#define KEYBD_ID     	0x83
#define KEYBD_BEEP_ID1 	0x53
#define KEYBD_BEEP_ID2 	0x47

#define SR_OBF		0x01			/* status register */
#define SR_IBF		0x02
#define SR_SYSTEM	0x04
#define SR_COMMAND	0x08
#define SR_INHIBIT	0x10
#define SR_MSFULL	0x20
#define SR_TO		0x40
#define SR_PARITY	0x80

#define CB_KBINTR	0x01			/* command byte */
#define CB_MSINTR	0x02
#define CB_SYSTEM	0x04
#define CB_KBDISABLE	0x10
#define CB_MSDISABLE	0x20

#define CMD_RCB		0x20			/* ctrl/kbd/ms commands */
#define CMD_WCB		0x60
#define CMD_RSTCTL	0xaa
#define CMD_ID_RESP     0xab
#define CMD_MSDISABLE	0xa7
#define CMD_MSENABLE	0xa8
#define CMD_KBDISABLE	0xad
#define CMD_KBENABLE	0xae
#define CMD_NEXTMS	0xd4
#define CMD_MSRES	0xe8
#define CMD_MSSTAT	0xe9
#define CMD_SETLEDS	0xed
#define CMD_SELSCAN	0xf0
#define CMD_ID          0xf2
#define CMD_MSSAMPL	0xf3
#define CMD_ENABLE	0xf4
#define CMD_DISABLE	0xf5
#define CMD_DEFAULT	0xf6
#define CMD_ALLMB	0xf8
#define CMD_ALLMBTYPE	0xfa
#define CMD_KEYMB	0xfc
#define CMD_RESEND	0xfe
#define CMD_MSRESET	0xff

#define MS_SAMPL_10SEC	0x0a
#define MS_SAMPL_20SEC	0x14
#define MS_SAMPL_40SEC	0x28
#define MS_SAMPL_60SEC	0x3c
#define MS_SAMPL_80SEC	0x50
#define MS_SAMPL_100SEC	0x64
#define MS_SAMPL_200SEC	0xcb

#define LED_SCROLL	0x01L
#define LED_NUMLOCK	0x02L
#define LED_CAPSLOCK	0x04L
#define EXTRA_LED_MASK	0xf8		/* some kbds up upper bits for leds */
#define EXTRA_LED_SHIFT	0x03

#define SCN_CAPSLOCK	0x14
#define SCN_NUMLOCK	0x76

#define PCKMFB		*(volatile int *)PHYS_TO_K1(0x1fc00000)
#define inp(port)	(*(volatile unsigned char *)(port))
#define outp(port,val)	(*(volatile unsigned char *)(port) = (val)),PCKMFB

#define I2CP(X)		{ (char)(X), (char)(X) }
#define KBDMAPNIL	""

/* textport keymap structure.
 */
struct key_type {
	short k_scan;
	short k_flags;			/* see KEY_ below */
	char k_nvalue[2];		/* normal (and k_keystate) */
	char k_svalue[2];		/* shift (and k_ledbits) */
	char k_cvalue[2];		/* control */
	char k_avalue[2];		/* alt */
};

#define KEY_NUMLOCK	0x01		/* pay attention to num lock */
#define KEY_CAPLOCK	0x02		/* pay attention to caps lock */
#define KEY_CHSTATE	0x04		/* key changes state */
#define KEY_CHLED	0x08		/* key changes led and state */
#define KEY_CAPNORM	0x10		/* subtract to get caplock value */
#define KEY_CAPSHFT	0x20		/* subtrack to get S-caplock value */

#define S_NUMLOCK	0x0001
#define S_CAPSLOCK	0x0002
#define S_SCROLLLOCK	0x0004
#define S_LEFTALT	0x0008
#define S_RIGHTALT	0x0010
#define S_LEFTSHIFT	0x0020
#define S_RIGHTSHIFT	0x0040
#define S_LEFTCONTROL	0x0080
#define S_RIGHTCONTROL	0x0100		/* not used in maps (use only 8 bits) */

#define KBD_EXT_TABLE	(char)0xff
#define KBD_F1		0
#define KBD_F2		1
#define KBD_F3		2
#define KBD_F4		3
#define KBD_F5		4
#define KBD_F6		5
#define KBD_F7		6
#define KBD_F8		7
#define KBD_F9		8
#define KBD_F10		9
#define KBD_F11		10
#define KBD_F12		11
#define KBD_UP		12
#define KBD_DOWN	13
#define KBD_LEFT	14
#define KBD_RIGHT	15
#define KBD_MS		16

#define S_ALLSHIFT	(S_LEFTSHIFT|S_RIGHTSHIFT)
#define S_ALLCONTROL	(S_LEFTCONTROL|S_RIGHTCONTROL)
#define S_ALLALT	(S_LEFTALT|S_RIGHTALT)

#if IOC3_PCKM
#include <sys/iobus.h>
#include <sys/PCI/ioc3.h>

struct pckm_data_info {
	uint	valid_bit;
	uint	data_bits;
	uint	shift_bits;
	uint	frame_err;
};

#define PCKM_DATA_VALID(X)	((X & p->valid_bit) && !(X & p->frame_err))
#define PCKM_DATA_SHIFT(X)	((X & p->data_bits) >> p->shift_bits)
#define PCKM_GOT_DATA(X)        (X & p->valid_bit)
#define PCKM_FRAME_ERR(X)       (X & p->frame_err)
#endif	/* IOC3_PCKM */

#if defined(_KERNEL) && !defined(_STANDALONE)
extern void pckm_setleds(int newvalue);
extern void initkbdtype(void);
extern void early_pckminit(void);
extern int pckm_kbdexists(void);
#if defined(SN) || defined(IP30)
extern void pckm_bell(void);
#endif
#endif

#if defined(_STANDALONE)
uint pckm_pollcmd(int, int);
#endif

#if MACE_PCKM

/*
 * The moosehead system I/O asic contains a simple PS/2 interface that 
 * supports both PS/2 style PC keyboards and mice. The intercace supports 
 * two connections that are intended to be used, one for the keyboard and
 * one for the mouse.
 */

/* moosehead register programming interface */
struct ps2if {
    __uint64_t tx_buf;      /* transmit buffer (8 bit) */
    __uint64_t rx_buf;      /* receive buffer (15 bit) */
    __uint64_t control; 	/* command and control register */
    __uint64_t status;      /* tx and rx status and error register */
};

/* keyboard and mouse address map */
#define KEYBOARD_PORT_ADDR  	PHYS_TO_K1(MACE_KBDMS)
#define MOUSE_PORT_ADDR 		PHYS_TO_K1(MACE_KBDMS+0x20)

/* PS/2 command and control register */
#define PS2_CMD_CLKINH  0x01    /* inhibit clock after xmission */
#define PS2_CMD_TxEN    0x02    /* transmit enable */
#define PS2_CMD_TxIEN   0x04    /* transmit interrupt enable */
#define PS2_CMD_RxIEN   0x08    /* receive interrupt enable */
#define PS2_CMD_CLKASS  0x10    /* assert clock */

/* PS/2 status register */
#define PS2_SR_CLKSIG   0x01    /* clock signal */
#define PS2_SR_CLKINH   0x02    /* clock inhibit */
#define PS2_SR_TIP      0x04    /* transmission in progress */
#define PS2_SR_TBE      0x08    /* transmit buffer empty */
#define PS2_SR_RBF      0x10    /* receive buffer full */
#define PS2_SR_RIP      0x20    /* reception in progress */
#define PS2_SR_PARITY   0x40    /* parity error */
#define PS2_SR_FRAMING  0x80    /* framing error */

/* keybd and mouse interrupt bits */
#define MACE_KEYBD_INTR	0x200
#define MACE_MOUSE_INTR	0x800
#endif	/* MACE_PCKM */

#endif
