/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:io/mem.c	10.6"*/
#ident	"$Revision: 3.99 $"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/sema.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/nodepda.h>
#include <sys/sbd.h>
#include <sys/sysmacros.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/edt.h>
#include <sys/vmereg.h>
#include <sys/pfdat.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/xlate.h>

#ifdef EVEREST
#include <sys/EVEREST/io4.h>
#endif
#if IP21
#include <sys/EVEREST/IP21.h>
#endif

#ifdef SN
#include <sys/SN/addrs.h>
extern nasid_t master_nasid;
#endif

#if RAWIO_DEBUG
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
int	numdmas = 1000;
dev_t rawio_dev, rawiot_dev;
int	print_debug;
#define IS_RAWIO_DEV(d) (((d) == rawio_dev) || (!dev_is_vertex(d) && (minor(d) == 4)))
#define IS_RAWIOT_DEV(d) (((d) == rawiot_dev) || (!dev_is_vertex(d) && (minor(d) == 5)))
#else /* RAWIO_DEBUG */
#define IS_RAWIO_DEV(d) 0
#define IS_RAWIOT_DEV(d) 0
#endif /* RAWIO_DEBUG */

paddr_t verify_mappable_tstamp(paddr_t, long);
#if IP19 || IP25
static int ip19_memrw(uio_t*, uio_rw_t, __psunsigned_t, __psunsigned_t);
#endif

dev_t mem_dev, kmem_dev, null_dev, mmem_dev;
#define IS_MEM_DEV(d)  (((d) == mem_dev)  || (!dev_is_vertex(d) && (minor(d) == 0)))
#define IS_KMEM_DEV(d) (((d) == kmem_dev) || (!dev_is_vertex(d) && (minor(d) == 1)))
#define IS_NULL_DEV(d) (((d) == null_dev) || (!dev_is_vertex(d) && (minor(d) == 2)))
#define IS_MMEM_DEV(d) (((d) == mmem_dev) || (!dev_is_vertex(d) && (minor(d) == 3)))

int mmdevflag = D_MP;

struct mmmap_addrs {
	unsigned long m_size;		/* number of bytes that can be mapped */
	unsigned long m_addr;		/* address of base page to be mapped */
	int m_prot;			/* PROT_READ, PROT_WRITE, PROT_EXEC */
};

/* ARGSUSED */
mmopen(dev_t *devp, int flag, int otyp, cred_t *crp)
{
	dev_t dev = *devp;

	if (	IS_MEM_DEV(dev) ||
		IS_KMEM_DEV(dev) ||
		IS_NULL_DEV(dev) ||
		IS_MMEM_DEV(dev) ||
		IS_RAWIO_DEV(dev) ||
		IS_RAWIOT_DEV(dev))
			return(0);
	else
			return(ENXIO);
}

int
mmclose()
{
	return 0;
}

/*ARGSUSED*/
int
mmpoll(dev_t dev, short events, int anyyet, short *reventsp,
       struct pollhead **phpp, unsigned int *genp)
{
	*reventsp = events;
	return 0;
}

static int mmrw(dev_t, uio_rw_t, uio_t *);

int
mmread(dev_t dev, uio_t *uio)
{
	return mmrw(dev, UIO_READ, uio);
}

int
mmwrite(dev_t dev, uio_t *uio)
{
	return mmrw(dev, UIO_WRITE, uio);
}

