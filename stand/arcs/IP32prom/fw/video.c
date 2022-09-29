/*****************************************************************************
 * $Id: video.c,v 1.2 1997/08/18 20:39:14 philw Exp $
 *
 * Copyright 1995, Silicon Graphics, Inc.
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
 *
 *****************************************************************************/

/*
 * MVP - Multiport Video Processor
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/pbus.h>
#include <sys/errno.h>
#include <arcs/hinv.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>
#include <sys/IP32.h> 
#include "tiles.h"

#undef	DEBUG
#define	MVP_PROM
#include <sys/mvpregs.h>

/* -------------------------------------------------------------------- */
/* these are bits defined in the input and output config regs */
#define	ICR_VIN_SOURCE_AB		0x00
#define	ICR_VIN_SOURCE_CD		0x04
#define	ICR_VIN_SOURCE_LPBK_E		0x08
#define	ICR_VIN_SOURCE_SVIDEO		0x01

#define	OCR_SYNC_SOURCE_INTERNAL	0x00
#define OCR_SYNC_SOURCE_EXT		0x40
#define	OCR_SYNC_SOURCE_AB		0x80
#define	OCR_SYNC_SOURCE_CD		0xc0

#define	OCR_VOUT_SOURCE_PASSTHRU	0x0a
#define	OCR_VOUT_SOURCE_YUV		0x14
#define	OCR_VOUT_COLORBARS		0x800

/* these should stay the same since they are used to index the i2c data table */
typedef	enum mvp_mode_e {
    MVP_PROM_MODE_NTSC = 0,
    MVP_PROM_MODE_PAL = 1
} mvp_mode_t;

/* these are stolen from mvp/kern/mvp/mvpi2cregs.h - but they shouldn't change */
#define MVP_I2C_7185_REG_SC	0x61
#define MVP_I2C_7111_REG_CC	0x0e
#define MVP_I2C_7111_REG_AIC1	0x02
#define MVP_I2C_7111_REG_LCR	0x09
#define MVP_I2C_7185_REG_IPC	0x3a

typedef	unsigned char i2c_chip_adrs_t;
typedef	unsigned char i2c_chip_regadrs_t;
typedef	unsigned char i2c_chip_regdata_t;

/* environment variables */
static	const char *videostatus  = "videostatus";
static	const char *videotiming  = "videotiming";
static	const char *videoinput   = "videoinput";
static	const char *videooutput  = "videooutput";
static	const char *videogenlock = "videogenlock";

/* error returns (via "videostatus") */
static	const char *illegal_env_var = "illegal_env_var";
static	const char *av_error	 = "av_error";
static	const char *initialized	 = "inited";

/* var debug print format */
static	const char *varfmt	 = "%s: %s\n";

#if	defined(DEBUG)
static	int mvp_debug = 0;
#define	DebugPrint	if( mvp_debug )printf
#define	RegPrint	if( mvp_debug > 1 )printf

#else
static	int mvp_debug = 0;
#define	DebugPrint	0 &&
#define	RegPrint	0 &&
#endif

/* -------------------------------------------------------------------- */
/* prom environment variable or boot command option "videotiming" */
static mvp_mode_t
mvp_preset_timing( void )
{
    char *c;

    if( c = getenv( videotiming ) ) {
	DebugPrint( varfmt, videotiming, c );
	if( strncasecmp( c, "ntsc", 4 ) == 0 ) {
	    return MVP_PROM_MODE_NTSC;
	}
	else if( strncasecmp( c, "pal", 3 ) == 0 ) {
	    return MVP_PROM_MODE_PAL;
	}
    }
    return (mvp_mode_t)-1;
}

/* -------------------------------------------------------------------- */
/* prom environment variable or boot command option "videoinput" */
static int
mvp_preset_input( void )
{
    char *c;

    if( c = getenv( videoinput ) ) {
	DebugPrint( varfmt, videoinput, c );
	if( strcasecmp( c, "composite" ) == 0 ) {
	    return ICR_VIN_SOURCE_AB;
	}
	else if( strcasecmp( c, "svideo" ) == 0 ) {
	    return ICR_VIN_SOURCE_AB | ICR_VIN_SOURCE_SVIDEO; 
	}
	else if( strcasecmp( c, "digital" ) == 0 ) {
	    return ICR_VIN_SOURCE_CD;
	}
	else if( strcasecmp( c, "loopback" ) == 0 ) {
	    return ICR_VIN_SOURCE_LPBK_E;
	}
    }
    return -1;
}

