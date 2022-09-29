/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef _SFLASH_H
#define _SFLASH_H

#if LANGUAGE_C
#include <arcs/types.h>
#include <arcs/time.h>

typedef volatile uint8_t	vu_char;
typedef volatile uint16_t	vu_short;
typedef volatile uint32_t	vu_int;
typedef volatile uint64_t	vu_long;

/* 
 * prototypes for the functions used in ml/RACER/power.c for
 * softpower interaction with the sflash driver
 */
int srflash_softpower_startdown( void );
int srflash_softpower_check( void );
#endif /* LANGUAGE_C */

/* define ioctl commands to the sflash device driver */
#define	SFL_BASE		(('s'<<16) | ('f'<<8))
#define	SFL_INCGET_RPROM	(SFL_BASE | 1)
#define	SFL_INCGET_FPROM	(SFL_BASE | 2)
#define	SFL_INCGET_PDS		(SFL_BASE | 3)

/* SHARP/INTEL Flash compatibility mode commands */

#define SFLASH_CMD_READ			0xffff
#define SFLASH_CMD_ID_READ		0x9090
#define SFLASH_CMD_STATUS_READ		0x7070
#define SFLASH_CMD_STATUS_CLEAR		0x5050
#define SFLASH_CMD_WRITE		0x4040
#define SFLASH_CMD_ERASE		0x2020
#define SFLASH_CMD_ERASE_CONFIRM	0xd0d0

/* extended commands */
#define SFLASH_CMD_ESR_READ		0x7171

#define SFLASH_STATUS_READY		0x80

#define SFLASH_MFG_ID			0x00b0
#define SFLASH_DEV_ID			0x66a8

/* CSR compabitible status register bit fields */
#define SFLASH_CSR_WSMS			0x80	/* 1=ready 0=busy */
#define SFLASH_CSR_ESS			0x40	/* 1=suspended 0=busy/done */
#define SFLASH_CSR_ES			0x20	/* 1=err 0=ok */
#define SFLASH_CSR_DWS			0x10	/* 1=err 0=ok */
#define SFLASH_CSR_VPPS			0x08	/* 1=low/abort 0=ok */
#define SFLASH_CSR_RSV2			0x04	/* reserved */
#define SFLASH_CSR_RSV1			0x02	/* reserved */
#define SFLASH_CSR_RSV0			0x01	/* reserved */

/* GSR global status register bit fields */
#define SFLASH_GSR_WSMS			0x80	/* 1=ready 0=busy */
#define SFLASH_GSR_OSS			0x40	/* 1=suspended 0=busy/done */
#define SFLASH_GSR_DOS			0x20	/* 1=err 0=ok */
#define SFLASH_GSR_DSS			0x10	/* 1=insleep 0=notsleep */
#define SFLASH_GSR_QS			0x08	/* 1=qfull 0=qavail */
#define SFLASH_GSR_PBAS			0x04	/* 1=avail 0=notavail */
#define SFLASH_GSR_PBS			0x02	/* 1=ready 0=busy */
#define SFLASH_GSR_PBSS			0x01	/* 1=sel1  0=sel0 */

/* BSR block status register bit fields */
#define SFLASH_BSR_WSMS			0x80	/* 1=ready 0=busy */
#define SFLASH_BSR_BLS			0x40	/* 1=unlock 0=locked */
#define SFLASH_BSR_BOS			0x20	/* 1=err 0=ok */
#define SFLASH_BSR_BOAS			0x10	/* 1=abort 0=notabort */
#define SFLASH_BSR_QS			0x08	/* 1=qfull 0=qavail */
#define SFLASH_BSR_VPPS			0x04	/* 1=low/abort 0=ok */
#define SFLASH_BSR_RSV1			0x02	/* reserved */
#define SFLASH_BSR_RSV0			0x01	/* reserved */

#define SFLASH_MAX_SEGS			(16)
#define SFLASH_SEG_SIZE			(64*1024)
#define SFLASH_MAX_SIZE			(SFLASH_MAX_SEGS * SFLASH_SEG_SIZE)
#define SFLASH_BTOS(_b)			((_b)/SFLASH_SEG_SIZE)
#define SFLASH_RDUP(_x)			\
	((((_x) + SFLASH_SEG_SIZE - 1) / SFLASH_SEG_SIZE) * SFLASH_SEG_SIZE)

#ifndef IP30
#define FLASH_MEM_BASE		0x1fc00000
#define FLASH_MEM_ALT_BASE	0x1fe00000
#else
/* defined in ip30.h */
#endif

/*
 * Definitions for all flash memory access
 */

/*
 * Flash memory allocations and sizes
 */