/* check the validity of addresses */
static int
mmck(register __psunsigned_t off, register u_long len, uio_rw_t rw, uio_t *uio)
{
	struct iovec *iov;
	register int incr;
	int val;

	iov = uio->uio_iov;

	while (len > 0) {
		if (iov->iov_len == 0) {
			iov = ++uio->uio_iov;
			--uio->uio_iovcnt;
			continue;
		}

		/* first try word accesses */
		if (len > 3 && !(off & 3))	
			incr = 4;
		/* then try half words */
		else if (len > 1 && !(off & 1))	
			incr = 2;
		/* then bytes */
		else
			incr = 1;
		
		switch (rw) {
		case UIO_READ:
			if (badaddr_val((volatile void *)off, incr, &val))
				return ENXIO;
			if (copyout((caddr_t)&val, iov->iov_base, incr))
				return EFAULT;
			break;

		case UIO_WRITE:
			if (copyin(iov->iov_base, (caddr_t)&val, incr))
				return EFAULT;
			if (wbadaddr_val((volatile void *)off, incr, &val))
				return ENXIO;
			break;

		default:
			if (badaddr((volatile void *)off, incr))
				return ENXIO;
			break;
		}
		iov->iov_base = (char *)iov->iov_base + incr;
		iov->iov_len -= incr;
		uio->uio_resid -= incr;
		uio->uio_offset += incr;
		len -= incr;
		off += incr;
	}

	return 0;
}

