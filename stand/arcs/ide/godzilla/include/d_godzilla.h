#ifndef __IDE_GODZILLA_H__
#define __IDE_GODZILLA_H__
/*
 * d_godzilla.h
 *
 * 	IDE godzilla header (common for godzilla chip set)
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "ide/godzilla/include/d_godzilla.h:  $Revision: 1.36 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

typedef struct _Test_Info {
        char *TestStr;
        char *ErrStr;
} _Test_Info;

/*
 * Godzilla Register Read-Write Modes 
 */
#define	GZ_READ_ONLY	0x0
#define	GZ_READ_WRITE	0x1
#define	GZ_WRITE_ONLY	0x2
#define	GZ_NO_ACCESS	0x3

/*
 * Modes of operation in modify register macros
 */
#define	CLR_MODE	0x0
#define	SET_MODE	0x1

#define D_EOLIST        -1

/*
 * FRU definitions: now all in d_frus.h
 */

/*
 * Multi-purpose masks
 */
#define	MASK_8		0x00000000000000ff      
#define	MASK_16		0x000000000000ffff      
#define	MASK_32		0x00000000ffffffff

/* 
 * flags
 */
/* for hb_chkout function */
#define	CHK_HEART	1
#define	DONT_CHK_HEART	0
#define	CHK_BRIDGE	1
#define	DONT_CHK_BRIDGE	0
#define	CHK_XBOW	1
#define	DONT_CHK_XBOW	0
/* for hb_reset function */
#define	RES_HEART	1
#define	DONT_RES_HEART	0
#define	RES_BRIDGE	1
#define	DONT_RES_BRIDGE	0
#define	RES_XBOW	1
#define	DONT_RES_XBOW	0
/* for hb_status function */
#define	HEART_STAT	1
#define	BRIDGE_STAT	1

/*
 * Variables used by all tests
 */
#if DEFINE_ERROR_DATA 	/* set in ide/godzilla/uif/status.c before include */
bool_t          d_exit_on_error = 0;
__uint64_t      d_errors = 0;
#else
extern	bool_t		d_exit_on_error;
extern	__uint64_t	d_errors;
#endif	/* DEFINE_ERROR_DATA */

/* functions */
extern int        report_pass_or_fail( _Test_Info *Test, __uint32_t errors);
extern __uint64_t pio_reg_mod_32(__uint32_t, __uint32_t, int);
extern __uint64_t pio_reg_mod_64(__uint32_t, __uint32_t, int);
extern __uint64_t br_reg_mod_64(__uint32_t, __uint32_t, int);
extern void	  _send_bootp_msg(char *msg);

/* NOTES: 								*/
/*	the registers can be either heart or bridge registers, 		*/
/*    	The address of a Bridge on a SN0 system may require the full 	*/
/*	64-bit address, so using the COMPAT space here is not desired 	*/
#if !defined(MFG_DBGPROM)

#define	PIO_REG_WR_64(address, mask, value) \
	*(volatile __uint64_t *)PHYS_TO_K1(address) = (heartreg_t) ((value) & (mask))

#define	PIO_REG_RD_64(address, mask, value) \
	value = *(volatile __uint64_t *)(PHYS_TO_K1(address)) & (mask)

#define	PIO_REG_WR_32(address, mask, value) \
	*(volatile __uint32_t *)PHYS_TO_K1(address) = ((value) & (mask))

#define	PIO_REG_RD_32(address, mask, value) \
	value = *(volatile __uint32_t *)(PHYS_TO_K1(address)) & (mask)

#define PIO_REG_WR_16(address, mask, value) \
	*(volatile unsigned short *)PHYS_TO_K1(address) = ((value) & (mask))

#define PIO_REG_WR_8(address, mask, value) \
	*(volatile unsigned char *)PHYS_TO_K1(address) = ((value) & (mask))

#define PIO_REG_RD_16(address, mask, value) \
	value = *(volatile unsigned short *)(PHYS_TO_K1(address)) & (mask)

#define PIO_REG_RD_8(address, mask, value) \
	value = *(volatile unsigned char *)(PHYS_TO_K1(address)) & (mask)

#else /* MFG_DBGPROM */

/*
 * The Mfg DBGPROM is very verbose and displays I/O reads and
 * writes.
 */

#define	PIO_REG_WR_64(address, mask, value) \
{ \
  msg_printf( SUM, "  WR 64  Address[0x%016llX] < ", \
      (unsigned long long)PHYS_TO_K1(address) ); \
	*(volatile __uint64_t *)PHYS_TO_K1(address) = (heartreg_t) ((value) & (mask)); \
  msg_printf( SUM, "0x%016llX\n", \
      (unsigned long long)((value) & (mask)) );\
}

#define	PIO_REG_RD_64(address, mask, value) \
{ \
  msg_printf( SUM, "  RD 64  Address[0x%016llX] > " , \
      (unsigned long long)PHYS_TO_K1(address) ); \
	value = *(volatile __uint64_t *)(PHYS_TO_K1(address)) & (mask); \
  msg_printf( SUM, "0x%016llX\n" , \
      (unsigned long long)((value) & (mask)) );\
}