#define SFLASH_RPROM_SEG	0
#define SFLASH_RPROM_NSEGS	3

#define SFLASH_FPROM_SEG	3
#define SFLASH_FPROM_NSEGS	12	

#define SFLASH_PDS_SEG		15
#define SFLASH_PDS_NSEGS	1

/*
 * address and offset definitions
 */

#if LANGUAGE_C
extern __psunsigned_t		flash_mem_base;
#endif /* LANGUAGE_C */
#define SFLASH_BASE_PADDR	flash_mem_base

#define SFLASH_SEG_OFF(n)	(SFLASH_SEG_SIZE * (n))
#define SFLASH_SEG_PADDR(n)	(SFLASH_BASE_PADDR + SFLASH_SEG_OFF(n))

#define SFLASH_RPROM_PADDR	(SFLASH_SEG_PADDR(SFLASH_RPROM_SEG))
#define SFLASH_FPROM_PADDR	(SFLASH_SEG_PADDR(SFLASH_FPROM_SEG))
#define SFLASH_PDS_PADDR	(SFLASH_SEG_PADDR(SFLASH_PDS_SEG))
#define SFLASH_PDS_END_PADDR	(SFLASH_SEG_PADDR(SFLASH_PDS_SEG + SFLASH_PDS_NSEGS))

#define SFLASH_RPROM_SIZE	(SFLASH_SEG_SIZE * SFLASH_RPROM_NSEGS)
#define SFLASH_FPROM_SIZE	(SFLASH_SEG_SIZE * SFLASH_FPROM_NSEGS)
#define SFLASH_PDS_SIZE		(SFLASH_SEG_SIZE * SFLASH_PDS_NSEGS)

#if LANGUAGE_C
#define SFLASH_BASE_ADDR	(vu_short *)PHYS_TO_K1(SFLASH_BASE_PADDR)
#define SFLASH_FCMD_ADDR	(vu_short *)PHYS_TO_K1(SFLASH_BASE_PADDR)
#define SFLASH_RPROM_ADDR	(vu_short *)PHYS_TO_K1(SFLASH_RPROM_PADDR)
#define SFLASH_FPROM_ADDR	(vu_short *)PHYS_TO_K1(SFLASH_FPROM_PADDR)
#define SFLASH_PDS_ADDR		(vu_short *)PHYS_TO_K1(SFLASH_PDS_PADDR)
#define SFLASH_PDS_END_ADDR	(vu_short *)PHYS_TO_K1(SFLASH_PDS_END_PADDR)
#define SFLASH_SEG_ADDR(n)	(vu_short *)PHYS_TO_K1(SFLASH_SEG_PADDR(n))

#define SFLASH_FPROM_HDR_ADDR	(flash_header_t *)PHYS_TO_K1(SFLASH_FPROM_PADDR)
#define SFLASH_RPROM_HDR_OFF	(SFLASH_RPROM_SIZE - FLASH_HEADER_SIZE)
#define SFLASH_RPROM_HDR_ADDR	(flash_header_t *)PHYS_TO_K1(SFLASH_RPROM_PADDR + \
							     SFLASH_RPROM_HDR_OFF)

/* CAUTION: Be very careful about K0 flash addresses */
#define SFLASH_RPROM_K0_ADDR	(vu_short *)PHYS_TO_K0(SFLASH_RPROM_PADDR)
#define SFLASH_FPROM_K0_ADDR	(vu_short *)PHYS_TO_K0(SFLASH_FPROM_PADDR)
#define SFLASH_SEG_K0_ADDR(n)	(vu_short *)PHYS_TO_K0(SFLASH_SEG_PADDR(n))

#define IS_FLASH_ADDR(x)	((__psunsigned_t)(x) >= (__psunsigned_t)SFLASH_BASE_ADDR &&	\
				 (__psunsigned_t)(x) <  (__psunsigned_t)SFLASH_SEG_ADDR(SFLASH_MAX_SEGS))

#define IS_PROM_PADDR(x)	((__psunsigned_t)(x) >= (__psunsigned_t)FLASH_MEM_BASE &&	\
				 (__psunsigned_t)(x) <  (__psunsigned_t)(FLASH_MEM_BASE+SFLASH_MAX_SIZE))
#endif /* LANGUAGE_C */

/*
 * follow addr/offsets are based on FLASH_MEM_BASE constant and
 * not the flash_mem_base global, these should only be use in
 * early RPROM startup
 */