/* -------------------------------------------------------------------- */
/* prom environment variable or boot command option "videooutput" */
static int
mvp_preset_output( void )
{
    char *c;

    if( c = getenv( videooutput ) ) {
	DebugPrint( varfmt, videooutput, c );
	if( strcasecmp( c, "image" ) == 0 ) {
	    return OCR_VOUT_SOURCE_PASSTHRU;
	}
	else if( strcasecmp( c, "black" ) == 0 ) {
	    return OCR_VOUT_SOURCE_YUV;
	}
	else if( strcasecmp( c, "colorbars" ) == 0 ) {
	    return OCR_VOUT_SOURCE_YUV | OCR_VOUT_COLORBARS;
	}
    }
    return -1;
}

/* -------------------------------------------------------------------- */
/* prom environment variable or boot command option "videogenlock" */
static int
mvp_preset_genlock( void )
{
    char *c;

    if( c = getenv( videogenlock ) ) {
	DebugPrint( varfmt, videogenlock, c );
	if( strcasecmp( c, "internal" ) == 0 ) {
	    return OCR_SYNC_SOURCE_INTERNAL;
	}
	else if( strcasecmp( c, "external" ) == 0 ) {
	    return OCR_SYNC_SOURCE_EXT;
	}
	else if( strcasecmp( c, "analog" ) == 0 ) {
	    return OCR_SYNC_SOURCE_AB;
	}
	else if( strcasecmp( c, "digital" ) == 0 ) {
	    return OCR_SYNC_SOURCE_CD;
	}
    }
    return -1;
}


/* -------------------------------------------------------------------- */
#ifndef _SYS_CRIME_H__
reg_t	read_reg64( reg_t *adrs );
void	write_reg64( reg_t value, reg_t *adrs );
#endif

#if 1	/* for debugging */
static reg_t
mvp_prom_read_reg( reg_t *reg, char *name )
{
    reg_t value = (reg_t)read_reg64( (__psunsigned_t)reg );

    if( reg != (reg_t *)0xbf330010 ) {
	RegPrint( "read  %s (0x%x) = 0x%llx\n", name, reg, value );
    }
    return value;
}

static void
mvp_prom_write_reg( reg_t *reg, reg_t value, char *name )
{
    if( reg != (reg_t *)0xbf330010 ) {
	RegPrint( "write %s (0x%x) = 0x%llx\n", name, reg, value );
    }
    write_reg64( (long long)value, (__psunsigned_t)reg );
}

#else

#define	mvp_prom_read_reg(a,r)	  read_reg64( (__psunsigned_t)a )
#define	mvp_prom_write_reg(a,d,r) write_reg64( (long long)d, (__psunsigned_t)a )
#endif

/* -------------------------------------------------------------------- */
/* 
 * Use macros to fill the tables so we can reuse them for different
 * purposes.  For example, we remove "names" from the kernel to
 * conserve memory, while keeping them for the standalone diag.
 *
 * Also, we use "nickname" to define a MACRO name for reference.
 * (that is if it's not "RES" or reserved.)
 */
#undef	RT
#undef	CT
#undef	BT

/* (chip) register table */
#define	RT(n,nn,a,ivn,ivp,m,f)		a,ivn,ivp,f

/*
 * description of each register inside the i2c chip
 * (see "T" macro above)
 */
typedef struct i2c_chipreg_s {
    i2c_chip_regadrs_t	address;		/* register address */
    i2c_chip_regdata_t	initial_value[2];	/* init value (ntsc/pal) */
    char		flags;			/* I2C_FLAGS_xxx */
} i2c_chipreg_t;

/* chip table */
#define	CT(n,nn,num,a,mr,mxr,pr,f,rp)	num,a,mr,mxr,pr,f,rp

/*
 * description of each i2c chip
 */
typedef struct i2c_chip_s {
    short		number;			/* part number */
    i2c_chip_adrs_t	address;		/* chip address */
    i2c_chip_regadrs_t	min_reg;		/* lowest register address */
    i2c_chip_regadrs_t	max_reg;		/* highest register address */
    short		probe_reg;		/* probe register address */
    short		flags;			/* chip flags */
    i2c_chipreg_t	*regs;			/* pointer to the reg desc's */
} i2c_chip_t;

#define	I2C_CONFIG_RESET	1
#define	I2C_CONFIG_FASTMODE	2
#define	I2C_CONFIG_DATA		4
#define	I2C_CONFIG_CLOCK	8

#define	I2C_STATUS_REG		(i2c_chipreg_t *)-2
#define	I2C_STATUS_REGNUM	-2

