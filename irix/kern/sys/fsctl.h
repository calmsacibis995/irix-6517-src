#ident	"$Revision: 1.5 $"
/*
 * definitions for fsctl pseudo-device
 */

#define ILOCK           0x1
#define ICOMMIT         0x2
#define BALLOC          0x3
#define BFREE           0x4
#define TSTALLOC        0x5
#define TSTFREE         0x6
#define IUNLOCK         0x7

/*
 * Be all end all multi-purpose data structure.
 * Different users (ioctl's) use different parts.
 */
struct fscarg {
        dev_t   fa_dev;
        ino_t   fa_ino;
        int     fa_ne;	/* # direct extents in 'ex' */
        int     fa_nie;	/* # indirect extents in 'ix' */
        extent  *fa_ex; /* direct extents */
	extent	*fa_ix;	/* indirect extents */
        int     fa_len;
        daddr_t fa_bn;
};

#ifdef _KERNEL

struct irix5_o32_fscarg {
        app32_ulong_t   fa_dev;
        app32_ulong_t   fa_ino;
        app32_int_t     fa_ne;	/* # direct extents in 'ex' */
        app32_int_t     fa_nie;	/* # indirect extents in 'ix' */
        app32_ptr_t     fa_ex;	/* direct extents */
	app32_ptr_t     fa_ix;	/* indirect extents */
        app32_int_t     fa_len;
        app32_long_t    fa_bn;
};

struct irix5_n32_fscarg {
        app32_ulong_t      fa_dev;
        app32_ulong_long_t fa_ino;
        app32_int_t        fa_ne;	/* # direct extents in 'ex' */
        app32_int_t        fa_nie;	/* # indirect extents in 'ix' */
        app32_ptr_t        fa_ex;	/* direct extents */
	app32_ptr_t        fa_ix;	/* indirect extents */
        app32_int_t        fa_len;
        app32_long_long_t  fa_bn;
};

#endif /* _KERNEL */
