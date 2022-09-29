/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef _KTPORT_H_
#define _KTPORT_H_

#if defined(EVEREST)
#define HIP1
#undef HIP2
#else  /* sn0 system */
#define HIP2
#undef HIP1
#endif

#include <KONA/hip_host.h>
#include <KONA/hip_arm.h>
#include <boothdr.h>
#include <tport_hipcmds.h>
#include <tportcmds.h>

#if defined(HIP1)
	
#define KONA_MAXPIPES    3
#define KONA_TPORT_PIPE  0	/* Pipe# for textport */

#elif defined(HIP2)

#include <sys/SN/klconfig.h>

#define KONA_MAXPIPES   MAX_GRAPHICS_PIPES 	/* really 6 for ship, but 8 for HILOarray */ 
#define KONA_TPORT_PIPE		 0		/* Pipe# for textport */
#define KONA_HUB_MAXPIPES	 2		/* XXXhart suggested MAX Pipes per XBOW - SN0 */

#endif /* HIP1 or HIP2 */


extern int konadebug;
extern unsigned long *gfxvaddr;	/* Base address of the KONA pipe which */
				/* is used for textport operations */

#if defined(HIP1)

/*
 * Everest architecture with F3 chip, IO4 I/O bus
 */

#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/fchip.h>

#define hipreg_addr(addr) (((char *) gfxvaddr) /* base-address */ \
			   + ADDR_NOTF_MASK    /* not F data   */ \
			   + addr)             /* reg offset   */

#define hh_write(addr, data) *((volatile int *) hipreg_addr(addr)) = data
#define hh_read(addr)	     *((volatile int *) hipreg_addr(addr))
#define kfifo(data)	     hh_write(HH_GFIFO, data)


#define ha_write(addr, data)            \
{					\
    hh_write(HH_ARM_ADDRESS_REG, addr); \
    hh_write(HH_ARM_DATA_REG, data);    \
}

#define ha_read(addr, data) 		\
{					\
    hh_write(HH_ARM_ADDRESS_REG, addr); \
    data = hh_read(HH_ARM_DATA_REG);    \
}

#define verbose konadebug && ttyprintf /* debug routine */

/* Function prototypes */

extern int kona_alive(void *gfxbase, int delay);
extern int receive_armdata(int n, unsigned int *buf, int delay);
extern int kona_initpipe(void *hw);
extern void kona_debug_enable(void);

extern long sginap(long);	/* From unistd.h */

#elif defined(HIP2)

/*
 * HIP2 implies an XG chip and Xtalk I/O bus.
 */
#include <KONA/xg_regs.h>

/* ---------------------- begin new ----------------------------- */
#define PIO_READ_32(addr)               ((__uint32_t)*(volatile __uint32_t *)(addr))            /* 32-bit read of I/O address   */
#define PIO_WRITE_32(addr, data)        (*(volatile __uint32_t *)(addr) = (__uint32_t)(*data))   /* 32-bit write an I/O address  */
#define PIO_READ_64(addr)               ((__uint64_t)*(volatile __uint64_t *)(addr))            /* 64-bit read of I/O address   */
#define PIO_WRITE_64(addr, data)        (*(volatile __uint64_t *)(addr) = (__uint64_t)(*data))   /* 64-bit write an I/O address  */
/*
 * XG and Hip Registers
 */
#define XG_REG_ADDR(reg_offset) (((char *) gfxvaddr)    /* base-address */ \
			  	+ reg_offset) 	        /* reg offset */

#define HIPREG_ADDR(reg_offset) (((char *) gfxvaddr)    /* base-address */ \
			  	+ XG_CLIENT_REGS        /*  0x00800000  */ \
			  	+ reg_offset) 	        /* reg offset */

#define HIPREG_FC_ADDR(reg_offset) (((char *) gfxvaddr)  /* base-address */ \
			  	+ XG_FC_CLIENT_REGS      /*  0x00c00000  */ \
			  	+ reg_offset) 	         /* reg offset */

#define GFXFIFO_ADDR(reg_offset) (((char *) gfxvaddr)  /* base-address */ \
			  	+ XG_FC_GFX		/*  0x00c10000  */ \
			  	+ reg_offset) 	         /* reg offset */

#define HIPREG_READ(reg_offset)         (PIO_READ_32( HIPREG_ADDR(reg_offset)))

#define HIPREG_WRITE( reg_offset, val ) { \
        uint32_t value = val; \
        PIO_WRITE_32( HIPREG_ADDR( reg_offset ), &value ); \
        }

#define HIPREG_FC_WRITE( reg_offset, val ) { \
        uint32_t value = val; \
        PIO_WRITE_32( HIPREG_FC_ADDR( reg_offset ), &value ); \
        }

#define XG_REG_WRITE( reg_offset, val ) { \
        uint32_t value = val; \
        PIO_WRITE_32( XG_REG_ADDR( reg_offset ), &value ); \
        }

#define XG_REG_READ(reg_offset)         (PIO_READ_32( XG_REG_ADDR(reg_offset)))

#define xgreg_fcaddr(addr) 	HIPREG_FC_ADDR(addr) 	/*    gfx_flow control */
#define xgreg_addr(addr) 	HIPREG_ADDR(addr) 	/* no gfx_flow control - used by prom */

/* ---------------------- begin old ----------------------------- */

#define hh_write(addr, data) *((volatile __uint32_t *) xgreg_addr(addr)) = data
#define hh_read(addr)	     *((volatile __uint32_t *) xgreg_addr(addr))
#define kfifo(data)	     *((volatile __uint32_t *) XG_REG_ADDR(XG_FC_GFX)) = data

/* ----------------------  end  old ----------------------------- */


#define ha_write(addr, data)            \
{					\
    hh_write(HH_ARM_ADDRESS_REG, addr); \
    hh_write(HH_ARM_DATA_REG, data);    \
}

#define ha_read(addr, data) 		\
{					\
    hh_write(HH_ARM_ADDRESS_REG, addr); \
    data = hh_read(HH_ARM_DATA_REG);    \
}

/* Function prototypes */

extern int kona_alive(void *gfxbase, int delay);
extern int receive_armdata(int n, unsigned int *buf, int delay);
extern int kona_initpipe(void *hw);
extern void kona_debug_enable(void);

#if 0
extern long sginap(long);	/* From unistd.h */
#endif

extern void sginap(int ticks); /* From kona_init.c */

#endif /* HIP1 or HIP2 */


#ifdef SN0
#include <sys/SN/addrs.h>

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
typedef __uint64_t	sn0_hubreg_t;
typedef __uint32_t 	hip_reg_t;

typedef uint_t		bool_t;
#define FALSE 0
#define TRUE  1
#endif

/*
 * Force a flush out of the WG and optionally wait for
 * it to leave the WG (but not necessarily reach the
 * graphics device).
 * read the real-time counter to force flush
 */

 #define WG_FLUSH(wait)                          \
{                                               \
	volatile sn0_hubreg_t rt_reg;		\
	rt_reg = LOCAL_HUB_L( PI_RT_COUNT );	\
}
#define kwg_flush(wait)		WG_FLUSH(wait)
 
#ifdef SN0_KONA_DEBUG

#endif /* SN0_KONA_DEBUG */

#endif /* SN0*/
 

#endif /* _KTPORT_H */
