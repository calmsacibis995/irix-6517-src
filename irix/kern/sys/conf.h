/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _IO_CONF_H	/* wrapper symbol for kernel use */
#define _IO_CONF_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:io/conf.h	1.4"*/
#ident	"$Revision: 3.40 $"

/*
 * Declaration of block device switch. Each entry (row) is
 * the only link between the main unix code and the driver.
 */
struct cred;
struct uio;
struct pollhead;
struct device_driver_s;
struct vhandl;
struct buf;
struct bdevsw {
	int	*d_flags;
	int	d_cpulock;
	int	(*d_open)(dev_t *, int, int, struct cred *);
	int	(*d_close)(dev_t, int, int, struct cred *);
	int	(*d_strategy)(struct buf *);
	int	(*d_print)(dev_t, char *);
	int	(*d_map)(struct vhandl *, off_t, size_t, uint_t);
	int	(*d_unmap)(struct vhandl *);
	int	(*d_dump)(dev_t, int, daddr_t, caddr_t, int);
	int	(*d_size)(dev_t);
	int	(*d_size64)(dev_t, daddr_t *);
	struct cfg_desc *d_desc;
	struct device_driver_s *d_driver;
};

/*
 * Character device switch.
 */
struct cdevsw {
	int	*d_flags;
	int	d_cpulock;
	int	(*d_open)(dev_t *, int, int, struct cred *);
	int	(*d_close)(dev_t, int, int, struct cred *);
	int	(*d_read)(dev_t dev, struct uio *uiop, struct cred *crp);
	int	(*d_write)(dev_t dev, struct uio *uiop, struct cred *crp);
	int	(*d_ioctl)();
	int	(*d_mmap)(dev_t, off_t, int);
	int	(*d_map)();
	int	(*d_unmap)();
	int	(*d_poll)();
	int	(*d_attach)(dev_t);
	int	(*d_detach)(dev_t);
	int	(*d_enable)(dev_t);
	int	(*d_disable)(dev_t);
	struct	tty *d_ttys;
	struct	streamtab *d_str;
	struct cfg_desc *d_desc;
	struct device_driver_s *d_driver;
};

#define	DC_OPEN		1
#define DC_CLOSE	2
#define	DC_READ		3
#define DC_WRITE	4
#define DC_IOCTL	5
#define DC_PRINT	6
#define DC_STRAT	7
#define DC_DUMP		8
#define DC_SIZE		9
#define DC_MAP		10
#define DC_UNMAP	11
#define DC_POLL		12
#define DC_MMAP		13
#define DC_ATTACH	14
#define DC_DETACH	15
#define DC_ENABLE	16
#define DC_DISABLE	17
#define	DC_SIZE64	18

#ifdef _KERNEL

int bdstrat(struct bdevsw *, struct buf *);

#define bdopen(devsw,devp,a2,a3,a4)	bdrv(devsw,DC_OPEN,devp,a2,a3,a4)
#define bdprint(devsw,a1,a2)		bdrv(devsw,DC_PRINT,a1,a2)
#define bdclose(devsw,a1,a2,a3,a4)	bdrv(devsw,DC_CLOSE,a1,a2,a3,a4)
#define bddump(devsw,a1,a2,a3,a4,a5)	bdrv(devsw,DC_DUMP,a1,a2,a3,a4,a5)
#define bdsize(devsw,a1)		bdrv(devsw,DC_SIZE,a1)
#define bdsize64(devsw,a1,a2)		bdrv(devsw,DC_SIZE64,a1,a2)
#define cdopen(devsw,devp,a2,a3,a4)	cdrv(devsw,DC_OPEN,devp,a2,a3,a4)
#define cdclose(devsw,dev,a2,a3,a4)	cdrv(devsw,DC_CLOSE,dev,a2,a3,a4)
#define cdread(devsw,dev,uio,cr)	cdrv(devsw,DC_READ,dev,uio,cr)
#define cdwrite(devsw,dev,uio,cr)	cdrv(devsw,DC_WRITE,dev,uio,cr)
#define cdioctl(devsw,dev,a2,a3,a4,a5,a6) cdrv(devsw,DC_IOCTL,dev,a2,a3,a4,a5,a6)
#define cdpoll(devsw,dev,a2,a3,a4,a5,a6) cdrv(devsw,DC_POLL,dev,a2,a3,a4,a5,a6)
#define cdattach(devsw,dev)		cdrv(devsw,DC_ATTACH,dev)
#define cddetach(devsw,dev)		cdrv(devsw,DC_DETACH,dev)
#define cdenable(devsw,dev)		cdrv(devsw,DC_ENABLE,dev)
#define cddisable(devsw,dev)		cdrv(devsw,DC_DISABLE,dev)
#endif
struct __vhandl_s;
int bdmap(struct bdevsw *, struct __vhandl_s *, off_t, size_t, uint_t);
int bdunmap(struct bdevsw *, struct __vhandl_s *);
int cdmap(struct cdevsw *, dev_t, struct __vhandl_s *, off_t, size_t, uint_t);
int cdunmap(struct cdevsw *, dev_t, struct __vhandl_s *);
int cdmmap(struct cdevsw *, dev_t, off_t, uint_t);