/* i2c_chipreg_t.flags */
/* the flags may look strange but it's so the default is "write only" */
#define I2C_FLAGS_NOT_USED	1	/* unused register */
#define I2C_FLAGS_CAN_READ	2	/* read enabled register */
#define I2C_FLAGS_CANT_WRITE	4	/* write disabled register */
#define I2C_FLAGS_NO_INITIALIZE	8	/* don't initialize register */
#define I2C_FLAGS_OR_INITVAL	0x10	/* or in init value to preserve bits */
#define I2C_FLAGS_READ_ONLY	( I2C_FLAGS_CAN_READ | I2C_FLAGS_CANT_WRITE )
#define I2C_FLAGS_WRITE_ONLY	0
#define I2C_FLAGS_CAN_WRITE	0

/* i2c_chip_t.flags */
#define I2C_FLAGS_AUTOINCREMENT		1	/* chip has auto-increment */
#define I2C_FLAGS_USE_SUBADRS		2	/* chip read uses subadrs */
#define I2C_FLAGS_PALMODE		4	/* chip has seperate pal mode */
#define I2C_FLAGS_FAST_MODE		8	/* can use i2c fast mode */
#define I2C_FLAGS_SECONDARY_PART	0x10	/* chip has secondary address */
#define I2C_FLAGS_IS_CAMERA		0x40	/* chip is on the camera */
#define I2C_FLAGS_CHIP_PRESENT		0x80	/* chip is present */

/*
 * Interface module for A/V Board 1
 *
 * Contains:
 *      7185 analog encoder 
 *      7111 analog decoder
 */

#define	RO	I2C_FLAGS_READ_ONLY
#define	WO	I2C_FLAGS_WRITE_ONLY
#define	NU	I2C_FLAGS_NOT_USED
#define RW      ( I2C_FLAGS_CAN_READ | I2C_FLAGS_CAN_WRITE )
#define NI      I2C_FLAGS_NO_INITIALIZE
#define CHIPNAME(a)

#include "mvp7111.h"
#include "mvp7185.h"

/* -------------------------------------------------------------------- */
/* i2c stuff */
#define	I2C_RETURN_ERROR(ret,erm)	{ DebugPrint ( erm ); return -1; }

/* Empirically derived i2c bus timeout constants */
#define ACK_WAIT_TIMEOUT	200
#define XFER_WAIT_TIMEOUT	200
#define IDLE_WAIT_TIMEOUT	200

/* global constants (were variables) */
#define mvpi2c_retrycount 5
#define mvpi2c_ackwait 1
#define mvpi2c_xfer_done_delay 20
#define mvpi2c_ack_wait_delay 20
#define mvpi2c_force_idle_delay 50
#define sync_address (int *)0xbffffff0
#define sync_data 0xbabecafe

/* -------------------------------------------------------------------- */
/* XXX - short delay */
static int mvp_dummy( volatile int i ) { return i + 1; }
static int _mvp_delay_factor = 2;
#if 0
static void
mvpi2c_delay( volatile int count )
{
     volatile int i;

     while( count-- > 0 ) {
 	for( i = _mvp_delay_factor; i > 0; i-- ) {
 	    mvp_dummy( i );
	}
     }
}
#else
#define mvpi2c_delay(c)	{ int i = c; while( i-- > 0 ) ; }
#endif


/* -------------------------------------------------------------------- */
/*  _mvpi2c_wait_for_xfer_done
 *
 *  Waits for the mvp asic to indicate that a data byte read transfer
 *  from a slave device to the mvp asic master chip has completed.
 *  An error is returned if the read transfer exceeds a specified time.
 *
 *  EAGAIN is returned if an error occured, but the i2c bus was reset.
 *  This indicates that the caller may attempt retry if desired.
 */
static int
_mvpi2c_wait_for_xfer_done( i2c_chip_t *chip )
{
    i2c_control_t xfer;
    int wait;
    i2c_regs_t *regs = MVP_I2C_BASE_ADDRESS;

    for( wait = XFER_WAIT_TIMEOUT; wait > 0; wait-- ) {
        xfer.word = MVP_I2CREG_READ( regs, i2c_control );
        if ( xfer.bits.transfer_status == 0 ) {
	    return (0);
	}
	mvpi2c_delay( mvpi2c_xfer_done_delay );
    }
    DebugPrint( "no xfer done\n" );
    return EIO;
}

/* -------------------------------------------------------------------- */
/*  _mvpi2c_wait_for_ack
 *
 *  Waits for the slave chip on the i2c bus to acknowledge a data byte
 *  transfer from the mvp asic which is the i2c bus master.  An error
 *  is returned if the ack is not received in a specified time.
 *
 *  EAGAIN is returned if an error occured, but the i2c bus was reset.
 *  This indicates that the caller may attempt retry if desired.
 */
