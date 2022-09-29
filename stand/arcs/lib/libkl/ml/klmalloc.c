/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: klmalloc.c
 * Very simple memory allocator. This initializes the memory for the KLCFG
 * data structure memory allocation. There are separate areas reserved for
 * different types of data structures and on every 'allocate request' the
 * next alined chunk of memory is 'allocated' and the OFFSET is returned.
 * These routines are used by IP27prom, IO6prom, MDK and FRU Analyzer.
 * The various types of data structures are:
 * lboard_t, klcompt_t, klerrinfo_t and klbigerrinfo_t.
 */

/*
 *

      +--------------------------+-------------+----------+----------+---------+
      |                          |             |          |          |         |
      |     Board                |  Component  |  Device  |  Errinfo | Nic     |
      |                          |             |          |          | strings |
      +--------------------------+-------------+----------+----------+---------+
      ^                  ^       ^
      |                  |       |
      +----------+       |       |
Board		base     |       |
		current -+
		limit -----------+
Component
Errinfo

 *
 */

#include <sys/types.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <libkl.h>


int klmalloc_size(int);
extern int bzero(void *, int);
void dump_klmalloc_table(nasid_t) ;
void init_klmalloc_device(nasid_t) ;

/*
 * This is called right at the beginning of XXXprom. It runs
 * on the CPU As of all nodes in MP.
 * Memory should be tested and valid. Inter nodal memory accesses
 * should be OK.
 * Normally, this routine is called by each node to do its local 
 * init, in which case, nasid should be INVALID_NASID.
 * If there is a node without CPUs then another node's CPU does the 
 * init, in which case a valid nasid should be used. 
 */


#define DEV_MHDR_OFF(o)	(((o + 7) & ~7) + 16)
#define KLMALLOC_ALIGN(o) DEV_MHDR_OFF(o)
#define GET_DEV_MHDR(m)	(klc_malloc_hdr_t *)\
			NODE_OFFSET_TO_K1(nasid,\
                   	DEV_MHDR_OFF(m[COMPONENT_STRUCT].km_limit)) ;

/* Reserve 3K of KLCONFIG space for NIC strings */
#define KLCFG_NIC_INFO_SIZE (3 * 1024) 
#define KLCFG_NIC_INFO_OFF(_n) KLMALLOC_ALIGN(KLCONFIG_OFFSET(_n) + \
				KLCONFIG_SIZE(_n) - KLCFG_NIC_INFO_SIZE)
#define KLCFG_NIC_INFO_NEXT_P(_n) ((klconf_off_t *)(NODE_OFFSET_TO_K1\
						(_n, KLCFG_NIC_INFO_OFF(_n))))
#define KLCFG_NIC_INFO_START(_n) ((klconf_off_t)KLCFG_NIC_INFO_OFF(_n) + 16)
#define KLCFG_NIC_INFO_CHECK(_n, _l) ((*KLCFG_NIC_INFO_NEXT_P(_n) + len) < \
				(KLCONFIG_OFFSET(_n) + KLCONFIG_SIZE(_n)))
#define KLCFG_ROUTER_NIC_P(_n)  (((__psunsigned_t)KLCFG_NIC_INFO_NEXT_P(_n)) + 8)

void
kl_config_hdr_init(nasid_t nasid)
{
	kl_config_hdr_t	klch ;

	KL_CONFIG_HDR_INIT_MAGIC(nasid);
	KL_CONFIG_HDR(nasid)->ch_cons_off = 
		((__psint_t)&(klch.ch_cons_info) - (__psint_t)&klch) ;
	KL_CONFIG_HDR(nasid)->ch_malloc_hdr_off = 
		((__psint_t)&(klch.ch_malloc_hdr) - (__psint_t)&klch) ;

	*KLCFG_NIC_INFO_NEXT_P(nasid) = KLCFG_NIC_INFO_START(nasid) ;
	*(__psunsigned_t *)KLCFG_ROUTER_NIC_P(nasid)    = 0 ;
}

