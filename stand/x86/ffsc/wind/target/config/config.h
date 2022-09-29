/* pc386/config.h - PC [34]86 configuration header */

/*
modification history
--------------------
01s,28sep95,dat  new BSP revision id
01r,14jun95,hdn  added INCLUDE_SW_FP for FPP software emulation library.
01q,08jun95,ms   changed PC_CONSOLE defines.
01p,12jan95,hdn  changed SYS_CLK_RATE_MAX to a safe number.
01o,08dec94,hdn  changed EEROM to EEPROM.
01n,15oct94,hdn  changed CONFIG_ELC and CONFIG_ULTRA.
		 added INCLUDE_LPT for LPT driver.
		 added INCLUDE_EEX32 for Intel EtherExpress32.
		 changed the default boot line.
		 moved INT_VEC_IRQ0 to pc.h.
01m,03jun94,hdn  deleted shared memory network related macros.
01l,28apr94,hdn  changed ROM_SIZE to 0x7fe00.
01k,22apr94,hdn  added macros INT_VEC_IRQ0, FD_DMA_BUF, FD_DMA_BUF_SIZE.
		 moved a macro PC_KBD_TYPE from pc.h.
		 added SLIP driver with 9600 baudrate.
01j,15mar94,hdn  changed ULTRA configuration.
		 changed CONSOLE_TTY number from 0 to 2.
01i,09feb94,hdn  added 3COM EtherlinkIII driver and Eagle NE2000 driver.
                 changed RAM_HIGH_ADRS and RAM_LOW_ADRS.
                 changed LOCAL_MEM_SIZE to 4MB.
01h,27jan94,hdn  changed RAM_HIGH_ADRS 0x110000 to 0x00108000.
		 changed RAM_ENTRY 0x10000 to 0x00008000.
01g,17dec93,hdn  added support for Intel EtherExpress driver.
01f,24nov93,hdn  added INCLUDE_MMU_BASIC.
01e,08nov93,vin  added support for pc console drivers.
01d,03aug93,hdn  changed network board's address and vector.
01c,22apr93,hdn  changed default boot line.
01b,07apr93,hdn  renamed compaq to pc.
01a,15may92,hdn  written based on frc386.
*/

/*
This module contains the configuration parameters for the
PC [34]86.
*/

#ifndef	INCconfigh
#define	INCconfigh

/* BSP version/revision identification, before configAll.h */
#define BSP_VER_1_1	1
#define BSP_VERSION	"1.1"
#define BSP_REV		"/0"	/* 0 for first revision */

#include "configall.h"
#include "pc.h"


/*
 * The define's in this symbol are absorbed from localdefs.h, which in
 * turn is generated automatically from symbols in bootdefs
 */
#define DEFAULT_BOOT_LINE \
"ene(0,0)" HOST_NAME ":" IMAGE_PATH " h=" HOST_IP_ADDR \
" e=" TARGET_IP_ADDR " tn=" TARGET_NAME \
" u=" FTP_NAME " pw=" FTP_PASSWD \
" f=" BOOT_FLAGS


#define INCLUDE_MMU_BASIC 	/* bundled mmu support */
#undef VM_PAGE_SIZE
#define VM_PAGE_SIZE		4096

/* XXX for the Pentium data cache with write-back mode
#undef USER_D_CACHE_MODE
#define USER_D_CACHE_MODE	CACHE_COPYBACK
*/

#define INCLUDE_SW_FP		/* software floating point emulation */

#undef INCLUDE_EX		/* include Excelan Ethernet interface */
#undef INCLUDE_ENP		/* include CMC Ethernet interface*/
#undef INCLUDE_SM_NET		/* include backplane net interface */
#undef INCLUDE_SM_SEQ_ADDR	/* shared memory network auto address setup */
#undef INCLUDE_ELC		/* include SMC Elite16 interface */
#undef INCLUDE_ULTRA		/* include SMC Elite16 Ultra interface */
#define INCLUDE_ENE		/* include Eagle/Novell NE2000 interface */
#undef INCLUDE_ELT		/* include 3COM EtherLink III interface */
#undef INCLUDE_EEX		/* include INTEL EtherExpress interface */
#undef INCLUDE_EEX32		/* include INTEL EtherExpress flash 32 */
#undef INCLUDE_SLIP            /* include serial line interface */
#undef SLIP_TTY        1       /* serial line IP channel COM2 */
#undef SLIP_BAUDRATE 9600      /*   baudrate 9600 */