static int
mmrw(dev_t dev, uio_rw_t rw, uio_t *uio)
{
#ifdef _NO_UNCACHED_MEM_WAR
	extern int ip26_allow_ucmem;
#endif
#ifdef IP32
        uint aPnum;
#endif
	struct iovec *iov;
	int error;

	/*
	 * Access /dev/null
	 */
	if (IS_NULL_DEV(dev)) {
		if (rw == UIO_WRITE) {
			uio->uio_offset += uio->uio_resid;
			iov = uio->uio_iov;
			iov->iov_base = (char *)iov->iov_base + uio->uio_resid;
			iov->iov_len -= uio->uio_resid;
			uio->uio_resid = 0;
		}
		return 0;
	} 

	/* Disallow read/write attempts to /dev/mmem */
	if (IS_MMEM_DEV(dev))
		return EINVAL;

#ifdef RAWIO_DEBUG

	/*
	 * This is special code to aid in debugging the setup and
	 * teardown work needed to do raw I/O.  It goes through the
	 * work of locking down the pages and then unlocking them
	 * without actually doing any real I/O.  This makes it possible
	 * to do a lot more testing in a short time (by eliminating
	 * the physical device overhead).
	 */

	if (IS_RAWIO_DEV(dev)) {
		int	cookie;
		int	buf_flags;

		if (rw == UIO_READ) {
			buf_flags = B_PHYS|B_ASYNC;


			if (print_debug)
			printf("Calling useracc base 0x%x len 0x%x buf_flags %d\n",
					uio->uio_iov->iov_base,
						uio->uio_iov->iov_len, buf_flags);

			if (error = fast_useracc(uio->uio_iov->iov_base,
				     uio->uio_iov->iov_len, buf_flags, &cookie))
				return error;
		} else {
			buf_flags = B_PHYS|B_ASYNC;
			if (print_debug)
			printf("Calling Unuseracc base 0x%x len 0x%x buf_flags %d\n",
						uio->uio_iov->iov_base,
						uio->uio_iov->iov_len, buf_flags);

			fast_unuseracc(uio->uio_iov->iov_base, 
				uio->uio_iov->iov_len, buf_flags, &cookie);
		}

		return 0;
	}

	if (IS_RAWIO_DEV(dev)) {
		ulong	tm;
		int	i;
		int	cookie;
		int	buf_flags;

		if (rw == UIO_READ)
			buf_flags = B_READ;
		else
			buf_flags = B_WRITE;

		drv_getparm(LBOLT,&tm);
		cmn_err(CE_NOTE,"Start time ticks %x \n",tm);
		for ( i = 0; i < numdmas; i++) {
			if (error = fast_useracc(uio->uio_iov->iov_base,
				     uio->uio_iov->iov_len, buf_flags, &cookie))
				return error;

			fast_unuseracc(uio->uio_iov->iov_base, 
				uio->uio_iov->iov_len, buf_flags, &cookie);
		}
		drv_getparm(LBOLT,&tm);
		cmn_err(CE_NOTE,"End time ticks %x \n",tm);
		
		return 0;
	}
#endif /* RAWIO_DEBUG */


	/*
	 * Access /dev/mem or /dev/kmem one page at a time.
	 */
	error = 0;
	while (!error && uio->uio_resid != 0)  {
		register __psunsigned_t n, offset;
		char *npage;
		__psunsigned_t noffset;
		void *p;

		npage = 0;
		offset = uio->uio_offset;
		n = ctob(1) - (offset % ctob(1));
		if (n > uio->uio_resid)
			n = uio->uio_resid;

		/*
		 * Check address validity.
		 */
		if (IS_MEM_DEV(dev)) {
#ifdef MAPPED_KERNEL
			/* XXX - Should we just use the mapped address? */
			/* XXX - Should check for 32-bit K2 addresses too. */
			/*
			 * If the address passed in is a mapped kernel
			 * address, convert it to a K0 address.
			 * Even with replicated kernel text this is okay as
			 * long as we don't try to write it.
			 */
			if (IS_MAPPED_KERN_RO(offset))
				offset = MAPPED_KERN_RO_TO_K0(offset);
			else if (IS_MAPPED_KERN_RW(offset))
				offset = MAPPED_KERN_RW_TO_K0(offset);
#endif

			/*
			 * For /dev/mem, only allow access to real core.
			 * This may need to be more liberal.
			 */
			if (CKPHYSPNUM(pnum(offset)))
				return ENXIO;
#if IP19 || IP25
			error = ip19_memrw(uio,rw, offset, n);
			continue;
#else /* IP19 || IP25 */
#ifdef CELL_IRIX
			/*
			 * Temporary hack for cellular IRIX import/export.
			 * Problem is that this driver tries to do pfntopfdat
			 * calls for pages which have no pfdats.  This messes
			 * up the proxy pfdat code and causes pfdattopfn on
			 * these to return the wrong value.
			 */

			npage = page_mapin_pfn(NULL, VM_UNCACHED, 0, pnum(offset));
#else
			npage = page_mapin( pfntopfdat(pnum(offset)),
				VM_UNCACHED, 0);
#endif
#endif /* IP19 || IP25 */
		} else if (IS_KMEM_DEV(dev)) {
			/*
			 * For /dev/kmem, any access
			 * to anything valid is allowed.
			 */

			/*
			 * Allow user to specify kmem addresses as starting
			 * at 0 or 0x80000000.  File system will disallow 
			 * the latter. 
			 */
#if _MIPS_SIM == _ABI64
			/* For o32 programs, addresses are given to
			 * kmem in the range 0 .. 0x7fffffff (except for
			 * lseek64...).
			 * For n32 programs, assume o32 mode if addr < 1^32
			 */
			if ((ABI_IS_IRIX5(get_current_abi()) ||
			     ABI_IS_IRIX5_N32(get_current_abi())) &&
			    (offset < (1L << 32))) {
#if IP21
				__psunsigned_t temp1;

				/* If address is within range of CC chip then
				 * we must remap the address into the correct
				 * 64-bit physical address (can't use 32-bit
				 * address to access CC chip on large memory
				 * configurations).
				 */
				temp1 = offset & 0x3fffffff;
				if ((temp1 >= 0x18000000) &&
				    (temp1 < 0x20000000))
					offset = CC_ADDR(temp1);
				else
					offset |= K0BASE;
#else /* !IP21 */
				offset = (offset & TO_PHYS_MASK) | K0BASE;
#if defined (SN0)
				if (NASID_GET(offset) == 0)
				    offset = TO_NODE(master_nasid, offset);
#endif /* SN0 */

#endif /* IP21 */
			} else
				offset |= 0x8000000000000000;
#else	/* _ARBI64 */
			offset |= 0x80000000;
#endif	/* _ABI64 */
			if (offset + n < offset)
				return ENXIO;

			noffset = offset;

			/*
			 * Access to anything that isn't core memory
			 * has to go through the mmck interface.
			 */
			if (is_kmem_space((void *)offset, n) ) {
				error = mmck(offset, n, rw, uio);
				continue;
			}

#ifdef MAPPED_KERNEL
			/* XXX - Should we just use the mapped address? */
			/* XXX - Should check for 32-bit K2 addresses too. */
			/*
			 * If the address passed in is a mapped kernel
			 * address, convert it to a K0 address.
			 * Even with replicated kernel text this is okay as
			 * long as we don't try to write it.
			 */
			if (IS_MAPPED_KERN_RO(offset))
				offset = MAPPED_KERN_RO_TO_K0(offset);
			else if (IS_MAPPED_KERN_RW(offset))
				offset = MAPPED_KERN_RW_TO_K0(offset);
#endif

			/*
			 * In KSEG2, allow simple access only to cached pages.
			 * Uncached pages are likely to be devices which are
			 * likely to bus error.
			 */
			if (IS_KSEG2(offset)) {
				register pde_t *pde;

				if (pnum(offset - K2SEG) >= syssegsz)
					return ENXIO;
				pde = kvtokptbl(offset);
#ifdef MH_R10000_SPECULATION_WAR
				if (!pg_isvalid(pde))
#else /* MH_R10000_SPECULATION_WAR */
				if (!pg_ishrdvalid(pde))	/* fail on bad addr */
#endif /* MH_R10000_SPECULATION_WAR */
					return ENXIO;

				/* if uncached, only */
				if (pg_isnoncache(pde)) {
					error = mmck(offset,n,(uio_rw_t)-1,uio);
					continue;	/* check validity */
				}

				/* Do not allow cached reads to K2SEG (or K0)
				 * pages. This may break assumptions
				 * the kernel makes about the state
				 * of pages with respect to the cache.
				 * e.g. the kernel assumes that pages on
				 * the page cache clean free list really
				 * are cache clean.
				 * The idea is to prevent a process, doing
				 * random reads from /dev/kmem, from
				 * changing 'known' cache state. Writes
				 * to /dev/kmem are not changed to go
				 * uncached.
				 * 
				 * On Everest systems, VM_UNCACHED is 0,
				 * so the body of the if statement will
				 * never be executed.  This is intentional,
				 * since the page_mapin will in fact
				 * cause VCE's rather than avoid them
				 * (because VM_UNCACHED is not defined, and
				 * because we don't pass a color, the system
				 * will either return the KSEG0 address or
				 * allocate another page.  In either case,
				 * there is no way to ensure that the proper
				 * page coloring is maintained).
				 */
				if (VM_UNCACHED && rw == UIO_READ) {
					npage = page_mapin(pdetopfdat(pde),
							   VM_UNCACHED, 0);
					dcache_wb((void *)offset, n);
				}
			} else if (IS_KSEG0(offset)  || IS_KSEG1(offset)) {
#ifdef EVEREST
				/* Note that for machines with > 256M
				 * the CKPHYSPNUM macro is not sufficient,
				 * since K1 addresses >= 0xb0000000 will
				 * map to everest IO space, but may indeed
				 * have memory at the corresponding offset
				 * checked by CKPHYSPNUM. Hence, we need
				 * to check specifically if the address
				 * is in the everest IO range.
				 */
				if (IS_KSEG1(offset) && offset >= SWINDOW_BASE)
					return ENXIO;
#endif
				/*
				 * Notice bad physical addresses.
				 */
				if (CKPHYSPNUM(pnum(svirtophys(offset))))
					return ENXIO;

#if IP19 || IP25
				if (IS_KSEG0(offset)) {
					error = ip19_memrw(uio,rw, 
						   K0_TO_PHYS(offset), n);
					continue;
				}
#endif
				if (VM_UNCACHED && IS_KSEG0(offset) && 
				    rw == UIO_READ) {
					noffset = K0_TO_K1(offset);
					cache_operation((void *)offset, n, CACH_FORCE);
				}
			} else {
				/* e.g., KPTEBASE address */
				return ENXIO;
			}
		} else
			return ENXIO;

#ifdef IP32     /* avoidable ECC hit prevention */
                /* 
                 * Prevent ECC errors.
		 */

		if (!IS_KSEG2(offset)) {
			aPnum = pnum(svirtophys(offset));
		} else {
			aPnum = pdetopfn(kvtokptbl(offset));
		}

	        /* if the requested page is an ECC hit, use the noecc alias */
	        /* for this request we recalculate the data pointer based   */
	        /* on the pfn.                                              */
		if (pfntopfdat(aPnum)->pf_flags & P_ECCSTALE) {
			aPnum = eccpfn_to_noeccpfn(aPnum);
			/* release the previous npage if it exists */
			if (npage) {
				page_mapout(npage);
			}
			/* recalulate p  based on new pfn	 */
			npage = page_mapin(pfntopfdat(aPnum), VM_UNCACHED, 0);
		}

#endif  /* IP32 */   /* ECC hit prevention */

		/*
		 * Do the access.
		 */
		
                if (npage) {
			p = (void *)(npage+poff(offset));
		}
		else 
			p = (void *)noffset;


#ifdef _NO_UNCACHED_MEM_WAR
		if (!ip26_allow_ucmem && rw == UIO_WRITE && !IS_KSEG2(p)) {
			void *p2 = (void *)PHYS_TO_K0(KDM_TO_PHYS(p));

			error = uiomove(p2, n, rw, uio);
			if (p != p2) {
				dki_dcache_wbinval(p2, n);
			}
		}
		else
#endif
		error = uiomove(p, n, rw, uio);
		if (npage) {
			page_mapout(npage);
		}
	}
	return error;
}