#define SFLASH_FIXED_OFF(n)	(FLASH_MEM_BASE + SFLASH_SEG_OFF(n))
#define SFLASH_FIXED_FPROM_HDR	PHYS_TO_K1(SFLASH_FIXED_OFF(SFLASH_FPROM_SEG))
#define SFLASH_FIXED_FPROM_PADDR (SFLASH_FIXED_OFF(SFLASH_FPROM_SEG) + FLASH_HEADER_SIZE)
#define SFLASH_FIXED_FPROM_ADDR PHYS_TO_K1(SFLASH_FIXED_FPROM_PADDR)
#define SFLASH_FTEXT_ADDR	PHYS_TO_COMPATK0(SFLASH_FIXED_FPROM_PADDR)

/*
 * addresses of legal locations for flash executable addresses
 * WHEN WE ARE EXECUTING from the flash and
 * not necessarily when we are accessing currently (i.e. we may have
 * the real flash, the flash_mem_base, at the alternate flash address
 * when we execute off the dip prom)
 */
#define SFLASH_EXE_BASE		FLASH_MEM_BASE
#define SFLASH_XSEG_PADDR(n)	(SFLASH_EXE_BASE + SFLASH_SEG_OFF(n))


/*
 * FLASH Persistant Data Storage entry definitions for the msg/data segement
 */
#define FPDS_HDR_MAGIC		0x46504453	/* 'F' 'P' 'D' 'S' */
/*
 * data structure offset in half word (16bit) units
 */
#define FPDS_ENT_TYPELEN_OFFS	0
#define FPDS_ENT_VALID_OFFS	1
#define FPDS_ENT_DATA_OFFS	2

#define FPDS_ENT_TYPE_MASK	0xff00
#define FPDS_ENT_HLEN_MASK	0x00ff

#define FPDS_ENT_ENV_TYPE	0x0100
#define FPDS_ENT_LOG_TYPE	0x1000
#define FPDS_ENT_DA0_TYPE	0x4000
#define FPDS_ENT_DA1_TYPE	0x8000
#define FPDS_ENT_DAT_TYPE	(FPDS_ENT_DA0_TYPE | FPDS_ENT_DA1_TYPE)

#define FPDS_FILTER_VALID	FPDS_ENT_TYPE_MASK
#define FPDS_FILTER_ALL		0
#define FPDS_TYPELEN_FREE	0xffff

#define FPDS_ENT_VALID		0xffff

#define FPDS_ENT_MAX_DATA_LEN	(128)
#define FPDS_ENT_MAX_DATA_HLEN	(FPDS_ENT_MAX_DATA_LEN/sizeof(uint16_t))

#define FPDS_ENT_MAX_LEN	(510)
#define FPDS_ENT_MAX_HLEN	(FPDS_ENT_MAX_LEN/sizeof(uint16_t))

#define FPDS_ENT_DAT_MAX_LEN	(16 * 1024) 

#define FPDS_HI_WATER_MARK	((SFLASH_PDS_SIZE*90)/100)


#if LANGUAGE_C
#define FPDS_ENT_LEN(_ent)	(_ent->pds_hlen * sizeof(uint16_t))
#define FPDS_ENT_TYPE(_ent)	(_ent->pds_type_len & FPDS_ENT_TYPE_MASK)

#define FPDS_NEXT_ENT(_ent)						\
	{								\
	    vu_short *_p = (vu_short *)(_ent);				\
	    _p += _p[FPDS_ENT_TYPELEN_OFFS] & FPDS_ENT_HLEN_MASK;	\
	    _p += FPDS_ENT_DATA_OFFS; 					\
	    (_ent) = (flash_pds_ent_t *)_p;				\
	}

typedef volatile struct flash_pds_ent_s {
	union {
	    struct typelen_s {
		unsigned char 	type;		/* env, log */
		unsigned char	hlen;		/* datalen in half/short wds */
	    } typelen;
	    unsigned short	type_len;
	} u;
	unsigned short		valid;
	/* data max size is FPDS_ENT_MAX_DATA_HLEN*2 bytes */
	unsigned short		data[1];
} flash_pds_ent_t;

/* convenience macros */
#define pds_hlen		u.typelen.hlen
#define pds_type_len		u.type_len

#define PDS_LOG_TYPE0		0
#define PDS_LOG_TYPE1		1
#define PDS_LOG_TYPE2		2

#define PDS_LOG_DATA_SIZE	(FPDS_ENT_MAX_DATA_HLEN*2 - sizeof(uint64_t))
#define PDS_LOG_TICK_MASK	0x00ffffffffffffff
#define PDS_LOG_TYPE_MASK	0xff00000000000000
#define PDS_LOG_TYPE_SHFT	56