static int
_mvpi2c_wait_for_ack( i2c_chip_t *chip, int wait_for_xfer_done )
{
    i2c_control_t ack;
    int wait, ret;
    i2c_regs_t *regs = MVP_I2C_BASE_ADDRESS;

    ack.word = MVP_I2CREG_READ( regs, i2c_control );
    if ( wait_for_xfer_done && ack.bits.transfer_status ) {
	if( ret = _mvpi2c_wait_for_xfer_done( chip ) ) {
	    I2C_RETURN_ERROR( ret, "A1" );
	    return ret;
	}
	ack.word = MVP_I2CREG_READ( regs, i2c_control );
    }

    if ( mvpi2c_ackwait && ack.bits.acknowledge_status ) {
	for( wait = ACK_WAIT_TIMEOUT; wait; --wait ) {
	    mvpi2c_delay( mvpi2c_ack_wait_delay );
	    ack.word = MVP_I2CREG_READ( regs, i2c_control );
	    if ( ack.bits.acknowledge_status == 0 ) {
		return 0;
	    }
	}
	DebugPrint( "no ack\n" );
	return EIO;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*  _mvpi2c_force_bus_idle
 *
 *  If the i2c bus is not idle then force it to an idle state.  Then
 *  loop waiting for the control/status reg to indicate that the i2c bus
 *  has become idle.  If the bus does not become idle within a timeout 
 *  period or if a bus error occurs then an error value is returned.
 *
 *  EAGAIN is returned if an error occured, but the i2c bus was reset.
 *  This indicates that the caller may attempt retry if desired.
 *
 * late changes: always force bus idle.
 * optimization: always save last status from i2c bus and use it here
 *               to determine if bus was idle from previous status read.
 */
static int
_mvpi2c_force_bus_idle( i2c_chip_t *chip, int mode )
{
    i2c_control_t idle;
    int wait;
    i2c_regs_t *regs = MVP_I2C_BASE_ADDRESS;

    /* if i2c bus is idle, then return */
    idle.word = MVP_I2CREG_READ( regs, i2c_control );
    if ( I2C_IS_IDLE( idle.word ) ) {
	MVP_I2CREG_WRITE( regs, i2c_control, mode );
	return 0;
    }

    /* else force it idle */
    MVP_I2CREG_WRITE( regs, i2c_control,
	( mode & ~I2C_IDLE_BIT ) | I2C_FORCE_IDLE );

    mvpi2c_delay( mvpi2c_force_idle_delay ); 

    /* and wait until it is idle, or a timeout occurs. */
    for( wait = IDLE_WAIT_TIMEOUT; wait; --wait ) {
	mvpi2c_delay( mvpi2c_force_idle_delay ); 
	idle.word = MVP_I2CREG_READ( regs, i2c_control);
	if ( I2C_IS_IDLE( idle.word ) )
	    break;
    }

    /* did a bus idle timeout occur, if so then report an error */
    if( !wait ) {
	DebugPrint( "no bus idle\n" );
	return EIO;
    }

    /* report an error if an i2c bus error occurred */
    idle.word = MVP_I2CREG_READ( regs, i2c_control );
    if( idle.bits.bus_error_status == 1 ) {
	DebugPrint( "bus error\n" );
	return EIO;
    }

    /* everything is ok and the bus is idle */
    return 0;
}

/* -------------------------------------------------------------------- */
/*  mvpi2c_read_reg
 *
 *  Read the specified register in the specified i2c chip.
 *  If any i2c bus handshake errors occur set "mvpi2c_error to error value"
 *  and return -1;
 */

static int
mvpi2c_read_reg( i2c_chip_t *chip, i2c_chipreg_t *reg )
{
    int reg_data;
    int ret;
    i2c_regs_t *regs = MVP_I2C_BASE_ADDRESS;

    RegPrint( "read i2c chip 0x%x ", chip->address );

    /* if reading the status reg then always read using no subaddressing */
    if( reg == I2C_STATUS_REG ) {
	goto read_status;
    }

    RegPrint( "reg 0x%x\n", reg->address );

    /* If the register address is needed, then send it */
    if( chip->flags & I2C_FLAGS_USE_SUBADRS ) {

	/* first, force bus idle if need be */
	if( ret = _mvpi2c_force_bus_idle( chip, 
	    I2C_HOLD_BUS | I2C_WRITE | I2C_NOT_IDLE ) ) {
	    I2C_RETURN_ERROR( ret, "E1" );
	}

	/* write out the i2c chip's WRITE address to the bus */
	MVP_I2CREG_WRITE( regs, i2c_data, chip->address | I2C_WRITE_ADRS );
	if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
	    I2C_RETURN_ERROR( ret, "E2" );
	}

	/* send "stop" after write */
	MVP_I2CREG_WRITE( regs, i2c_control,
	    I2C_RELEASE_BUS | I2C_WRITE | I2C_NOT_IDLE );

	/* write the register subaddress to the device */
	MVP_I2CREG_WRITE( regs, i2c_data, reg->address );
	if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
	    I2C_RETURN_ERROR( ret, "E3" );
	}

	if( ret = _mvpi2c_force_bus_idle( chip, 
	    I2C_HOLD_BUS | I2C_WRITE | I2C_NOT_IDLE ) ) {
	    I2C_RETURN_ERROR( ret, "E4" );
	}

	/* Write out the i2c chip's READ address */
	MVP_I2CREG_WRITE( regs, i2c_data, chip->address | I2C_READ_ADRS );
	if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
	    I2C_RETURN_ERROR( ret, "E5" );
	}

	/* Tell mvp to do an i2c bus READ operation and wait for the data */
	MVP_I2CREG_WRITE( regs, i2c_control,
	    I2C_RELEASE_BUS | I2C_READ | I2C_NOT_IDLE );	

	if( ret = _mvpi2c_wait_for_xfer_done( chip ) ) {
	    return ret;
	}
    }
    else {

    read_status:

	RegPrint( "reg %d\n", I2C_STATUS_REG );

	/* set control reg and force bus idle if need be */
	if( ret = _mvpi2c_force_bus_idle( chip, 
	    I2C_WRITE | I2C_HOLD_BUS | I2C_NOT_IDLE ) ){
	    I2C_RETURN_ERROR( ret, "E6" );
	}

	/* Write out the i2c chip address */
	MVP_I2CREG_WRITE( regs, i2c_data, chip->address | I2C_READ_ADRS );
	if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
	    I2C_RETURN_ERROR( ret, "E7" );
	}

	/* Tell MACE to do an i2c bus READ operation and wait for the data */
	MVP_I2CREG_WRITE( regs, i2c_control,
	    I2C_RELEASE_BUS | I2C_READ | I2C_NOT_IDLE );	

	if( ret = _mvpi2c_wait_for_xfer_done( chip ) ) {
	    return ret;
	}
    }

    /* return the value read from the i2c register */
    reg_data = MVP_I2CREG_READ( regs, i2c_data );

    MVP_I2CREG_WRITE( regs, i2c_control,
	I2C_RELEASE_BUS | I2C_FORCE_IDLE | I2C_WRITE );

    return reg_data;
}