#if IP19 || IP25
static caddr_t memrw_buffer = NULL;
static mutex_t memrw_mutex;
#endif

int
mminit()
{
#if IP19 || IP25
	mutex_init(&memrw_mutex, MUTEX_DEFAULT, "memrw");
	memrw_buffer = kvpalloc(1, VM_DIRECT, 0);
	if (memrw_buffer == NULL)
		return ENOMEM;
#endif /* IP19 || IP25 */

	return 0;
}

int
mmreg()
{
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "mem", "mm", &mem_dev);
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "kmem", "mm", &kmem_dev);
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "null", "mm", &null_dev);
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "mmem", "mm", &mmem_dev);

#if RAWIO_DEBUG
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "rawio", "mm", &rawio_dev);
	(void)hwgraph_char_device_add(GRAPH_VERTEX_NONE, "rawio_t", "mm", &rawiot_dev);
#endif /* RAWIO_DEBUG */

	hwgraph_chmod(null_dev, 0666);

	return 0;
}


#if IP19 || IP25
/*
 * Handling reads and writes of arbitrary pages is incredibly
 * complicated, primarily because of a weird mixture of bugs
 * in the R4400.  Because of a weird interaction between 
 * cached and uncached reads in MP systems, we cannot simply
 * fetch the page uncached the way other systems do (since trying
 * to do so could cause data corruption).  Therefore, we have to
 * use a cached reference.  Unfortunately, this presents us with
 * the problem of VCEs.  Normally, VCEs are fairly innocuous,
 * but you have to make sure that you do not take a VCE on your
 * own UAREA and KERNSTACK pages, since doing so will cause absolutely
 * horrible things to happen in the locore fault handlers.  Also,
 * you have to make sure that you don't contaminate the cache with
 * improperly colored pages when you're done with a page (otherwise,
 * you could cause some other process to take a VCE on its UAREA
 * at some undetermined period of time in the future).  To make sure
 * this doesn't happen, we flush the cache when we're done with it.
 * The only other thing you need to worry about is sleeping before
 * the cache gets flushed.  To avoid this possibility, we perform
 * any user copies either before we contaminate the cache or after
 * we flush the cache, so that if we sleep everything will be copacetic.
 */