typedef struct flash_pds_log_data_s {
    	uint64_t		log_hdr;
	union {
	    char		c[PDS_LOG_DATA_SIZE];
	    uint16_t		h[1];
	    uint32_t		w[1];
	    uint64_t		d[1];
	    struct init_entry_s {
		uint32_t	tick_mult;
		uint32_t	tick_div;
		TIMEINFO	tod;
	    } init;
	} u;
} flash_pds_log_data_t;

typedef struct flash_pds_da0_s {
	uint16_t		datasum;
	uint16_t		datalen;
} flash_pds_da0_t;

/*
 * magic MUST be the 1st 32bits of the structure
 */
typedef volatile struct flash_pds_hdr_s {
	unsigned		magic;
	unsigned		erases;
	TIMEINFO		last_flashed;
	unsigned short		cksum;
} flash_pds_hdr_t;
#endif /* LANGUAGE_C */

/*
 * RPROM and FPROM Headers
 */

/* magic numbers */
#define FLASH_PROM_MAGIC	0x50524f4d      /* 'P' 'R' 'O' 'M' */

#define FLASH_HEADER_SIZE	256		/* rprom jumps to */
						/* fprom + FLASH_HEADER_SIZE */

/*
 * magic MUST be the 1st 32bits of the structure
 */
#if LANGUAGE_C
typedef volatile struct flash_header_s {
	int32_t		magic;			/* magic stuff */
	int32_t 	nflashed;		/* # times data flashed */
	int32_t 	version;		/* 24bit major, 8 bit minor */
	int32_t 	datalen;		/* length of data checksum'd */
	uint16_t 	dcksum;			/* data region cksum, sum -r style */
	TIMEINFO	timestamp;		/* timestamp */
	uint16_t 	hcksum;			/* header cksum, inet style */
} flash_header_t;
#endif /* LANGUAGE_C */

/*
 * flash error and debugging definitions
 */
#define FL_ERR			1		/* generic */

#if DEBUG
#define FDEBUG			1
#endif

/*
 * bit fields for 2 byte RPROMFLG in the dallas nvram
 * bits 0:2	- Verbose level
 * bits 3	- Fprom Validate Pending bit
 * bits 4:6	- Debug level
 * bits 7	- Rprom test bit
 */
#define RPROM_FLG_VRB_MSK	0x0007
#define RPROM_FLG_FVP		0x0008
#define RPROM_FLG_DBG_MSK	0x0070
#define RPROM_FLG_TEST		0x0080

#define RPROM_FLG_VRB(flg)	((flg) & RPROM_FLG_VRB_MSK)
#define RPROM_FLG_DBG(flg)	(((flg) & RPROM_FLG_DBG_MSK) >> 4)

#ifdef FDEBUG

#define	FPR_FLG_MSG	0x80000000
#define	FPR_FLG_ERR	0x40000000
#define	FPR_FLG_PERF	0x20000000
#define	FPR_FLG_HI	0x00000001
#define	FPR_FLG_LO	0x00000002
#define	FPR_FLG_PDS	0x00000004

#if LANGUAGE_C
extern unsigned int fpr_flg;

#define	FPR_HI(x)	if (fpr_flg & FPR_FLG_HI) printf x
#define	FPR_PDS(x)	if (fpr_flg & FPR_FLG_PDS) printf x
#define	FPR_LO(x)	if (fpr_flg & FPR_FLG_LO) printf x
#define	FPR_PERF(x)	if (fpr_flg & FPR_FLG_PERF) printf x
#define	FPR_MSG(x)	if (fpr_flg & FPR_FLG_MSG) printf x
#define	FPR_ERR(x)	if (fpr_flg & FPR_FLG_ERR) printf x
#define FPR_PR(x)	printf x

#ifdef IP22
/*
 * this section requires mc.h
 */
#define SETUP_TIME      static volatile unsigned int rpss_t0;
#define START_TIME      rpss_t0 = *(vu_int *)PHYS_TO_K1(RPSS_CTR)

#define SAMPLE_TICKS(ticks)					\
{								\
    volatile unsigned int rpss_t1;				\
    rpss_t1 = *(volatile unsigned int *)PHYS_TO_K1(RPSS_CTR);	\
    if (rpss_t1 > rpss_t0)					\
        (ticks) = rpss_t1 - rpss_t0;				\
    else 							\
	(ticks) = rpss_t1 + (0xffffffff - rpss_t0);		\
}

#define STOP_TIME(str)						\
{								\
    volatile unsigned int rpss_ticks;				\
    SAMPLE_TICKS(rpss_ticks);					\
    FPR_PERF(("%s took %d.%04d msecs\n",			\
	    (str),rpss_ticks/10000,rpss_ticks%10000));		\
}
#elif IP30