#undef  INCLUDE_DOSFS		/* dosFs file system */
#undef  INCLUDE_FD		/* floppy disk driver */
#define FD_DMA_BUF	0x2000	/* floppy disk DMA buffer address */
#define FD_DMA_BUF_SIZE	0x3000	/* floppy disk DMA buffer size */
#undef  INCLUDE_IDE		/* hard disk driver */
#undef  INCLUDE_LPT		/* parallel port driver */

#define IO_ADRS_ELC	0x240
#define INT_LVL_ELC	0x0b
#define INT_VEC_ELC	(INT_VEC_IRQ0 + INT_LVL_ELC)
#define MEM_ADRS_ELC	0xc8000
#define MEM_SIZE_ELC	0x4000
#define CONFIG_ELC	0	/* 0=EEPROM 1=RJ45+AUI 2=RJ45+BNC */

#define IO_ADRS_ULTRA	0x240
#define INT_LVL_ULTRA	0x0b
#define INT_VEC_ULTRA	(INT_VEC_IRQ0 + INT_LVL_ULTRA)
#define MEM_ADRS_ULTRA	0xc8000
#define MEM_SIZE_ULTRA	0x4000
#define CONFIG_ULTRA	0	/* 0=EEPROM 1=RJ45+AUI 2=RJ45+BNC */

#define IO_ADRS_EEX	0x240
#define INT_LVL_EEX	0x0b
#define INT_VEC_EEX	(INT_VEC_IRQ0 + INT_LVL_EEX)
#define NTFDS_EEX	0x00
#define CONFIG_EEX	0	/* 0=EEPROM  1=AUI  2=BNC  3=RJ45 */
/* Auto-detect is not supported, so choose the right one you're going to use */

#define IO_ADRS_ELT	0x240
#define INT_LVL_ELT	0x0b
#define INT_VEC_ELT	(INT_VEC_IRQ0 + INT_LVL_ELT)
#define NRF_ELT		0x00
#define CONFIG_ELT	0	/* 0=EEPROM 1=AUI  2=BNC  3=RJ45 */

#define IO_ADRS_ENE	0x300
#define INT_LVL_ENE	0x09
#define INT_VEC_ENE	(INT_VEC_IRQ0 + INT_LVL_ENE)
/* Hardware jumper is used to set RJ45(Twisted Pair) AUI(Thick) BNC(Thin) */

#ifdef	INCLUDE_EEX32
#define INCLUDE_EI		/* include 82596 driver */
#define INT_LVL_EI	0x0b
#define INT_VEC_EI	(INT_VEC_IRQ0 + INT_LVL_EI)
#define EI_SYSBUS	0x44	/* 82596 SYSBUS value */
#define EI_POOL_ADRS	NONE	/* memory allocated from system memory */
/* Auto-detect is not supported, so choose the right one you're going to use */
#endif	/* INCLUDE_EEX32 */


/* miscellaneous definitions */

#define NV_RAM_SIZE     NONE            /* no NVRAM */

/* SYS_CLK_RATE_MAX depends upon a CPU power and a work load of an application.
 * The value was chosen in order to pass the internal test suit,
 * but it could go up to PIT_CLOCK.
 */
#define SYS_CLK_RATE_MIN  19            /* minimum system clock rate */
#define SYS_CLK_RATE_MAX  (PIT_CLOCK/256) /* maximum system clock rate */
#define AUX_CLK_RATE_MIN  2             /* minimum auxiliary clock rate */
#define AUX_CLK_RATE_MAX  8192          /* maximum auxiliary clock rate */


/* pc console definitions  */

#if 1
#define INCLUDE_PC_CONSOLE 		/*  KBD and VGA are included */
#endif

#ifdef INCLUDE_PC_CONSOLE

#define	PC_CONSOLE		0	/* console number */
#define	N_VIRTUAL_CONSOLES	2	/* shell / application */

#endif /* INCLUDE_PC_CONSOLE */

#undef	NUM_TTY
#define NUM_TTY			(N_UART_CHANNELS)

/* define a type of keyboard. The default is 101 KEY for PS/2 */

#define PC_KBD_TYPE		PC_PS2_101_KBD
/* #define PC_KBD_TYPE		PC_XT_83_KBD */


/* memory addresses */

/*
 * Local-to-Bus memory address constants:
 * the local memory address always appears at 0 locally;
 * it is not dual ported.
 */

#define LOCAL_MEM_LOCAL_ADRS	0x00000000	/* fixed */
#define LOCAL_MEM_BUS_ADRS	0x00000000	/* fixed */
#define LOCAL_MEM_SIZE		0x00400000	/* 4MB */

#define ROM_TEXT_ADRS		(ROM_BASE_ADRS)

#endif	/* INCconfigh */
