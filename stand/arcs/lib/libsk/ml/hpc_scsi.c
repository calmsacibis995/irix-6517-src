#if IP20 || IP22 || IP26 || IP28		/* whole file */
/*
 * hpcscsi.c - Support for IP20 HPC and IP22/IP26/IP28 HPC3 scsi.
 *
 * $Revision: 1.29 $
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/sema.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/param.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <libsc.h>
#include <libsk.h>
#include "sys/scsi.h"   /* for scsidev.h typedefs */
#include "sys/scsidev.h"        /* for D_MAPRETAIN */


/* Imports */
extern int needs_dma_swap;
extern int cachewrback;

const scuzzy_t scuzzy[] = {
#if IP22 || IP26 || IP28
	{
		(u_char *)PHYS_TO_K1(SCSI0A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI0D_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI0_CTRL_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI0_BC_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI0_CBP_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI0_NBDP_ADDR),
                (unsigned int *)PHYS_TO_K1(HPC3_SCSI_DMACFG0),
                (unsigned int *)PHYS_TO_K1(HPC3_SCSI_PIOCFG0),
                D_PROMSYNC|D_MAPRETAIN,
                0x80
	},
	{
		(u_char *)PHYS_TO_K1(SCSI1A_ADDR),
		(u_char *)PHYS_TO_K1(SCSI1D_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI1_CTRL_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI1_BC_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI1_CBP_ADDR),
		(unsigned int *)PHYS_TO_K1(SCSI1_NBDP_ADDR),
                (unsigned int *)PHYS_TO_K1(HPC3_SCSI_DMACFG1),
                (unsigned int *)PHYS_TO_K1(HPC3_SCSI_PIOCFG1),
                D_PROMSYNC|D_MAPRETAIN,
                0x80
	}
#else
        {
                (u_char *)PHYS_TO_K1(SCSI0A_ADDR),
                (u_char *)PHYS_TO_K1(SCSI0D_ADDR),
                (ulong *)PHYS_TO_K1(SCSI0_CTRL_ADDR),
                (ulong *)PHYS_TO_K1(SCSI0_BC_ADDR),
                (ulong *)PHYS_TO_K1(SCSI0_CBP_ADDR),
                (ulong *)PHYS_TO_K1(SCSI0_NBDP_ADDR),
                (ulong *)PHYS_TO_K1(SCSI0_PNTR_ADDR),
                (ulong *)PHYS_TO_K1(SCSI0_FIFO_ADDR),
                D_PROMSYNC|D_MAPRETAIN,
                0x80
        }
#endif
};


/*	used to sanity check adap values passed in, etc.  maybe should
	assert somewhere that this is <= SC_MAXADAP, but that would mean
	including scsi.h and all it needs. */
#define NSCUZZY (sizeof(scuzzy)/sizeof(scuzzy_t))
/*
 * scsiinit - set up machine dependent SCSI parameters
 *
 */
void
scsiinit(void)
{
	extern int scsiburstdma, scsicnt;
	extern int *scsidev;
	extern uint scsi_syncenable[];
	char *scsiid;
	int adap;

#if IP22
	/* one on guinness, two on fullhouse. */
	scsicnt = is_fullhouse() ? 2 : 1;
#elif IP26 || IP28
	scsicnt = 2;
#else
	scsicnt = 1;			/* only one host adapter */
#endif 
	scsidev = 0;			/* force scsi re-init */
	scsiburstdma = 1;	

	if ((scsiid = getenv ("scsihostid")) && *scsiid) {
	    int id = *scsiid - 0x30;
	    extern u_char scsi_ha_id[];

	    if (id >= 0 && id <= 7)
		for (adap = 0; adap < SC_MAXADAP; adap++)
		    scsi_ha_id[adap] = id;
	}

#if IP22 || IP26 || IP28
        if (ip22_gio_is_33MHZ()) {
#ifdef IP22_WD95 
                *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG0) =
                        WD93_DMA_CFG_33MHZ;	
		if (scsicnt == 2)
		    *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG1) =
                        WD95_DMA_CFG_33MHZ;
#else
                *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG0) =
                        WD93_DMA_CFG_33MHZ;	
		if (scsicnt == 2)
		    *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG1) =
                        WD93_DMA_CFG_33MHZ;
#endif
        } else {
#ifdef IP22_WD95
                *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG0) =
                        WD93_DMA_CFG_25MHZ;
		if (scsicnt == 2)
		    *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG1) =
                        WD95_DMA_CFG_25MHZ;
#else
                *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG0) =
                        WD93_DMA_CFG_25MHZ;
		if (scsicnt == 2)
		    *(volatile u_int *)PHYS_TO_K1(HPC3_SCSI_DMACFG1) =
                        WD93_DMA_CFG_25MHZ;
