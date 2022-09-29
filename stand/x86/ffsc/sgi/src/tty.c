/*
 * tty.c
 *	Functions for handling the MMSC serial ports
 */


#include <vxworks.h>

#include <iolib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslib.h>
#include <ttylib.h>

#include "ffsc.h"

#include "console.h"
#include "nvram.h"
#include "tty.h"

/* Default TTY settings - typically only used on uninitialized systems */
const ttyinfo_t ttyDefaultSettings[FFSC_NUM_TTYS] = {
	{
		9600,
		TTYCF_CFVALID | TTYCF_PORTVALID,
		PORT_TERMINAL,
		TTY_DFLT_RX_BUF_SIZE,
		TTY_DFLT_TX_BUF_SIZE * 16,
	},
#ifdef ADAPTIVE_HWFC
	/*
	 * HWFC enabled since it could be a new prom (r3.2). If we cannot talk to
	 * it initially. We will adapt.
	 */
	{
		9600,
		TTYCF_CFVALID | TTYCF_PORTVALID | TTYCF_NOFFSC| TTYCF_HWFLOW,
		PORT_ELSC_U,
		TTY_DFLT_RX_BUF_SIZE * 4,
		TTY_DFLT_TX_BUF_SIZE
	},
	{
		9600,
		TTYCF_CFVALID | TTYCF_PORTVALID | TTYCF_NOFFSC| TTYCF_HWFLOW,
		PORT_ELSC_L,
		TTY_DFLT_RX_BUF_SIZE * 4,
		TTY_DFLT_TX_BUF_SIZE
	},
#else
	/*
	 * HWFC disabled since it needs more testing before release.
	 */
	{
		9600,
		TTYCF_CFVALID | TTYCF_PORTVALID | TTYCF_NOFFSC,
		PORT_ELSC_U,
		TTY_DFLT_RX_BUF_SIZE * 4,
		TTY_DFLT_TX_BUF_SIZE
	},
	{
		9600,
		TTYCF_CFVALID | TTYCF_PORTVALID | TTYCF_NOFFSC,
		PORT_ELSC_L,
		TTY_DFLT_RX_BUF_SIZE * 4,
		TTY_DFLT_TX_BUF_SIZE
	},
#endif
	{
		9600,
		TTYCF_CFVALID | TTYCF_PORTVALID |  TTYCF_OOBOK, 
		PORT_SYSTEM,
		TTY_DFLT_RX_BUF_SIZE * 4,
		TTY_DFLT_TX_BUF_SIZE
	},
	{
		9600,
		TTYCF_CFVALID | TTYCF_PORTVALID,
		PORT_ALTCONS,
		TTY_DFLT_RX_BUF_SIZE,
		TTY_DFLT_TX_BUF_SIZE
	},
	{
		9600,
		/* @@: TODO -fix for defaults. You'll want a routine to swap
		 */
#if 0
		TTYCF_CFVALID | TTYCF_PORTVALID | TTYCF_NOFFSC,
#else
		TTYCF_CFVALID | TTYCF_PORTVALID | TTYCF_OOBOK,
#endif
		PORT_DEBUG,
		TTY_DFLT_RX_BUF_SIZE,
		TTY_DFLT_TX_BUF_SIZE
	},
};

/* Current TTY settings */
ttyinfo_t ttyInfo[FFSC_NUM_TTYS];


/* Global variables */
int DISPLAYfd;			/* VGA console display */
int TTYfds[FFSC_NUM_TTYS];	/* Serial port TTYs */



/*
 * comGetInfo
 *	Get the TTY info associated with the specified COM port
 */
const ttyinfo_t *
comGetInfo(int COM)
{
	return (const ttyinfo_t *) &ttyInfo[COM - 1];
}


/*
 * comGetRxBuf
 * comGetTxBuf
 *	Returns the current value for the receive or transmit buffer
 *	size for the specified COM port, substituting defaults as needed.
 *	Returns -1 if something goes wrong.
 */
int
comGetRxBuf(int COM)
{
	return ttyGetRxBuf(COM - 1);
}

int
comGetTxBuf(int COM)
{
	return ttyGetTxBuf(COM - 1);
}


/*
 * comResetFlag
 *	Resets a control flag for the specified COM port and notifies
 *	all of the appropriate consoles. Returns a TTYR_* return code.
 */
