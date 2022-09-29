/* ide_s1chip.h 
 * 	Contains defs from libsk/ml/s1chip.c which we want to use for our
 *	ide tests.
 */

typedef struct s1chip {
        uchar_t s1_slot;                /* slot for io4 */
        uchar_t s1_adap;                /* adapter number */
        uchar_t s1_win;                 /* window for this bus */
        uchar_t s1_adap_type;           /* chips attached */
        int     (*s1_func)();           /* interrupt routine */
        int     s1_intr_lvl;            /* interrupt level */
#ifndef TFP
        caddr_t s1_swin;                /* small window */
#else
        paddr_t s1_swin;                /* small window */
#endif
        uchar_t s1_num_chans;           /* number of channels */
        uchar_t s1_chan[3];             /* scsi_chan assigned */
} s1chip_t;

#define MAX_S1_CHIPS    16              /* maximum s1 chips */
#define MAX_S1_CHANS    3               /* maximum channel per chip */
#define MAX_SCSI_CHANNELS       128     /* max scsi buses */
#define MAX_CHANS_IO4   10               /* max scsi buses per io4 board */

#define S1_GET_REG(_addr, _reg)         load_double((long long *)(_addr + _reg))
#define S1_PUT_REG(_addr, _reg, _value) store_double((long long *)(_addr + _reg), (long long)(_value))


#define S1_CTRL(_chan)          (S1_CTRL_BASE + _chan * S1_CTRL_OFFSET)


#define GET_DMA_REG(_sbp, _reg) \
                *(uint *)(_sbp + _reg + 4)
#define SET_DMA_REG(_sbp, _reg, _val) \
                *(uint *)(_sbp + _reg + 4) = (_val)
#define S1_ON_MOTHERBOARD       4

/* pieces from stand/fx */
struct strbuf
{
        char c[128];            /* max size string */
};
typedef struct strbuf IDESTRBUF;

#define SCSI_NAME  "dksc"                  /* Name of scsi disk driver */
#define SCSI_NUMBER     0                       /* Internal number for scsi */
#define DKIP_NAME       "dkip"                  /* Name of dkip disk driver */
#define DKIP_NUMBER     1                       /* Internal number for dkip */
#define XYL_NAME        "xyl"                   /* Name of xyl disk driver */
#define XYL_NUMBER      2                       /* Internal number for xyl */
#define IPI_NAME        "ipi"                   /* Name of ipi disk driver */
#define IPI_NUMBER      4                       /* Internal number for ipi */
#define JAG_NAME        "jag"                   /* Name of vme-scsi driver */
#if !defined(_STANDALONE) && defined(SCSI)
/* smfd floppy under unix only, and only if SCSI defined. */
#define SMFD_NAME       "fd"             /* prefix for floppy disk devices */
#define SMFD_NUMBER     3                       /* Internal number for smfd */
#endif

#define MAX_DRIVE 16
#define VP(x)          ((struct volume_header *)x)
#define DP(x)             (&VP(x)->vh_dp)
#define PT(x)          (VP(x)->vh_pt)
#define dp_heads  dp_trks0

#define PTNUM_FSROOT            0               /* root partition */
#define PTNUM_FSSWAP            1               /* swap partition */
#define PTNUM_FSUSR             6               /* usr partition */
#define PTNUM_FSALL             7               /* one big partition */
#define PTNUM_VOLHDR            8               /* volume header partition */
#define PTNUM_REPL              9               /* trk/sec replacement part */
#define PTNUM_VOLUME            10              /* entire volume partition */

/* default dma transfer sizes */
#define DFLT_BLK_SIZE        512
#define DFLT_BLK_NUM         1