/* -------------------------------------------------------------------- */
/*  _mvpi2c_write_adrs
 *
 *  Write out the i2c chip address in write mode and the reg subaddress.
 *  If any i2c bus handshake errors are detected then report an error.
 */
static int
_mvpi2c_write_adrs( i2c_chip_t *chip, int reg_adrs )
{
    int ret;
    int retry = mvpi2c_retrycount;
    i2c_regs_t *regs = MVP_I2C_BASE_ADDRESS;

    while( 1 ) {
	if( ret = _mvpi2c_force_bus_idle( chip,
	    I2C_HOLD_BUS | I2C_WRITE ) ) {
	    if( ret == EAGAIN && retry-- > 0 )
		continue;

	    return ret;
	}

	MVP_I2CREG_WRITE( regs, i2c_data, chip->address | I2C_WRITE_ADRS );
	if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
	    if( ret == EAGAIN && retry-- > 0 )
		continue;

	    return ret;
	}

	/* write the register subaddress */
	MVP_I2CREG_WRITE( regs, i2c_data, reg_adrs );		
	if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
	    if( ret == EAGAIN && retry-- > 0 )
		continue;

	    return ret;
	}
	break;
    }

    return 0;
}

/* -------------------------------------------------------------------- */
/*
 *  mvpi2c_write_reg
 *
 *  Write out the specified value to the register in the specified
 *  i2c chip.  If any i2c bus handshake errors occur then report an
 *  error.
 */
static int
mvpi2c_write_reg( i2c_chip_t *chip, i2c_chipreg_t *reg,
    i2c_chip_regdata_t reg_data )
{
    int ret;
    int retry = mvpi2c_retrycount;
    i2c_regs_t *regs = MVP_I2C_BASE_ADDRESS;

    /* write the chip address and register subaddress */
    while( 1 ) {
	if( ret = _mvpi2c_write_adrs( chip, reg->address ) ) {
	    if( ret == EAGAIN && retry-- > 0 )
		continue;
	    return ret;
	}

	/* now write the register data and wait for ack */
	MVP_I2CREG_WRITE( regs, i2c_control,
	    I2C_RELEASE_BUS | I2C_WRITE | I2C_NOT_IDLE );


	MVP_I2CREG_WRITE( regs, i2c_data, reg_data );		
	if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
	    if( ret == EAGAIN && retry-- > 0 )
		continue;
	    return ret;
	}
	break;
    }
    return 0;
}