void
init_config_malloc(nasid_t nasid)
{
	int size, i ;
	off_t next_offset;
	klc_malloc_hdr_t  *malloc_hdr;
	klc_malloc_hdr_t  *dev_mhdr;
	void *start;

	if (nasid == INVALID_NASID) 	/* self node init */
		nasid = get_nasid();

#if !defined (SABLE)
	/* bzero the config space. */
	start = (void *)KLCONFIG_ADDR(nasid);
	size = KLCONFIG_SIZE(nasid);

	db_printf("init_klcfg: nasid %x start %lx size %x\n",
		  nasid, start, size);
	bzero(start, size);
#endif


	kl_config_hdr_init(nasid);

	malloc_hdr = KL_CONFIG_CH_MALLOC_HDR(nasid);
	next_offset = KL_CONFIG_INFO_START(nasid);
	for (i = 0; i < KLMALLOC_TYPE_MAX; i++) {
		malloc_hdr[i].km_base = malloc_hdr[i].km_current = next_offset;
		if ((size = klmalloc_size(i)) < 0) size = 0;
		if ((malloc_hdr[i].km_base + size) > 
		    (KLCONFIG_OFFSET(nasid) + KLCONFIG_SIZE(nasid) - KLCFG_NIC_INFO_SIZE)) {
			printf("init_klcfg:type %d exceeds klconfig size\n", i);
			malloc_hdr[i].km_limit = malloc_hdr[i].km_base;
		}
		else {
			malloc_hdr[i].km_limit = malloc_hdr[i].km_base + size;
			next_offset += size;
		}
	}

	/* 
	   Reduce the size of COMPONENT_STRUCT area by 2 so that
           we can allocate for devices from the second half and
	   reset its value in the IO6prom.
	 */
	malloc_hdr[COMPONENT_STRUCT].km_limit = 
	malloc_hdr[COMPONENT_STRUCT].km_base +
	((malloc_hdr[COMPONENT_STRUCT].km_limit - 
	malloc_hdr[COMPONENT_STRUCT].km_base) / 2) ;

	/* Get pointer to device malloc area struct */
        dev_mhdr = GET_DEV_MHDR(malloc_hdr) ;
	dev_mhdr->km_base = 
	dev_mhdr->km_current = DEV_MHDR_OFF(malloc_hdr[COMPONENT_STRUCT].km_limit)
			    + 128 ; /* size of klc_malloc_hdr_t + padding */
	dev_mhdr->km_limit = malloc_hdr[ERRINFO_STRUCT].km_base - 16 ;

#if DEBUG
	dump_klmalloc_table(nasid) ;
#endif
}


int
klmalloc_size(int type)
{
	unsigned int size;

	switch (type) {
	case BOARD_STRUCT:
		size = (MAX_SLOTS_PER_NODE * sizeof(lboard_t)) ;
		break;

	case COMPONENT_STRUCT:
		/*
		 * Dont reserve for all components. We dont have so
		 * much memory. Just reserve for 1/4 of max. The allocation
		 * will warn us if we run out.
		 */
		/* Reserve for 1/2 of max. We still have some space in
		   the 48 K reserved for KLCONFIG and we are seriously
		   building large systems for FCS.
		   Also, if we have to tune MAX_COMPTS_PER_BRD value
		   then this will have enough space for everyone.
		 */
		size = ((MAX_SLOTS_PER_NODE * (MAX_COMPTS_PER_BRD >> 1))
			* sizeof(klcomp_t));
		break;

	case ERRINFO_STRUCT:
		size = ((MAX_SLOTS_PER_NODE * sizeof(klerrinfo_t)) +
			(MAX_SLOTS_PER_NODE * MAX_COMPTS_PER_BRD *
			 sizeof(klgeneric_error_t)));
		break;

#if 0
	case DEVICE_STRUCT:
		size = ( 6 * 4 * 10 * sizeof(kldev_t)) ;
		/* 
		   MAX_IO_BOARDS_PER_NODE = 6 
		   MAX_PCI_CTLR_PER_IO    = 4
		   MAX_DEVICE_PER_CTLR    = 4
		*/
		break ;
#endif
	default:
		printf("Unknown malloc type %d\n", type);
		size = -1;
		break;
	}
	return size;
}




