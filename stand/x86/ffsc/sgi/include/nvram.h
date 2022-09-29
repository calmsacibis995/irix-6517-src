/*
 * FFSC NVRAM constants and declarations
 */

#ifndef _FFSC_NVRAM_H_
#define _FFSC_NVRAM_H_


/* NVRAM object IDs */
#define NVRAM_HDR	0		/* NVRAM header */
#define NVRAM_NETINFO	1		/* Network address info */
#define NVRAM_RACKID	2		/* Rack ID */
#define NVRAM_DEBUGINFO 3		/* Debugging info */
#define NVRAM_TTYINFO   4		/* TTY info */
#define NVRAM_ELSCINFO	5		/* ELSC info */
#define NVRAM_CONSTERM  6		/* Console info for CONS_TERMINAL */
#define NVRAM_CONSALT	7		/* Console info for CONS_ALTERNATE */
#define NVRAM_TUNE	8		/* Tuneable variables */
#define NVRAM_LOGINFO	9		/* Logging info */
#define NVRAM_USERTERM	10		/* User info for USER_TERMINAL */
#define NVRAM_USERALT	11		/* User info for USER_ALTERNATE */
#define NVRAM_PASSINFO	12		/* System passwords */
/* Additions to this list must also be updated in nvram.c! */


/* Type declarations */
typedef unsigned int nvramid_t;		/* NVRAM object ID */

#define NH_VALID_WORDS	4
typedef struct nvramhdr {		/* NVRAM header information */
	uint32_t Signature;		/* FFSC signature */
	uint32_t Version;		/* FFSC NVRAM layout version # */
	uint32_t NumObjs;		/* Number of objects */
	uint32_t Size;			/* Total size of objects */
	uint32_t Valid[NH_VALID_WORDS];	/* Object valid bits */
	uint32_t reserved[8];		/* reserved for future expansion */
} nvramhdr_t;

#define NH_SIGNATURE	0xff5cff5c	/* FFSC NVRAM signature */
#define NH_CURR_VERSION	6		/* NVRAM format version number */


/* Function prototypes */
STATUS nvramInit(void);
STATUS nvramPrintInfo(void);
STATUS nvramRead(nvramid_t, void *);
STATUS nvramReset(void);
STATUS nvramWrite(nvramid_t, void *);
STATUS nvramZap(void);


#endif  /* _FFSC_NVRAM_H_ */