/* -------------------------------------------------------------------- */
/*
 * mvpi2c_init_regs
 *
 * Initialize the registers of an i2c chip on the A/V board.
 */
static int
mvpi2c_init_regs( i2c_chip_t *chip, mvp_mode_t timing )
{
    i2c_regs_t *regs = MVP_I2C_BASE_ADDRESS;
    i2c_chipreg_t *reg;
    i2c_chip_regadrs_t reg_adrs;
    short adrs_needed = 1;
    short last_byte = 1;
    short autoincr;
    int ret;

    autoincr = chip->flags & I2C_FLAGS_AUTOINCREMENT;

    for( reg_adrs = chip->min_reg, reg = chip->regs;
	 reg_adrs <= chip->max_reg;
	 reg_adrs++, reg++ ) {


	/* check for unused or not writable registers */
	if( reg->flags & (
	    I2C_FLAGS_NOT_USED |
	    I2C_FLAGS_CANT_WRITE |
	    I2C_FLAGS_NO_INITIALIZE ) ) {
	    adrs_needed = 1;

	    if( !last_byte ) {	/* still have the bus */
		MVP_I2CREG_WRITE( regs, i2c_control,
		    I2C_RELEASE_BUS | I2C_WRITE | I2C_NOT_IDLE );

		last_byte = 1;
	    }
	    continue;	/* skip this reg */
	}

	RegPrint( "%d 0x%x = 0x%x\n",
	    chip->address, reg->address, reg->initial_value[ timing ] );

	/* if this chip supports autoincrement */
	if( autoincr ) {

	    /* write chip address */
	    if( adrs_needed ) {
		if( ret = _mvpi2c_write_adrs( chip, reg_adrs ) ) {
		    return ret;
		}
		adrs_needed = 0;
		last_byte = 0;
	    }

	    /* Determine if the next data will be the last, and if so,
	     * indicate such to the I2c controller with "release bus" (or
	     * "last byte").
	     */
	    if( ( reg_adrs == chip->max_reg ) || 
		( reg[1].flags & (
		    I2C_FLAGS_NOT_USED |
		    I2C_FLAGS_CANT_WRITE |
		    I2C_FLAGS_NO_INITIALIZE ) ) ) {

		MVP_I2CREG_WRITE( regs, i2c_control,
		    I2C_RELEASE_BUS | I2C_WRITE | I2C_NOT_IDLE );

		last_byte = 1;
	    }

	    /* write the data value to the register in the i2c chip */
	    MVP_I2CREG_WRITE( regs, i2c_data,
		reg->initial_value[ timing ]);

	    /* wait for ack */
	    if( ret = _mvpi2c_wait_for_ack( chip, 1 ) ) {
		return ret;
	    }
	}
	else { /* !autoincrement */
	    if( ret = mvpi2c_write_reg( chip, reg,
		reg->initial_value[ timing ] ) ){
		    return ret;
	    }
	}
    }

    if( !last_byte ) {
	MVP_I2CREG_WRITE( regs, i2c_control,
	    I2C_RELEASE_BUS | I2C_WRITE | I2C_NOT_IDLE );

	last_byte = 1;
    }

    return 0;
}

/* -------------------------------------------------------------------- */
/* mvp_i2c_reg - look up register by hex address */
static i2c_chipreg_t *
mvp_i2c_reg( i2c_chip_t *chip, int regnum )
{
    int num;
    i2c_chip_regadrs_t ra;
    i2c_chipreg_t *reg;
    char *s;

    if( regnum == chip->regs[ ra - chip->min_reg ].address ) {
	return &chip->regs[ ra - chip->min_reg ];
    }

    for( ra = chip->min_reg, reg = chip->regs;
	 ra <= chip->max_reg; ra++, reg++) {

	if( regnum == reg->address ) {
	    return reg;
	}
    }
    return 0;
}

/* -------------------------------------------------------------------- */
static int
mvp_probe( i2c_chip_t *chip )
{
    i2c_chipreg_t *reg;

    if( chip->probe_reg == I2C_STATUS_REGNUM ) {
	return( mvpi2c_read_reg( chip, I2C_STATUS_REG ) != -1 );
    }
    reg = mvp_i2c_reg( chip, chip->probe_reg );
    return( mvpi2c_read_reg( chip, reg ) != -1 );
}