static int
ip19_memrw(uio_t *uio, uio_rw_t rw, __psunsigned_t offset, __psunsigned_t n)
{
	__psunsigned_t npage;
	int error = 0;
	extern void __dcache_wb_inval(__psunsigned_t, __psunsigned_t);


	ASSERT(pnum(offset) == pnum(offset + n - 1));
	
	if (pnum(offset) == pg_getpfn(&curuthread->ut_kstkpgs[KSTKIDX]))
		npage = KERNELSTACK;
	else {
#if CELL_IRIX
		/*
		 * Temporary hack for cellular IRIX import/export.
		 * Problem is that this driver tries to do pfntopfdat
		 * calls for pages which have no pfdats.  This messes
		 * up the proxy pfdat code and causes pfdattopfn on
		 * these to return the wrong value.
		 */

		npage = (__psunsigned_t)
				  page_mapin_pfn(NULL, VM_VACOLOR,
						 colorof(offset), pnum(offset));
#else
		npage = (__psunsigned_t) 
			          page_mapin(pfntopfdat(pnum(offset)), 
					     VM_VACOLOR, colorof(offset));
#endif
	}

	/* Now transfer the data.  Since uiomove can sleep, we can't transfer
	 * directly, since if we sleep the cache will be contaminated.
	 */
	mutex_lock(&memrw_mutex, PZERO-1);

	if (rw == UIO_WRITE) {
		if (uiomove(memrw_buffer, n, UIO_WRITE, uio) == 0) {
		    	bcopy(memrw_buffer, (caddr_t) npage+poff(offset), n);
#if IP19
		        __dcache_wb_inval(npage+poff(offset), n);
#endif
		} else {
			error = EFAULT;
		}	
	} else {
		bcopy((caddr_t) npage + poff(offset), memrw_buffer, n);
#if IP19
		__dcache_wb_inval(npage+poff(offset), n);
#endif
		error = uiomove(memrw_buffer, n, UIO_READ, uio);
	}

	/* By the time we get here, the cache should be clean in all cases */
	mutex_unlock(&memrw_mutex);

	if (npage != KERNELSTACK)
		page_mapout((caddr_t) npage);
	
	return error;
}