int
comResetFlag(int COM, int16_t Flag)
{
	int i;
	int NumUpdated;

	/* Actually update the flag */
	ttyInfo[COM - 1].CtrlFlags &= ~Flag;

	/* Update NVRAM */
	(void) nvramWrite(NVRAM_TTYINFO, ttyInfo);

	/* Update the appropriate console(s) if necessary */
	NumUpdated = 0;
	for (i = 0;  i < NUM_CONSOLES;  ++i) {
		NumUpdated += cmCheckTTYFlags(consoles[i]);
	}

	return (NumUpdated > 0) ? TTYR_OK : TTYR_RESET;
}


/*
 * comSetFlag
 *	Sets a control flag for the specified COM port and notifies
 *	all of the appropriate consoles. Returns a TTYR_* return code.
 */
int
comSetFlag(int COM, int16_t Flag)
{
	int i;
	int NumUpdated;

	/* Actually update the flag */
	ttyInfo[COM - 1].CtrlFlags |= Flag;

	/* Update NVRAM */
	(void) nvramWrite(NVRAM_TTYINFO, ttyInfo);

	/* Update the appropriate console(s) if necessary */
	NumUpdated = 0;
	for (i = 0;  i < NUM_CONSOLES;  ++i) {
		NumUpdated += cmCheckTTYFlags(consoles[i]);
	}

	return (NumUpdated > 0) ? TTYR_OK : TTYR_RESET;
}


/*
 * comSetFlowCntl
 * Enable/Disable the hardware flow control
 * Index by ttyNum.
 */
STATUS
comSetFlowCntl(int ttyNum, int Mode)
{
	UINT32	dev_options; 			/* low level hardware device options */
	unsigned short drv_options;		/* driver level options (tyLib.c) */


	/*Set the device options */
	/* First go and get the current options */
	if (ioctl(TTYfds[ttyNum], SIO_HW_OPTS_GET, (int ) &dev_options) < 0) { 
			ffscMsg("Unable to get hardware dev_options on FD %d", TTYfds[ttyNum]);
			return ERROR;
	}

	if (Mode) {
		/* enable hardware flow control */
		dev_options &= ~CLOCAL;
		dev_options |= HUPCL;
		comSetFlag((ttyNum+1), TTYCF_HWFLOW);
	}
	else {
		/* disable hardware flow control */
		dev_options |= CLOCAL;
		dev_options &= ~HUPCL;
		comResetFlag((ttyNum+1), TTYCF_HWFLOW);
	}

	/* Now set the options with the new flow control mode */
	if (ioctl(TTYfds[ttyNum], SIO_HW_OPTS_SET, dev_options) < 0) {
			ffscMsg("Unable to set hardware options on FD %d", TTYfds[ttyNum]);
			return ERROR;
	}

	/*Set the driver options */
   /* First go and get the current options */
   drv_options = ioctl(TTYfds[ttyNum], FIOGETOPTIONS, 0);

   if (Mode) 
      /* enable hardware flow control */
      drv_options |= OPT_RTS;

   else 
      /* disable hardware flow control */
      drv_options &= ~OPT_RTS;

   /* Now set the options with the new flow control mode */
   if (ioctl(TTYfds[ttyNum], FIOSETOPTIONS, drv_options) < 0) {
         ffscMsg("Unable to set hardware options on FD %d", TTYfds[ttyNum]);
         return ERROR;
   }


   return OK;
}

/*
 * comSetFunction
 *	Sets the function of the specified COM port (*not* TTY)
 */
STATUS
comSetFunction(int COM, int16_t Function)
{
	int i;
	int TTYNum = COM - 1;

	/* Update our entry in ttyInfo */
	ttyInfo[TTYNum].Port = Function;
	if (ffscDebug.Flags & FDBF_CONSOLE) {
		ffscMsg("Changed port of COM%d to %d", COM, Function);
	}

	/* If anyone else is using this port, unassign them */
	for (i = 0;  i < FFSC_NUM_TTYS;  ++i) {
		if (ttyInfo[i].Port == Function  &&  i != TTYNum) {
			ttyInfo[i].Port = PORT_UNASSIGNED;
			if (ffscDebug.Flags & FDBF_CONSOLE) {
				ffscMsg("Port for COM%d is now UNASSIGNED",
					i + 1);
			}
		}
	}

	/* Update NVRAM */
	(void) nvramWrite(NVRAM_TTYINFO, ttyInfo);

	return TTYR_RESET;
}