#define	PIO_REG_WR_32(address, mask, value) \
{ \
  msg_printf( SUM, "  WR 32  Address[0x%016llX] < ", \
      (unsigned long long)PHYS_TO_K1(address) ); \
	*(volatile __uint32_t *)PHYS_TO_K1(address) = ((value) & (mask)); \
  msg_printf( SUM, "0x%08llX\n", \
      (unsigned long long)((value) & (mask)) );\
}

#define	PIO_REG_RD_32(address, mask, value) \
{ \
  msg_printf( SUM, "  RD 32  Address[0x%016llX] > " , \
      (unsigned long long)PHYS_TO_K1(address) ); \
	value = *(volatile __uint32_t *)(PHYS_TO_K1(address)) & (mask); \
  msg_printf( SUM, "0x%08llX\n" , \
      (unsigned long long)((value) & (mask)) );\
}

#define PIO_REG_WR_16(address, mask, value) \
{ \
  msg_printf( SUM, "  WR 16  Address[0x%016llX] < ", \
      (unsigned long long)PHYS_TO_K1(address) ); \
	*(volatile unsigned short *)PHYS_TO_K1(address) = ((value) & (mask)); \
  msg_printf( SUM, "0x%04llX\n", \
      (unsigned long long)((value) & (mask)) );\
}

#define PIO_REG_WR_8(address, mask, value) \
{ \
  msg_printf( SUM, "  WR  8  Address[0x%016llX] < ", \
      (unsigned long long)PHYS_TO_K1(address) ); \
	*(volatile unsigned char *)PHYS_TO_K1(address) = ((value) & (mask)); \
  msg_printf( SUM, "0x%02llX\n", \
      (unsigned long long)((value) & (mask)) );\
}

#define PIO_REG_RD_16(address, mask, value) \
{ \
  msg_printf( SUM, "  RD 16  Address[0x%016llX] > " , \
      (unsigned long long)PHYS_TO_K1(address) ); \
	value = *(volatile unsigned short *)(PHYS_TO_K1(address)) & (mask); \
  msg_printf( SUM, "0x%04llX\n" , \
      (unsigned long long)((value) & (mask)) );\
}

#define PIO_REG_RD_8(address, mask, value) \
{ \
  msg_printf( SUM, "  RD  8  Address[0x%016llX] > " , \
      (unsigned long long)PHYS_TO_K1(address) ); \
	value = *(volatile unsigned char *)(PHYS_TO_K1(address)) & (mask); \
  msg_printf( SUM, "0x%02llX\n" , \
      (unsigned long long)((value) & (mask)) );\
}
#endif /* !MFG_DBGPROM */

#define REPORT_PASS_OR_FAIL( Test, errors) return(report_pass_or_fail( (Test), (errors)) )


/* If you add/remove something from the table CPU_Test_err (util/cpu_globals.c) you
 * need to modify this to reflect changes.
 */
enum {
  CPU_ERR_BRIDGE_0000=0,
  CPU_ERR_BRIDGE_0001,
  CPU_ERR_BRIDGE_0002,
  CPU_ERR_BRIDGE_0003,
  CPU_ERR_BRIDGE_0004,
  CPU_ERR_BRIDGE_0005,

  CPU_ERR_ECC_0000,

  CPU_ERR_ETHERNET_0000,
  CPU_ERR_ETHERNET_0001,
  CPU_ERR_ETHERNET_0002,

  CPU_ERR_HXB_0000,
  CPU_ERR_HXB_0001,
  CPU_ERR_HXB_0002,

  CPU_ERR_HEART_0000,
  CPU_ERR_HEART_0001,
  CPU_ERR_HEART_0002,
  CPU_ERR_HEART_0003,
  CPU_ERR_HEART_0004,
  CPU_ERR_HEART_0005,
  CPU_ERR_HEART_0006,

  CPU_ERR_HAB_0000,
  CPU_ERR_HAB_0001,		/* obsolete */
  CPU_ERR_HAB_0002,		/* obsolete */
  CPU_ERR_HAB_0003,
  
  CPU_ERR_IOC3_0000,
  CPU_ERR_IOC3_0001,
  CPU_ERR_IOC3_0002,

  CPU_ERR_RTC_0000,

  CPU_ERR_MEM_0000,

  CPU_ERR_PCI_0000,
  CPU_ERR_PCI_0001,
  CPU_ERR_PCI_0002,
  CPU_ERR_PCI_0003,
  CPU_ERR_PCI_0004,
  CPU_ERR_PCI_0005,
  CPU_ERR_PCI_0006,
  CPU_ERR_PCI_0007,
  CPU_ERR_PCI_0008,

  CPU_ERR_RAD_0000,
  CPU_ERR_RAD_0001,
  CPU_ERR_RAD_0002,
  CPU_ERR_RAD_0003,

  CPU_ERR_SCSI_0000,
  CPU_ERR_SCSI_0001,

  CPU_ERR_HEART_XBOW_0000,

  CPU_ERR_XBOW_0000,
  CPU_ERR_XBOW_0001,

  CPU_END_OF_LIST
};


#endif /* C || C++ */

#endif	/* __IDE_GODZILLA_H__ */
