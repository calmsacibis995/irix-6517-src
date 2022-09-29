#ifndef __SYS_DEVICE_ID_H__
#define __SYS_DEVICE_ID_H__

#ident $Revision: 1.2 $
	/* Device Id's for use with interrupt functions. */
#define DEV_SW_UNUSED0		0
#define	DEV_SW_UNUSED1		1
#define	DEV_FPU			2
#define	DEV_LOCAL0		3
#define	DEV_LOCAL1		4
#define	DEV_TIMER0		5
#define	DEV_TIMER1		6
#define	DEV_PARITY		7

#define	DEV_GIO_0		8
#define DEV_IDE_DMA		9
#define	DEV_SCSI		10
#define	DEV_ETHERNET		11
#define	DEV_GRAPHICS		12
#define	DEV_DUART		13
#define	DEV_GIO_1		14
#define	DEV_VME_0		15

#define	DEV_UNUSED16		16
#define	DEV_GR1STAT		17
#define	DEV_UNUSED18		18
#define	DEV_VME_1		19
#define	DEV_DSP			20
#define	DEV_ACFAIL		21
#define	DEV_VIDOPT		22
#define	DEV_VRETRACE		23

#define	DEV_PC_KBD		24
#define	DEV_PC_IDE		25
#define	DEV_PC_UNUSED26		26
#define	DEV_PC_MOUSE		27
#define	DEV_PC_FLOPPY		28
#define	DEV_PC_UNUSED29		29
#define	DEV_PC_UART1		30
#define	DEV_PC_UART2		31
#define	DEV_PC_PPORT		32
#define	DEV_PC_UNUSED33		33
#define	DEV_PC_AUDIO		34


	/* Eisa interrupts on EISA bus 0. */
#define	DEV_EISA0_IRQ0		40
#define	DEV_EISA0_IRQ1		41
#define	DEV_EISA0_IRQ2		42
#define	DEV_EISA0_IRQ3		43
#define	DEV_EISA0_IRQ4		44
#define	DEV_EISA0_IRQ5		45
#define	DEV_EISA0_IRQ6		46
#define	DEV_EISA0_IRQ7		47
#define	DEV_EISA0_IRQ8		48
#define	DEV_EISA0_IRQ9		49
#define	DEV_EISA0_IRQ10		50
#define	DEV_EISA0_IRQ11		51
#define	DEV_EISA0_IRQ12		52
#define	DEV_EISA0_IRQ13		53
#define	DEV_EISA0_IRQ14		54
#define	DEV_EISA0_IRQ15		55


#define	DEV_MAX			35		/* Number of Devices         */
#define DEV_NONE		-1		/* All 1's => no such device */

#endif
