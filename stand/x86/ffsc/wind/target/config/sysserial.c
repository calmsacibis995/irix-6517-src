/* sysSerial.c - EPC4 BSP serial device initialization */

/* Copyright 1984-1995 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01f,15jul97,blm  Added 16550 support.
01e,23oct95,jdi  doc: cleaned up and removed all NOMANUALs.
01d,03aug95,myz  fixed the warning message
01c,20jun95,ms   fixed comments for mangen
01b,15jun95,ms	 updated for new serial driver
01a,15mar95,myz  written based on mv162 version.
*/

#include "vxworks.h"
#include "iv.h"
#include "intlib.h"
#include "config.h"
#include "syslib.h"
#include "drv/serial/ns16552.h"

/* device initialization structure */

typedef struct
    {
    USHORT vector;
    ULONG  baseAdrs;
    USHORT regSpace;
    USHORT intLevel;
    } NS16550_CHAN_PARAS;

#ifdef INCLUDE_PC_CONSOLE       /* if key board and VGA console needed */

#include "serial/pcconsole.c"
#include "serial/m6845vga.c"

#if (PC_KBD_TYPE == PC_PS2_101_KBD)     /* 101 KEY PS/2                 */
#include "serial/i8042kbd.c"
#else
#include "serial/i8048kbd.c"            /* 83 KEY PC/PCXT/PORTABLE      */
#endif /* (PC_KBD_TYPE == PC_XT_83_KBD) */

#endif /* INCLUDE_PC_CONSOLE */


/* Local data structures */

static NS16550_CHAN  ns16550Chan[N_UART_CHANNELS];

static NS16550_CHAN_PARAS devParas[] = 
    {
      {COM1_INT_VEC,COM1_BASE_ADR,UART_REG_ADDR_INTERVAL,COM1_INT_LVL},
      {COM2_INT_VEC,COM2_BASE_ADR,UART_REG_ADDR_INTERVAL,COM2_INT_LVL},
      {COM3_INT_VEC,COM3_BASE_ADR,UART_REG_ADDR_INTERVAL,COM3_INT_LVL},
      {COM4_INT_VEC,COM4_BASE_ADR,UART_REG_ADDR_INTERVAL,COM4_INT_LVL},
      {COM5_INT_VEC,COM5_BASE_ADR,UART_REG_ADDR_INTERVAL,COM5_INT_LVL},
      {COM6_INT_VEC,COM6_BASE_ADR,UART_REG_ADDR_INTERVAL,COM6_INT_LVL}
    };

#define UART_REG(reg,chan) \
(devParas[chan].baseAdrs + reg*devParas[chan].regSpace)

/******************************************************************************
*
* sysSerialHwInit - initialize the BSP serial devices to a quiesent state
*
* This routine initializes the BSP serial device descriptors and puts the
* devices in a quiesent state.  It is called from sysHwInit() with
* interrupts locked.
*
* RETURNS: N/A
*/

void sysSerialHwInit (void)
    {
    int i;

	for (i=0;i<N_UART_CHANNELS;i++) {
		ns16550Chan[i].channelMode = 0;
		ns16550Chan[i].xtal        = UART_XTAL;

		ns16550Chan[i].regs      = (UINT8 * ) devParas[i].baseAdrs;
		ns16550Chan[i].level     = devParas[i].intLevel;
		ns16550Chan[i].regDelta  = devParas[i].regSpace;
  

		ns16550Chan[i].data_add = UART_REG(UART_RDR, i);
		ns16550Chan[i].ier_add = UART_REG(UART_IER, i);
		ns16550Chan[i].fcr_add = UART_REG(UART_FCR, i);
		ns16550Chan[i].isr_add = UART_REG(UART_ISR, i);
		ns16550Chan[i].lcr_add = UART_REG(UART_LCR, i);
		ns16550Chan[i].mcr_add = UART_REG(UART_MCR, i);
		ns16550Chan[i].lsr_add = UART_REG(UART_LSR, i);
		ns16550Chan[i].msr_add = UART_REG(UART_MSR, i);
		ns16550Chan[i].spr_add = UART_REG(UART_SPR, i);
		ns16550Chan[i].dll_add = UART_REG(UART_DLL, i);
		ns16550Chan[i].dlm_add = UART_REG(UART_DLM, i);

   	ns16550Chan[i].outByte = sysOutByte;
   	ns16550Chan[i].inByte  = sysInByte;

		ns16550DevInit(&ns16550Chan[i]);

 	}
}

/******************************************************************************
*
* sysSerialHwInit2 - connect BSP serial device interrupts
*
* This routine connects the BSP serial device interrupts.  It is called from
* sysHwInit2().  Serial device interrupts could not be connected in
* sysSerialHwInit() because the kernel memory allocator was not initialized
* at that point, and intConnect() calls malloc().
*
* RETURNS: N/A
*/

void sysSerialHwInit2 (void)
{
    int i;

    /* connect serial interrupts */

	for (i=0;i<N_UART_CHANNELS; i++) {
		if (devParas[i].vector) {
			(void) intConnect (INUM_TO_IVEC (devParas[i].vector),
										ns16550Int, (int)&ns16550Chan[i] );
			sysIntEnablePIC (devParas[i].intLevel);
		}
	}
}


/******************************************************************************
*
* sysSerialChanGet - get the SIO_CHAN device associated with a serial channel
*
* This routine gets the SIO_CHAN device associated with a specified serial
* channel.
*
* RETURNS: A pointer to the SIO_CHAN structure for the channel, or ERROR
* if the channel is invalid.
*/

SIO_CHAN * sysSerialChanGet
    (
    int channel      /* serial channel */
    )
    {
		if (channel < N_UART_CHANNELS)
    		return ( (SIO_CHAN *)&ns16550Chan[channel] );
		else
    		return (ERROR);
    }