/*
 * comSetRxBuf
 *	Sets the receive buffer size of the specified COM port (*not* TTY).
 *	Note that the change does not take effect until the MMSC is reset.
 */
STATUS
comSetRxBuf(int COM, int Size)
{
	int ttyNum = COM - 1;

	/* Update NVRAM */
	ttyInfo[ttyNum].RxBufSize = Size;
	if (nvramWrite(NVRAM_TTYINFO, ttyInfo) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * comSetSpeed
 *	Sets the baud rate of the specified COM port (*not* TTY)
 */
STATUS
comSetSpeed(int COM, int Baud)
{
	int ttyNum = COM - 1;

	/* Reset the baud rate */
	if (ttySetSpeed(TTYfds[ttyNum], Baud) != OK) {
		ffscMsgS("Unable to set baud rate to %d on COM%d",
			Baud, COM);
		return ERROR;
	}

	/* Update NVRAM */
	ttyInfo[ttyNum].BaudRate = Baud;
	if (nvramWrite(NVRAM_TTYINFO, ttyInfo) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * comSetTxBuf
 *	Sets the transmit buffer size of the specified COM port (*not* TTY).
 *	Note that the change does not take effect until the MMSC is reset.
 */
STATUS
comSetTxBuf(int COM, int Size)
{
	int ttyNum = COM - 1;

	/* Update NVRAM */
	ttyInfo[ttyNum].TxBufSize = Size;
	if (nvramWrite(NVRAM_TTYINFO, ttyInfo) != OK) {
		return ERROR;
	}

	return OK;
}


/*
 * ttyGetInfo
 *	Get the TTY info associated with the specified TTY
 */
const ttyinfo_t *
ttyGetInfo(int TTYNum, int *FDPtr)
{
	if (TTYNum < 0  ||  TTYNum >= FFSC_NUM_TTYS) {
		return NULL;
	}

	if (FDPtr != NULL) {
		*FDPtr = TTYfds[TTYNum];
	}

	return (const ttyinfo_t *) &ttyInfo[TTYNum];
}


/*
 * ttyGetRxBuf
 * ttyGetTxBuf
 *	Returns the current value for the receive or transmit buffer
 *	size for the specified TTY, substituting defaults as needed.
 *	Returns -1 if the TTY number is invalid.
 */
int
ttyGetRxBuf(int TTYNum)
{
	if (TTYNum < 0  ||  TTYNum >= FFSC_NUM_TTYS) {
		return -1;
	}

	if (ttyInfo[TTYNum].RxBufSize == 0) {
		return ttyDefaultSettings[TTYNum].RxBufSize;
	}

	return ttyInfo[TTYNum].RxBufSize;
}

int
ttyGetTxBuf(int TTYNum)
{
	if (TTYNum < 0  ||  TTYNum >= FFSC_NUM_TTYS) {
		return -1;
	}

	if (ttyInfo[TTYNum].TxBufSize == 0) {
		return ttyDefaultSettings[TTYNum].TxBufSize;
	}

	return ttyInfo[TTYNum].TxBufSize;
}


/*
 * ttyInit
 *	Initialize the TTY devices
 */
STATUS
ttyInit(void)
{
	char devName[20];
	int ix;
	int UpdateNVRAM = 0;
	int Mode;

	/* Install the TTY driver */
	if (ttyDrv() != OK) {
		fprintf(stderr, "Unable to install TTY driver\n");
	}

	/* Try to read TTY parameters from NVRAM */
	if (nvramRead(NVRAM_TTYINFO, ttyInfo) != OK) {

		/* Debugging info is not available in NVRAM, so build */
		/* some default debugging info.			      */
		bzero((char *) &ttyInfo, sizeof(ttyInfo));
		for (ix = 0;  ix < FFSC_NUM_TTYS;  ++ix) {
			ttyInfo[ix] = ttyDefaultSettings[ix];
			ttyInfo[ix].RxBufSize = 0;
			ttyInfo[ix].TxBufSize = 0;
		}

		/* Remember to update NVRAM */
		UpdateNVRAM = 1;
	}

	/* Check for any old-style ttyinfo's */
	for (ix = 0;  ix < FFSC_NUM_TTYS;  ++ix) {
		if (!(ttyInfo[ix].CtrlFlags & TTYCF_CFVALID)) {
			ttyInfo[ix].CtrlFlags =
			    ttyDefaultSettings[ix].CtrlFlags;
			ttyInfo[ix].Port =
			    ttyDefaultSettings[ix].Port;
			UpdateNVRAM = 1;
		}
		else if (!(ttyInfo[ix].CtrlFlags & TTYCF_PORTVALID)) {
			ttyInfo[ix].CtrlFlags |= TTYCF_PORTVALID;
			ttyInfo[ix].Port =
			    ttyDefaultSettings[ix].Port;
			UpdateNVRAM = 1;
		}
	}

	/* If we made any changes to the NVRAM stuff, update it */
	if (UpdateNVRAM) {
		nvramWrite(NVRAM_TTYINFO, ttyInfo);
	}

	/* Initialize the I/O ports to "uninitialized" */
	for (ix = 0;  ix < NUM_PORTS;  ++ix) {
		ffscPortFDs[ix] = -1;
	}
	/* Initialize each of the TTY devices (i.e. serial ports) */
	for (ix = 0;  ix < FFSC_NUM_TTYS;  ix++) {
		int Flags;

#if (defined(INCLUDE_WDB) && (WDB_COMM_TYPE == WDB_COMM_UDPL_SLIP))
		/* Do not attempt to initialize WDB's channel */
		if (ix == WDB_TTY_CHANNEL) {
			continue;
		}
#endif

		/* Create a TTY device */
		sprintf(devName, "/tyCo/%d", ix);
		if (ttyDevCreate(devName,
				 sysSerialChanGet(ix),
				 ttyGetRxBuf(ix),
				 ttyGetTxBuf(ix)) != OK)
		{
			fprintf(stderr,
				"Unable to initialize TTY %s\n",
				devName);
			continue;
		}

		/* Open the device */
		TTYfds[ix] = open(devName, O_RDWR, 0);
		if (TTYfds[ix] < 0) {
			fprintf(stderr, "Unable to open %s\n", devName);
			continue;
		}

		/* Initialize the device's baud rate */
		if (ttySetSpeed(TTYfds[ix], ttyInfo[ix].BaudRate) != OK) {
			fprintf(stderr,
				"Unable to set baud rate of %s to %ld\n",
				devName, ttyInfo[ix].BaudRate);

			close(TTYfds[ix]);
			TTYfds[ix] = -1;

			continue;
		}

#ifdef OLD_CODE
		/**
		 * NOTE: previous versions of MMSC firmware ( < 1.3) would
		 * use the special option "OPT_TERMINAL" to allow the tty
		 * to drop into the "monitor" which is really the VxWorks
		 * shell. We get rid of that here and init all ports in
		 * raw mode now that we want no interpretations done on 
		 * COM6. This is left as a gratuitous example.
		 *
		 * Initialize the device's mode flags 
		 */
		if (ttyInfo[ix].Port == PORT_DEBUG) {
			Flags = OPT_TERMINAL;
		}
		else {
			Flags = OPT_RAW;
		}
#else
		Flags = OPT_RAW;
#endif

		if (ioctl(TTYfds[ix], FIOSETOPTIONS, Flags) < 0) {
			fprintf(stderr,
				"Unable to set options of %s to 0x%x\n",
				devName, Flags);

			close(TTYfds[ix]);
			TTYfds[ix] = -1;

			continue;
		}

		/* Create a mapping to this TTY's assigned port */
		if (ttyInfo[ix].Port >= 0) {
			ffscPortFDs[ttyInfo[ix].Port] = TTYfds[ix];
		}

		/* Initialize the Hardware flow control mode */
		if (ttyInfo[ix].CtrlFlags & TTYCF_HWFLOW)
			Mode = 1;
		else
			Mode = 0;
		comSetFlowCntl(ix, Mode);
		
	}

	return OK;
}


/*
 * ttyReInit
 *	Reinitialize the serial port hardware
 */
STATUS
ttyReInit(void)
{
	int i;
	void sysSerialHwInit(void);

	/* Initialize all of the serial ports */
	sysSerialHwInit();

	/* Set the interrupt mode on each port */
	for (i = 0;  i < FFSC_NUM_TTYS;  ++i) {
		if (ioctl(TTYfds[i], SIO_MODE_SET, SIO_MODE_INT) != OK) {
			ffscMsg("Unable to set mode on FD %d", TTYfds[i]);
		}
	}

	return OK;
}


/*
 * ttySetSpeed
 *	Sets the baud rate of a serial port
 */
STATUS
ttySetSpeed(int FD, int Speed)
{
	return (ioctl(FD, FIOBAUDRATE, Speed) < 0) ? ERROR : OK;
}



/*
 * Toggles the HWFC on the given port and reinitializes the tty 
 * library and devices.
 */

STATUS ttyToggleHwflow(int fd)
{
  int i, status = -1;
  int comPort;

  for(i = 0; i < FFSC_NUM_TTYS;++i){
    /* COM port */
    comPort = ttyInfo[i].Port;
    /* Compare file descriptor to see if it is ours.*/
    if(ffscPortFDs[comPort] == fd){
      if (ttyInfo[i].CtrlFlags & TTYCF_HWFLOW){
	/*
	 *  If it is currently on, then shut it off, 
	 * and reinitialize the serial channels.
	 */
	comSetFlowCntl(i, 0); /* off we go */
	ttyInfo[i].CtrlFlags &= ~TTYCF_HWFLOW;
	/* Restart the TTY Channel */
	sioTxStartup(sysSerialChanGet(comPort));
	status = 0;

      }
      else{
	/* Turn it on */
	comSetFlowCntl(i, 1);
	ttyInfo[i].CtrlFlags |= TTYCF_HWFLOW;
	/* Restart the TTY Channel */
	sioTxStartup(sysSerialChanGet(comPort));
	status = 0;
      }
    }
  }
  
  if(status < 0)
    return ERROR;
  else
    return OK;
}


/*----------------------------------------------------------------------*/
/*									*/
/*			      DEBUG FUNCTIONS				*/
/*									*/
/*----------------------------------------------------------------------*/

#ifndef PRODUCTION

/*
 * ttyinfo
 *	Dump what we know about TTYs
 */
void
ttyinfo(void)
{
	int i;

	ffscMsg("\r\nTTY Information:");
	for (i = 0;  i < FFSC_NUM_TTYS;  ++i) {
		int Mode;
		int NRead;
		int NWrite;

		ffscMsg("/tyCo/%d (COM%d): %6ld baud  ctrl 0x%04x",
			i, i+1,
			ttyInfo[i].BaudRate,
			(unsigned short) ttyInfo[i].CtrlFlags);
		ffscMsg("                 AFTER RESET - port %d  rxbuf %d "
			    "txbuf %d",
			ttyInfo[i].Port,
			ttyInfo[i].RxBufSize,
			ttyInfo[i].TxBufSize);

		Mode = -1;
		ioctl(TTYfds[i], SIO_MODE_GET, (int) &Mode);

		NRead = -1;
		ioctl(TTYfds[i], FIONREAD, (int) &NRead);

		NWrite = -1;
		ioctl(TTYfds[i], FIONWRITE, (int) &NWrite);

		ffscMsg("                 STATE - mode %d  nread %d  "
			    "nwrite %d",
			Mode, NRead, NWrite);

		ffscMsg("");
	}
}


/*
 * ttyreinit
 *	Reinitialize the serial port hardware
 */
STATUS
ttyreinit(void)
{
	return ttyReInit();
}


/*
 * ttyspeed
 *	Sets the baud rate of the specified COM port (*not* TTY #)
 */
STATUS
ttyspeed(int COM, int Baud)
{
	return comSetSpeed(COM, Baud);
}


void
setflow() {
   /* Initialize the Hardware flow control mode (for COM 1-6) */
int ix;
int Mode;
   for (ix = 0; ix < FFSC_NUM_TTYS; ix++) {
      if (ttyInfo[ix].CtrlFlags & TTYCF_HWFLOW)
         Mode = 1;
      else
         Mode = 0;

      comSetFlowCntl(ix, Mode);
   }
}



#endif  /* !PRODUCTION */