#endif
	}
#endif /* IP22 || IP26 || IP28 */

#if IP22 || IP20 || IP26 || IP28
	for (adap = 0; adap < SC_MAXADAP; adap++) 
		scsi_syncenable[adap] = (uint)0xfe;
#endif

#ifdef IP22_WD95
	/* Farid- Later Check for board revision and probe gio bus for wd95
	   daugther board to call the correct driver/adapter. 
	 */
        for (adap = 0; adap < scsicnt; adap++) {
                scsiunit_init(adap);
		wd95_earlyinit(adap);
	}
#else
        for (adap = 0; adap < scsicnt; adap++) 
                scsiunit_init(adap);
#endif

}
/*
 * Reset the SCSI bus. Interrupt comes back after this one.
 * this also is tied to the reset pin of the SCSI chip.
 */
void
scsi_reset(int adap)
{
	*scuzzy[adap].d_ctrl = SCSI_RESET;
	flushbus();
	DELAY(25);	/* hold 25 usec to meet SCSI spec */
	*scuzzy[adap].d_ctrl = 0;	/* no dma, no flush */
}

/*
 * Set the DMA direction, and tell the HPC to set up for DMA.
 * Have to be sure flush bit no longer set.
 * dma_index will be zero after scsidma_map() is called, but may be
 * non-zero after dma_flush, when a target has re-connected partway
 * through a transfer.  This saves us from having to re-create the map
 * each time there is a re-connect.
 */
/*ARGSUSED*/
/* dma Rs or Ws should set active_mask to low , reset should be low before dma,
   endianess must be set low */
void
scsi_go(dmamap_t *map, caddr_t addr, int readflag)
{
	scuzzy_t *sc = (scuzzy_t *)&scuzzy[map->dma_adap];

	*sc->d_nextbp = (__psunsigned_t)
				((scdescr_t*)map->dma_addr + map->dma_index);
#if IP22 || IP26 || IP28
	*sc->d_ctrl = readflag ? SCSI_STARTDMA : (SCSI_STARTDMA|SCDIROUT);
#else
        *sc->d_ctrl = readflag ? (SCSI_STARTDMA|SCSI_TO_MEM) : SCSI_STARTDMA;
#endif
}

/*	allocate the per-device DMA descriptor list.  Arg is the per adapter
	dmamap
*/
dmamap_t *
scsi_getchanmap(dmamap_t *cmap)
{
	dmamap_t *map;
#if IP20
	caddr_t vaddr;
#endif

	if(!cmap)
		return cmap;	/* paranoia */

	/* use dmabuf_malloc because chip dmas from it, and also because
	 * IP22 needs double word alignment, which cache alignment
	 * guarantees.
	 * cache line align alignment also guarantees that no descriptor
	 * crosses a page boundary, for IP22. For others, may need
	 * to adjust.
	 *
	 * Use dmabuf_malloc() on IP26 just to be sure.
	 */
	map = (dmamap_t *)malloc(2*sizeof(*map));
#if IP22 || IP26 || IP28
	map[1].dma_virtaddr = 
	map->dma_virtaddr = dmabuf_malloc(sizeof(*map) + (1+NSCSI_DMA_PGS)
		* sizeof(scdescr_t));
#else
	vaddr = dmabuf_malloc(sizeof(*map) + (1+NSCSI_DMA_PGS)
		* sizeof(scdescr_t));
	map[1].dma_virtaddr = vaddr;
	map->dma_virtaddr = vaddr + ((IO_NBPP-io_poff(vaddr))
		% sizeof(scdescr_t));
#endif
	
	/* so we don't have to calculate each time in dma_map */
	/* always in k0 or k1 for standalone */
	map->dma_addr = KDM_TO_PHYS(map->dma_virtaddr);
	map->dma_adap = cmap->dma_adap;
	map->dma_type = DMA_SCSI;
	map->dma_size = NSCSI_DMA_PGS;
	return map;
}

void
scsi_freechanmap(dmamap_t *list)
{
	dmabuf_free((void *)list[1].dma_virtaddr);
	kern_free(list);
}