off_t 
kl_config_info_alloc(nasid_t nasid, int type, unsigned int size)
{
	off_t  retval ;
	klc_malloc_hdr_t *malloc_hdr;
	klc_malloc_hdr_t *dev_mhdr;

	if (nasid == INVALID_NASID) /* allocate on the current node */
	    nasid = get_nasid();

	if (!KL_CONFIG_CHECK_MAGIC(nasid)) {
		printf("kl_config_info_alloc: bad magic %x, nasid %d\n",
		       KL_CONFIG_MAGIC(nasid), nasid);
		return -1;
	}
	malloc_hdr = KL_CONFIG_CH_MALLOC_HDR(nasid);

	/* 
	 * pad this size with a couple of double words.
	 * This makes structure changes/additions a little easier to
	 * handle
	 */
	size = ((size + 7) & ~7) + 16;

	switch (type) {
	case BOARD_STRUCT:
	case COMPONENT_STRUCT:
	case ERRINFO_STRUCT:
		retval = malloc_hdr[type].km_current;
		if ((retval + size) < malloc_hdr[type].km_limit) {
			malloc_hdr[type].km_current += size;
		}
		else {
			retval = -1;
		}
		break;
		
	case DEVICE_STRUCT:
		dev_mhdr = GET_DEV_MHDR(malloc_hdr) ;

		retval = dev_mhdr->km_current ;
                if ((retval + size) < dev_mhdr->km_limit) {
                        dev_mhdr->km_current += size;
                }
                else {
                        retval = -1;
                }

		break ;

	default:
		retval = -1;
		break;
	}
	
	return (retval) ;
}

void
dump_klmalloc_table(nasid_t nasid)
{
	int i ;
	klc_malloc_hdr_t *mh, *devmh ;

	printf("**************KLMALLOC TABLE****************\n") ;

        mh = KL_CONFIG_CH_MALLOC_HDR(nasid);

	printf("index	base	limit	current	size\n") ;

 	for (i=0;i< KLMALLOC_TYPE_MAX ; i++) {
		printf("%d	0x%x	0x%x	0x%x	%d\n",
		i, mh[i].km_base, mh[i].km_limit, mh[i].km_current, 
		(mh[i].km_limit - mh[i].km_base)) ;
	}
        devmh = GET_DEV_MHDR(mh) ;
        printf("%d      0x%x    0x%x    0x%x    %d\n",
                DEVICE_STRUCT, devmh->km_base, devmh->km_limit, devmh->km_current,
                (devmh->km_limit - devmh->km_base)) ;
	
	printf("NIC INFO START = 0x%lx\n", *KLCFG_NIC_INFO_NEXT_P(nasid)) ;
	printf("ROUTER NIC INFO PTR = 0x%lx\n", KLCFG_ROUTER_NIC_P(nasid)) ;

}

void 
init_klmalloc_device(nasid_t nasid) 
{
        int i ;
        klc_malloc_hdr_t *mh, *devmh ;

        mh = KL_CONFIG_CH_MALLOC_HDR(nasid);

        devmh = GET_DEV_MHDR(mh) ;

        devmh->km_current = devmh->km_base ; /* reset the value */
}

static void
klmalloc_nic_set_next(nasid_t nasid, int len)
{
	(*KLCFG_NIC_INFO_NEXT_P(nasid)) = 
	KLMALLOC_ALIGN(*KLCFG_NIC_INFO_NEXT_P(nasid) + len) ;
}

klconf_off_t
klmalloc_nic_off(nasid_t nasid, int len)
{
	klconf_off_t rv ;

	if (KLCFG_NIC_INFO_CHECK(nasid, len)) {
		rv = (*KLCFG_NIC_INFO_NEXT_P(nasid)) ;
		klmalloc_nic_set_next(nasid, len) ;
	}
	else
		rv = NULL ;

	return rv ;
}

klconf_off_t
get_klcfg_router_nicoff(nasid_t nasid, int len, int flag)
{

	if (flag) /* Allocate a new nic buffer */
		*(klconf_off_t *)KLCFG_ROUTER_NIC_P(nasid) = klmalloc_nic_off(nasid, len) ;
	return(*(klconf_off_t *)KLCFG_ROUTER_NIC_P(nasid)) ;
}