/* -------------------------------------------------------------------- */
static void
mvp_select_genlock( int locked, mvp_mode_t timing )
{
    i2c_chipreg_t *reg7111, *reg7185;
    int val7111, val7185;

    reg7185 = mvp_i2c_reg( &chip_7185, MVP_I2C_7185_REG_SC );
    reg7111 = mvp_i2c_reg( &chip_7111, MVP_I2C_7111_REG_CC );

    val7185 = reg7185->initial_value[ timing ];
    val7111 = reg7111->initial_value[ timing ];

    if( locked ) {

	/* enable subcarrier */
	val7185 |= 0x08;
	mvpi2c_write_reg( &chip_7185, reg7185, val7185 );

	val7111 |= 0x80;
	mvpi2c_write_reg( &chip_7111, reg7111, val7111 );

	val7111 &= ~0x80;
	mvpi2c_write_reg( &chip_7111, reg7111, val7111 );
    }
    else {

	/* disable subcarrier */
	val7185 &= ~0x08;
	mvpi2c_write_reg( &chip_7185, reg7185, val7185 );
    }
}

/* -------------------------------------------------------------------- */
static reg_t mvpreg_init_ntsc_in[] = {
    0x2d0d5c88LL,	/* hclip */
    0x10304411LL,	/* vclip odd */
    0x10204010LL,	/* vclip even */
    0xb40LL,		/* line width */
    0xffLL,		/* alpha */
    0x0,		/* field offset */
    0x000100e3LL,	/* iconfig */
};

static reg_t mvpreg_init_pal_in[] = {
    0x2d0d748eLL,	/* hclip */
    0x13505816LL,	/* vclip odd */
    0x13605c17LL,	/* vclip even */
    0xb40LL,		/* line width */
    0xffLL,		/* alpha */
    0x0,		/* field offset */
    0x000100a3LL,	/* iconfig */
};
#define	ICONFIG	6

static reg_t mvpreg_init_ntsc_out[] = {
    0xb40446b043LL, 	/* hpad */
    0x410540c10LL, 	/* vpad odd */
    0x410640c10LL,	/* vpad even */
    0xf3b40LL,		/* field size */
    0x0,		/* field offset */
    0x00980e21LL, 	/* oconfig */
    0x1LL,		/* genlock delay */
    0x800005LL,		/* vhw_cfg */
};
    
static reg_t mvpreg_init_pal_out[] = {
    0xb40476bc46LL, 	/* hpad */
    0x55374d415LL, 	/* vpad odd */
    0x59384d816LL,	/* vpad even */
    0x120b40LL,		/* field size */
    0x0,		/* field offset */
    0x00980a21LL, 	/* oconfig */
    0x1LL,		/* genlock delay */
    0x800005LL,		/* vhw_cfg */
};

#define	OCONFIG	5	/* offset to "oconfig" reg in above table */
#define	VHW_CFG	7	/* offset to "vhw_cfg" reg in above table */

/* -------------------------------------------------------------------- */
static void
mvp_initmace( int input, mvp_input_channel_regs_t *regs, reg_t *r )
{
    int chan;

    if( input ) {
	MVP_REG_WRITE( regs, control,  0 );
	MVP_REG_WRITE( regs, iconfig,  0 );
	MVP_REG_WRITE( regs, h_clip_odd,  *r );
	MVP_REG_WRITE( regs, h_clip_even, *r++ );
	MVP_REG_WRITE( regs, v_clip_odd,  *r++ );
	MVP_REG_WRITE( regs, v_clip_even, *r++ );
	MVP_REG_WRITE( regs, line_width,  *r++ );
	MVP_REG_WRITE( regs, alpha_odd,   *r   );
	MVP_REG_WRITE( regs, alpha_even,  *r++ );
	MVP_REG_WRITE( regs, field_offset,*r++ );
	MVP_REG_WRITE( regs, iconfig,     *r++ );
    }
    else {

#define	oregs ((mvp_output_channel_regs_t *)regs)

	MVP_REG_WRITE( oregs, control,  0 );
	MVP_REG_WRITE( oregs, oconfig,  0 );
	MVP_REG_WRITE( oregs, h_pad_odd,     *r   );
	MVP_REG_WRITE( oregs, h_pad_even,    *r++ );
	MVP_REG_WRITE( oregs, v_pad_odd,     *r++ );
	MVP_REG_WRITE( oregs, v_pad_even,    *r++ );
	MVP_REG_WRITE( oregs, field_size,    *r++ );
	MVP_REG_WRITE( oregs, field_offset,  *r++ );
	MVP_REG_WRITE( oregs, oconfig,       *r++ );
	MVP_REG_WRITE( oregs, genlock_delay, *r++ );
	MVP_REG_WRITE( oregs, vhw_cfg,       *r++ );
    }
}

