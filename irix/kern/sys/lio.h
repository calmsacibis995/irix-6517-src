/*
 * List IO (Kernel Support for Oracle KLIO)
 */

#ifndef _SYS_LIO_H
#define _SYS_LIO_H

#ifdef _KERNEL
typedef struct {
        timespec_t startq;
        timespec_t startwait;
        timespec_t endwait;
        timespec_t endphysio;
        timespec_t endstat;
} kl_timings_t;

typedef struct parg {
        off_t		offset;	/* forces 8-byte alignment for K32 cbuf & timings */
#if (_MIPS_SZPTR == 32)
	int		cbuf_pad;
#endif
	char		*cbuf;
        int		fdes;
        unsigned int	count;
        int		sbase;
        int		rw;
        int     	rval;
        int     	err;
	/* below stuff is not normally used */
#if (_MIPS_SZPTR == 32)
	int		timings_pad;
#endif
	kl_timings_t	*timings;
        dev_t	   	rdev;
} parg_t;

typedef struct {
	struct uio	uio;
	struct iovec    iov;
	struct vfile	*fp;
} puio_t;

#define	KLISTIO		1
#define	KAIO		2

#else
/* IRIX 5.3 */
typedef struct parg {
        int	fdes;
        char	*cbuf;
        unsigned int count;
        off_t	offset;
        int	sbase;
        int     rw;
        int     rval;
        int     err;
        timespec_t reserved1[5];
        dev_t   reserved2;
} parg_t;

typedef struct parg64 {
        off64_t offset;		/* forces 8-byte alignment for cbuf */
#if (_MIPS_SZPTR == 32)
	int	reserved1;
#endif
	char	*cbuf;
        int	fdes;
        unsigned int count;
        int	sbase;
        int     rw;
        int     rval;
        int     err;
	__int64_t reserved2;
        dev_t   reserved3;
} parg64_t;
#endif /*_KERNEL*/

#endif