/*
 * driver semaphoring types
 * and cdevsw/bdevsw flags
 */
#define DLOCK_MASK	0x7
#define	D_PROCESSOR	0x0	/* driver executed on master processor */
#define	D_MP		0x1	/* driver does own semaphoring */
#define	D_MT		0x2	/* driver is thread-aware */
/* #define DLOCK_xx	0x4	unused */
#define	D_WBACK		0x8	/* write back cache before strategy routine */

/*
 * Device flags.
 *
 * Bit 0 to bit 15 are reserved for kernel.
 * Bit 16 to bit 31 are reserved for different machines.
 */
#define	D_OBSOLD		0x10	/* old-style driver - OBSOLETE */
/*
 * Added for UFS.
 */
#define D_SEEKNEG       0x20    /* negative seek offsets are OK */
#define D_TAPE          0x40    /* magtape device (no bdwrite when cooked) */
/*
 * Added for pre-4.0 drivers backward compatibility.
 */
#define D_NOBRKUP	0x80	/* no breakup needed for new drivers */
#define D_ASYNC_ATTACH  0x100

#define	FMNAMESZ	8

struct fmodsw {
	char	f_name[FMNAMESZ+1];
	struct  streamtab *f_str;
	int	*f_flags;	/* same as device flags */
};

#define	GROUPNAMESZ	20

/*
 * tunable group table 
 */
struct tunetable {
	char		t_name[GROUPNAMESZ+1];
	unsigned int	t_flag;
	int     	(*t_sanity)();
};
	
#define N_STATIC 1
#define N_RUN    2

#define TUNENAMESZ	63
/*
 * tunable name table
 */
struct tunename {
	char		t_name[TUNENAMESZ + 1];
	int		*t_addr;
	int		t_size;
	int		t_group;
};

/*
 * MAXDEVNAME defines the longest possible canonical device name.
 */
#define MAXDEVNAME 256


#ifdef _KERNEL

extern struct bdevsw bdevsw[];
extern struct cdevsw cdevsw[];
extern struct fmodsw fmodsw[];

extern int	bdevcnt;
extern int	cdevcnt;
extern int	fmodcnt;
extern int	bdevmax;
extern int	cdevmax;
extern int	fmodmax;
extern int	vfsmax;
extern int	nfstype;
extern int	bdrv(struct bdevsw *, int, ...);
extern int	cdrv(struct cdevsw *, int, ...);

#define bdstatic(b) ((b)->d_desc == NULL)
#define cdstatic(c) ((c)->d_desc == NULL)
#define fmstatic(fmaj)  ((fmaj)< fmodcnt)

#define bdvalid(b)  ((b) != NULL)
#define cdvalid(c)  ((c) != NULL)
#define fmvalid(fm)  (fmstatic(fm) || ((fm)< fmodmax && fmodsw[fm].f_flags))

extern int cdhold(struct cdevsw *);
extern void cdrele(struct cdevsw *);
extern int bdhold(struct bdevsw *);
extern void bdrele(struct bdevsw *);
extern int fmhold(int);
extern void fmrele(int);

extern struct tunename tunename[];
extern struct tunetable tunetable[];
extern int tunetablefind(char *);

extern char *dev_to_name(dev_t, char *, uint);

#include <sys/hwgraph.h>

/* Given a dev_t, return the corresponding bdevsw entry */
#define get_bdevsw(dev) (dev_is_vertex(dev) ? \
			hwgraph_bdevsw_get(dev_to_vhdl(dev)) : \
			(((major(dev) < bdevcnt) || \
			 ((major(dev) < bdevmax) && bdevsw[major(dev)].d_flags)) ? \
				&(bdevsw[major(dev)]) : NULL))


/* Given a dev_t, return the corresponding cdevsw entry */
#define get_cdevsw(dev) (dev_is_vertex(dev) ? \
			hwgraph_cdevsw_get(dev_to_vhdl(dev)) : \
			(((major(dev) < cdevcnt) || \
			 ((major(dev) < cdevmax) && cdevsw[major(dev)].d_flags)) ? \
				&(cdevsw[major(dev)]) : NULL))

/* clone driver support */
struct cdevsw * clone_get_cdevsw(dev_t);

#endif	/* _KERNEL */
#endif	/* _IO_CONF_H */