#define SETUP_TIME      static vu_long ht_t0;
#define START_TIME      ht_t0 = HEART_PIU_K1PTR->h_count

#define SAMPLE_TICKS(ticks)					\
{								\
    (ticks) = HEART_PIU_K1PTR->h_count - ht_t0;			\
}

#define STOP_TIME(str)						\
{								\
    vu_long ht_ticks;						\
    ulong ns;							\
    SAMPLE_TICKS(ht_ticks);					\
    ns = ht_ticks * HEART_COUNT_NSECS;				\
    FPR_PERF(("%s took %d,%03d,%03d nsecs\n",			\
	    (str),ns/1000000, (ns%1000000)/1000, (ns%1000000)%1000));\
}
#endif	/* IP30 */

#endif /* LANGUAGE_C */
#else	/* !FDEBUG */

#define	FPR_HI(x)
#define	FPR_LO(x)
#define	FPR_PDS(x)
#define	FPR_PERF(x)
#define	FPR_MSG(x)
#define	FPR_ERR(x)	printf x
#define FPR_PR(x)	printf x

#define SETUP_TIME
#define START_TIME
#define SAMPLE_RPSS(ticks)
#define SAMPLE_USECS(us)
#define SAMPLE_TICKS(ticks)
#define STOP_TIME(str)

#endif	/* !FDEBUG */


#if _STANDALONE
/*********************************************************
 * Standalone System I/F
 *
 */
#define FLASH_ERR_PARANOID 	-1
#define FLASH_ERR_ERASE_TO	-2
#define FLASH_ERR_WRITE_TO	-3
#define FLASH_ERR_UNKOWN_CMD	-4
#define FLASH_ERR_PDS_ADDR 	-5
#define FLASH_ERR_PDS_NEW 	-6
#define FLASH_ERR_STR_TOO_LONG	-7
#define FLASH_ERR_ENT_TOO_LONG	-8
#define FLASH_ERR_OUT_OF_MEM	-9

#if LANGUAGE_C

typedef __psunsigned_t		flash_err_t;

int yes(char *question);
u_short flash_cksum_r(char *, int len, u_short);
int  flash_init(void);

void flash_id(unsigned short *, unsigned short *);
void flash_print_status(int);
void flash_print_prom_info(flash_header_t *, u_short *);
void flash_print_pds_status(int);

int  flash_get_nflashed(int);

int  flash_get_nv_cnt(int);
void flash_set_nv_cnt(int, int *);

unsigned short flash_get_nv_rpromflg(void);
void flash_set_nv_rpromflg(ushort flg);

void flash_upd_prom_hdr(int, __psunsigned_t);
int  flash_header_ok(flash_header_t *);
int  flash_header_invalidate(void *);
int  flash_prom_ok(char *, flash_header_t *);
int  flash_ok(void);

int  flash_fprom_reloc_chk(void);

void flash_print_err(flash_err_t);
flash_err_t  flash_erase(int);
flash_err_t  flash_cp(uint16_t *, vu_short *, int);
flash_err_t  flash_program(uint16_t *, int, unsigned, unsigned);

/* pds i/f */
flash_pds_ent_t *flash_pds_ent_next(flash_pds_ent_t *p, u_short filter);
vu_short *flash_pds_init(int);
flash_err_t flash_pds_ent_write(u_short *data, int type, int len);
flash_err_t flash_pds_ent_inval(flash_pds_ent_t *p);

/* env i/f */
void flash_resetenv(void);
int flash_setenv(char *var, char *str);
int flash_loadenv(int (*putstr)(char *, char *, struct string_list *), struct string_list *);

/* log i/f */
void flash_pds_prf(char *fmt, ...);
void flash_pds_log(char *);
void flash_print_log(void *, int);
flash_err_t flash_pds_write_log(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
flash_pds_ent_t *flash_pds_get_log(flash_pds_ent_t *p, char **data, int *len, int inval);

/* data i/f */
flash_pds_ent_t *flash_pds_find_data(char *key, flash_pds_ent_t *ent);
void flash_pds_inval_data(char *key);
int flash_pds_copy_data(char *key, char *buf, int cksum);
flash_err_t flash_pds_set_data(char *key, char *data, int datalen);

/* debug  */
#ifdef FDEBUG

flash_pds_ent_t *flash_findenv(flash_pds_ent_t *p, char *var);
void flash_pds_init_free(void);
vu_short *flash_pds_compress(int);
#endif

void dump_bytes(char *ptr, int count, int flag /*show addr*/);
#endif /* LANGUAGE_C */


#endif	/* _STANDALONE */

#endif	/* !_SFLASH_H */