/* -------------------------------------------------------------------- */
void
initVideo( void )
{
    mvp_mode_t timing;
    int input, output, genlock;
    reg_t *ir, *or;
    char *c, *initvar;

#ifdef	DEBUG
    if( c = getenv( "verbose" ) ) {
	mvp_debug = atoi( c );
    }
#endif

    DebugPrint( "initvideo\n");
    initvar = getenv( videostatus );

    /* query for video environment variables */
    if( ( ( timing  = mvp_preset_timing( ) )  == -1 ) ||
	( ( input   = mvp_preset_input( ) )   == -1 ) ||
	( ( output  = mvp_preset_output( ) )  == -1 ) ||
	( ( genlock = mvp_preset_genlock( ) ) == -1 ) ) {

	/* indicate variables not set or not recognized */
	/* (only set if not already set - to cut down on flash writes) */
	if( ( initvar == 0 ) || ( strcmp( c , illegal_env_var ) != 0 ) ) {
	    _setenv( (char *)videostatus, (char *)illegal_env_var, 0);
	}
	return;
    }

    /* echo what values we found */
    DebugPrint( "timing %d, input 0x%x, output 0x%x, genlock 0x%x\n",
	timing, input, output, genlock );

    /* probe for i2c chips */
    DebugPrint( "probe chips\n" );
    if( ( mvp_probe( &chip_7185 ) == 0 ) ||
	( mvp_probe( &chip_7111 ) == 0 ) ) {

	/* (only set if not already set - to cut down on flash writes) */
	if( ( initvar == 0 ) || ( strcmp( c , av_error ) != 0 ) ) {
	    _setenv( (char *)videostatus, (char *)av_error, 0);
	}
	return;
    }

    /* pick up pointers to initial mace values */
    ir = timing ? mvpreg_init_pal_in  : mvpreg_init_ntsc_in;
    or = timing ? mvpreg_init_pal_out : mvpreg_init_ntsc_out;

    /* modify init data tables for timing, input, output, genlock */
    DebugPrint( "modify init data\n" );

    /* default analog input is composite, change for svideo */
    if( input & ICR_VIN_SOURCE_SVIDEO ) {
	i2c_chipreg_t *aic = mvp_i2c_reg( &chip_7111, MVP_I2C_7111_REG_AIC1 );
	i2c_chipreg_t *lcr = mvp_i2c_reg( &chip_7111, MVP_I2C_7111_REG_LCR );
	aic->initial_value[ timing ] |= 0x07;
	lcr->initial_value[ timing ] |= 0x80;
	input -= ICR_VIN_SOURCE_SVIDEO;
    }

    /* default analog output is mace, change for color bars */
    if( output & OCR_VOUT_COLORBARS ) {
	i2c_chipreg_t *reg = mvp_i2c_reg( &chip_7185, MVP_I2C_7185_REG_IPC );
	reg->initial_value[ timing ] = 0x80;
	output -= OCR_VOUT_COLORBARS;
    }

    /* select vin source in mace input config */
    {
	reg_t icr = ir[ ICONFIG ];
	icr |= input;
	ir[ ICONFIG ] = icr;
    }

    /* select vout source for mace, and source for analog encoder */
    {
	reg_t ocr = or[ OCONFIG ];
	ocr |= output | genlock;
	or[ OCONFIG ] = ocr;
    }

    /* modify vhw_cfg for things like port select and audio sync source */
    {
	reg_t r;
	if( c = getenv( "videovhw_cfg" ) ) {
	    (void)atob_L( c, (long long *)&r );
	    or[ VHW_CFG ] = r;
	}
    }

    /* setup mace video section */
    DebugPrint( "init mace\n" );
    mvp_initmace( 1, MVP_VIDEO_IN1_ADDRESS, ir );
    mvp_initmace( 1, MVP_VIDEO_IN2_ADDRESS, ir );
    mvp_initmace( 0, (mvp_input_channel_regs_t *)MVP_VIDEO_OUT_ADDRESS, or );

    /* initialize chips */
    DebugPrint( "init i2c\n" );
    mvpi2c_init_regs( &chip_7185, timing );
    mvpi2c_init_regs( &chip_7111, timing );

    /* one last hook - get subcarrier locked (or unlocked) */
    DebugPrint( "genlock fix\n" );
    mvp_select_genlock( genlock == OCR_SYNC_SOURCE_AB, timing );

    /* indicate video is initialized */
    /* (only set if not already set - to cut down on flash writes) */
    if( ( initvar == 0 ) || ( strcmp( c , initialized ) != 0 ) ) {
	_setenv( (char *)videostatus, (char *)initialized, 0);
    }

    /* and PAU HANA!! */
    DebugPrint( "pau hana!\n" );
}
/* === */
