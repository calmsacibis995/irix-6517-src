#ifndef _SYS_IP32FLASH_H_
#define _SYS_IP32FLASH_H_
#include <sys/IP32.h>


#ifndef _STANDALONE
#define SEG_MAX_NAME    32
#define SEG_MAX_VSN     8

#define FLASH_SEGMENT_MAGIC	'SHDR'
#define FLASH_SEGMENT_MAGICx	0x53484452

#define __RUP(x,t)      ((t)(((unsigned long)(x)+3)&~3))
#define name(f)         (f->name)
#define version(f)      (f->version)
#define chksum(f)       (&f->chksum)
#define body(f)         (chksum(f)+1)
#define segChksum(f)    (__RUP(((char*)f)+f->segLen,long*)-1)
#define newSegChksum(f,n)   (__RUP(((char*)f)+n->segLen,long*)-1)
#define hdrSize(f)      (long)(sizeof(FlashSegment))
#define bodySize(f)     (f->segLen - hdrSize(f))

typedef struct {
  long long reserved;                   /* 00 Reserved */
  long      magic;                      /* 08 Segment magic */
  long      segLen;                     /* 0c Segment length in bytes (includes
hdr) */
  unsigned long nameLen : 8,            /* 10 Length of name in bytes */
                vsnLen  : 8,            /* 11 Length of vsn data in bytes */
                segType : 8,            /* 12 Segment type */
                pad     : 8;            /* 13 Pad */
  char      name[SEG_MAX_NAME];         /* 14 segment name */
  char      version[SEG_MAX_VSN];       /* 34 of version data */
  long      chksum;                     /* 3c Header checksum */
} FlashSegment;

#endif
/*
 * CPU types supported by prom/
 */
#define PROM_CPU_R4600       0x1         /* R4600PC cpu module      */
#define PROM_CPU_R4600SC     0x2         /* R4600SC cpu module      */
#define PROM_CPU_R5000       0x4         /* R5000PC cpu module      */
#define PROM_CPU_R5000SC     0x8         /* R5000SC cpu module      */
#define PROM_CPU_R5000LM     0x10        /* R5000/LM cpu module     */
#define PROM_CPU_R5000SCLM   0x20        /* R5000SC/LM cpu module   */
#define PROM_CPU_R10000      0x40        /* R10000  cpu module      */
#define PROM_CPU_R10000MP    0x80        /* MP R10000 cpu module    */
#define PROM_CPU_R10000LM    0x100       /* R10000/LM cpu module    */
#define PROM_CPU_R10000MPLM  0x200       /* MP R10000/LM cpu module */

/*
 * Flash memory parameters
 */
#define FLASH_PHYS_BASE	0x1fc00000
#define FLASH_ROM_BASE		FLASH_PHYS_BASE
#define FLASH_K1BASE		PHYS_TO_K1(FLASH_PHYS_BASE)
#define FLASH_PAGE_SIZE 	0x100
#define FLASH_PROTECTED	0x4000 /* Size of protected segment */
#define FLASH_SIZE             (512*1024)
#define FLASH_PROGRAMABLE	(FLASH_PHYS_BASE+FLASH_PROTECTED)
#define FLASH_WENABLE          PHYS_TO_K1(ISA_FLASH_NIC_REG)

#define FLASH_WP_OFF           1
#define FLASH_WP_ON            ~FLASH_WP_OFF

#define NVLEN_MAX	256
#define NVOFF_CHECKSUM	0
#define NVLEN_CHECKSUM	1

#define PASSWD_LEN 8
#define NVLEN_PASSWD_KEY 8
#define NVOFF_PASSWD_KEY 0

extern char *flash_getenv(char *);
#if (_KERNEL)
extern int flash_version(int);
#else
extern int flash_version(int, char *, int);
#endif
extern int flash_write_sector(volatile char *, char *);
extern void flash_set_nvram_changed(void);
#ifdef RESET_DUMPSYS
extern void flash_jump_vector(void);
extern int flash_dump_set;
#endif /* RESET_DUMPSYS */
extern void flash_sync_globals(void);
#if DEBUG
extern void flash_dump_globals(void);
#endif /* DEBUG */

/* arguments for flash_version() */
#define FLASHPROM_MAJOR 0
#define FLASHPROM_MINOR 1

#ifndef _STANDALONE
/*
 * this structure is the header for the PROM image
 * as shipped on CD-ROM.  The image consists of
 * POST2 and other code, fonts, boot tune, data,
 * etc.  It is a complete PROM image sans the persistent
 * environment variables.
 *
 * Note that this structure is padded to the size of a
 * flash page.  This makes it easier to map the file
 * in when it is to be written into the flash.  Remember
 * to adjust the size of the pad if any chages are made
 * to this structure.
 */
#define PROM_MAGIC (('P' << 24)|('R' << 16)|('O' << 8)|('M'))
typedef struct {
    int magic;
    int cksum;     /* checksum of the prom image */
    int offset;    /* offset from 0xbfc00000 to load at */
    int len;       /* length of the image which follows */
    int version[2];/* PROM version number               */
    int cputypes;  /* cpu types supported by this prom  */
    char pad[FLASH_PAGE_SIZE - (7 * sizeof(int))];
} promhdr_t;
#endif

#endif