#endif /* IP19 || IP25 */

mmioctl()
{
	return EINVAL;
}


extern struct mmmap_addrs mmmap_addrs[];

/* ARGSUSED */
int
mmmap(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
	register char *vaddr = (char *)off;

	/*
	 * Make sure that len isn't so big that it causes the address 
	 * range to wrap-around.
	 */

	if (vaddr + len < vaddr)
		return ENXIO;

	/*
	 * Mapping only allowed through /dev/mmem.
	 */
	if (IS_MMEM_DEV(dev)) {
		register struct mmmap_addrs *ma;
		auto int prot;
		int error;

		for (ma = &mmmap_addrs[0]; ma->m_addr; ma++) {
			if (vaddr >= (caddr_t)ma->m_addr &&
			    vaddr + len <= (caddr_t)ma->m_addr + ma->m_size) {
				if (error = v_getprot(vt, v_getaddr(vt),
								NULL, &prot))
					return error;
				if (prot & ~ma->m_prot)
					/* Requested unsupport protection */
					return EACCES;
				return v_mapphys(vt, vaddr, len);
			}
		}
		/*
		 * Check to see if this is the token for the cycle counter
		 * if it is then map the cycle counter page instead.
		 */
		if (((__psunsigned_t)vaddr & 0x00000000ffffffff) ==
		    (RTC_MMAP_ADDR & 0x00000000ffffffff)) {
			uint cycle;
			vaddr = (char *)query_cyclecntr(&cycle);
			return v_mapphys(vt, vaddr, len);
		}
		if (vaddr = (char *)verify_mappable_tstamp((paddr_t)vaddr &0x00000000ffffffff, len)) {
			return v_mapphys(vt, vaddr, len);
                }
	
		return ENXIO;
	}

	return ENODEV;
}

/*
 * Stub to avoid calling nodev() out of cdevsw[].
 */
/* ARGSUSED */
int
mmunmap(dev_t dev, vhandl_t *vt)
{
	return 0;
}
