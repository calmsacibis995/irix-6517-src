
#ifndef _MKFP_H_
#define _MKFP_H_

#include "fp.h"


#define	MKFP_NONE	 0
#define MKFP_GENERIC	 1
#define MKFP_LOWFMT_F    2
#define MKFP_LOWFMT_S    3
#define MKFP_WRITE       4
#define MKFP_MEMORY      5
#define MKFP_TYPE        6
#define MKFP_USAGE       7
#define MKFP_VLABEL      8
#define MKFP_NOTFLOP     9
#define MKFP_FSYSTYPE    10
#define MKFP_OPENDEV     11
#define MKFP_MOUNTED     12
#define MKFP_IOCTL       13
#define MKFP_WPROT       14
#define MKFP_DEVEXIST    15
#define MKFP_DEVVH       16
#define MKFP_UNINITFP    17
#define MKFP_BADSEC      18
#define MKFP_BADFLOP     19
#define	MKFP_FLOPPY	 20
#define	MKFP_CMDLINE	 21
#define	MKFP_PART_CREATE 22
#define	MKFP_NO_FS	 23
#define	MKFP_SCSI_QUERY	 24
#define	MKFP_NO_FMT	 25

#define DOS             (1)
#define HFS             (2)

#define P_INIT          (0)             /* Initial state */
#define P_ENTIRE        (1)             /* Create one large partition state */
#define P_SEVERAL       (2)             /* Create several partitions state  */
#define P_VIEW          (3)             /* View partitions state */

#define	Throw(condition, var, err, label) \
			if (condition){	      \
				var = err;    \
				goto label;   \
			}

			

/* Structure to hold list of partitions to be created */
typedef struct pstr {
        char    *plabel;                /* Partition label */
        u_long  psize;                  /* Partition size (in bytes) */
} pstr_t;



extern int    do_smfdformat(FPINFO *);
extern int    do_scsiformat(FPINFO *);
extern int    scsi_readcapacity(FPINFO *, unsigned *);
extern int    dm_format(void *, char *, int);
extern void   dos_formatfunc_init(int);
extern int    dos_format(FPINFO *);
extern int    mac_volabel(FPINFO *, char *);
extern int    dos_volabel(FPINFO *, char *);
extern char * spaceskip(char *);
#endif