/*
 * Flush the fifo to memory after a DMA read.
 * OK to leave flush set till next DMA starts.  SCSI_STARTDMA bit gets
 * cleared when fifo has been flushed.
 * We 'advance' the dma map chain when we are called, so that when
 * some data was transferred, but more remains we don't have
 * to reprogram the entire descriptor chain.
 * When reading, this is only valid after the flush has completed.  So
 * we check the SCSI_STARTDMA bit to be sure flush has completed.
 *
 * Note that when writing, the curbp can be considerably off,
 * since the WD chip could have asked for up to 15 extra bytes,
 * plus the HPC could have asked for up to 64 extra bytes.
 * Thus we might not even be on the 'correct' descriptor any
 * longer when writing.  We must therefore advance the map index
 * by calculation.  For smaller code and simplicity, we do it for reads
 * also.  This is still faster than rebuilding the map, particularly in the
 * kernel.
 *
 * We also check for a parity error after a DMA operation.
 * return 1 if there was one
 * 
 * For HPC3 users, SCCH_ACTIVE_MASK should be set if writing
 * to any bits in d_ctrl other than SCSI_STARTDMA. If realy want to
 * set SCSI_STARTDMA hi then must set SCCH_ACTIVE_MASK low.
 *
 */
int
scsidma_flush(dmamap_t *map, int readflag, uint xferd)
{
	scdescr_t *sd;
	scuzzy_t *scp = (scuzzy_t *)&scuzzy[map->dma_adap];
#ifdef IP28		/* R10000_DMA_READ_WAR */
	uint fcnt;
#endif

#if IP22_WD95
	if(readflag) {
#else
	if(readflag && xferd) {
#endif
		unsigned cnt = 512;	/* normally only 2 or 3 times */

#if IP22 || IP26 || IP28
		*scp->d_ctrl |= (SCSI_FLUSH|SCCH_ACTIVE_MASK);
#else
		*scp->d_ctrl |= SCSI_FLUSH;
#endif
		while((*scp->d_ctrl & SCSI_STARTDMA) && --cnt)
			;
		if(cnt == 0)
			panic("SCSI DMA in progress bit never cleared\n");
#ifdef IP28		/* R10000_DMA_READ_WAR */
		/* index from last scsi_go */
		sd = (scdescr_t*)map->dma_virtaddr + map->dma_index;

		fcnt = xferd;
		while (fcnt > 0) {
			if (sd->eox == 1)
				break;

			if (fcnt < sd->bcnt)
				cnt = fcnt;
			else
				cnt = sd->bcnt;

			/* for standalone inval whole chunk */
			__dcache_inval((void *)PHYS_TO_K0(sd->cbp),cnt);

			fcnt -= cnt;
			sd++;
		}

		/* ok to keep descriptors cached as is consistant with mem */
#endif
	}
	else
		/* turn off dma bit; mainly for cases where drive 
		* disconnects without transferring data, and HPC already
		* grabbed the first 64 bytes into it's fifo; otherwise gets
		* the first 64 bytes twice, since it 'notices' that curbp was
		* reset, but not that is the same.  */
		*scp->d_ctrl = 0;
	if(!xferd)
		return 0;	/* disconnect with no i/o */

	/* index from last scsi_go */
	sd = (scdescr_t*)map->dma_virtaddr + map->dma_index;
#ifdef SCDES
if(map->dma_adap) printf("1st flsh: sd@%x cbp=0x%x bcnt=%d nbp=0x%x\n",sd, sd->cbp,sd->bcnt,sd->nbp);
#endif

	/* handle possible partial page in first descr */
	if(xferd >= sd->bcnt) {
#ifdef SCDES
if(map->dma_adap) printf("flsh %x xfrd(>=bcnt): sd@%x cbp=0x%x bcnt=%d nbp=0x%x\n", xferd, sd, sd->cbp,sd->bcnt,sd->nbp);
#endif
		xferd -= sd->bcnt;
		sd++;
	}
#ifdef SCDES
if(map->dma_adap) printf("done: hpc3 cbp=%x bc=%x ctrl=%x nbp=%x\n", 
		*(__uint32_t *)PHYS_TO_K1(SCSI1_CBP_ADDR),
		*(__uint32_t *)PHYS_TO_K1(SCSI1_BC_ADDR),
		*(__uint32_t *)PHYS_TO_K1(SCSI1_CTRL_ADDR),
		*(__uint32_t *)PHYS_TO_K1(SCSI1_NBDP_ADDR));
#endif
	if(xferd) {
		sd += xferd / IO_NBPP;
		sd->bcnt -= io_poff(xferd);
		sd->cbp += io_poff(xferd);
#ifdef SCDES
if(map->dma_adap) printf("flsh next (xferd=%x) sd@%x cbp=0x%x bcnt=%d nbp=0x%x\n", xferd, sd, sd->cbp,sd->bcnt,sd->nbp);
#endif
		/* flush descriptors back to memory so HPC gets right data */
		if (cachewrback)
			clear_cache((void *)sd, sizeof(*sd));

	}
	/* prepare for rest of dma (if any) after next reconnect */
	map->dma_index = (int)(sd - (scdescr_t *)map->dma_virtaddr);

#ifdef LATERON	/* still isn't working correctly */
	/* on transfers from memory, check to see if a parity error occured */
	if(!readflag && (*(volatile uint *)PHYS_TO_K1(PAR_ERR_ADDR) & PAR_DMA)) {
		readflag = *(volatile int *)PHYS_TO_K1(DMA_PAR_ADDR);
		printf("Parity error detected during SCSI DMA at 0x%x\n",
			readflag);
		/* Got a parity error, clear the parity err bit */
		*(volatile u_char *)PHYS_TO_K1(PAR_CL_ADDR) = 0;
		return(1);
	}
	else
#endif /* LATERON still isn't working correctly */
		return(0);
}


/*
 * allocate a DMA map region; return -1 if not SCSI
 */
/*ARGSUSED*/
dmamap_t *
dma_mapalloc(int type, int adap, int npage, int flags)
{
	static dmamap_t smap[NSCUZZY];

	if(type != DMA_SCSI || adap >= NSCUZZY)
		return((dmamap_t *)(__psint_t)-1);
	smap[adap].dma_adap = adap;
	smap[adap].dma_type = DMA_SCSI;
	return &smap[adap];
}


/*
 *	Map `len' bytes from starting address `addr' for a SCSI DMA op.
 *	Returns the actual number of bytes mapped.
 *
 *	4k pages for scsi hardware, so don't use system page size,
 *	which may be 4K or 16K.  XXX -- HPC3 supports 8K page DMAs!
 */
int
dma_map(dmamap_t *map, caddr_t addr, int len)
{
	uint bytes, npages, partial;
	uint paddr, psd;
	scdescr_t *sd;

	if((__psunsigned_t)addr & 0x3)
		return(0); /* can only deal with word aligned addresses */

	map->dma_index = 0;	/* for scsi_go */
	sd = (scdescr_t *)map->dma_virtaddr;
	psd = (uint)map->dma_addr;		/* see paddr comment below */

	if(bytes = (uint)io_poff(addr)) {
		if((bytes = IO_NBPP - bytes) > len) {
			bytes = len;
			npages = 1;
		}
		else {
			npages = (uint)io_btoc(len-bytes) + 1;
		}
	}
	else {
		bytes = 0;
		npages = (uint)io_btoc(len);
	}
	if(npages > map->dma_size) {
		npages = map->dma_size;
		partial = 0;
		len = (npages-(bytes>0))*IO_NBPP + bytes;
	}
	else if(bytes == len)
		partial = bytes;	/* only one descr, and that is part of a page */
	else {
		partial = (len - bytes) % IO_NBPP;/* partial page in last descr? */
	}

	/*  Both HPC 1.5 and 3.0 take phys addrs -- make sure we only have a
	 * 32 bit quanity to avoid illegal cast in 64 bit machines.
	 */
	paddr = KDM_TO_MCPHYS(addr);

#ifdef SCDES
if(map->dma_adap) printf("prog: bytes=%d npages=%d\n",bytes,npages);
#endif
	if(bytes) {
		sd->cbp = paddr;
		paddr += bytes;
		sd->bcnt = bytes;
		psd += sizeof(scdescr_t);
		sd->nbp = psd;
		sd->eox = 0;
#ifdef SCDES
if(map->dma_adap) printf("bytes: sd@%x cbp=0x%x bcnt=%d nbp=0x%x\n",sd, sd->cbp,sd->bcnt,sd->nbp);
#endif
		sd++;
		npages--;
	}
	/* flush descriptors back to memory so HPC gets right data */
	while(npages--) {
		unsigned incr;
		sd->cbp = paddr;
		incr = (!npages && partial) ? partial : IO_NBPP;
		paddr += incr;
		sd->bcnt = incr;
		psd += sizeof(scdescr_t);
		sd->nbp = psd;
		sd->eox = 0;
#ifdef SCDES
if(map->dma_adap) printf("pages: sd@%x cbp=0x%x bcnt=%d nbp=0x%x\n",sd, sd->cbp,sd->bcnt,sd->nbp);
#endif
		sd++;
	}

#if IP22 || IP26 || IP28
	/* hpc3 bug workaround.  Last descriptor is extra.  eox is only
	 * used when writing.
	 */
	(sd)->eox = 1;
	(sd)->bcnt = 0;
	sd++;			/* incr so we flush WAR descriptor */
#else
	(sd-1)->eox = 1;	/* only used when writing */
#endif

	/* flush descriptors back to memory so HPC gets right data */
	if (cachewrback)
#if R4000 || R10000
		clear_cache((void *)map->dma_virtaddr,
			(sd-(scdescr_t *)map->dma_virtaddr) * sizeof(*sd));
#else
		clean_dcache((void *)map->dma_virtaddr,
			(sd-(scdescr_t *)map->dma_virtaddr) * sizeof(*sd));
#endif

	return len;
}
#endif	/* IPXX */
