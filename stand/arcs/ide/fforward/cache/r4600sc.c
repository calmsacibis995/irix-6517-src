/*
 * r4600sc.c
 *
 *
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#if IP22

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include "cache.h"

static jmp_buf fault_buf;
static u_int sc_save_area[0x200];
static u_int tlb_index_save; 
extern u_int            memsize;
extern u_int _r4600sc_sidcache_size;

void tlbwired_sc(int, int, unsigned int, unsigned int, unsigned int);

/*
 * is_r4600sc - check to see if running on r4600sc and return
 * result for use by mfg scripts
 *
 */

u_int
sc_present(void)
{

	if(! _r4600sc_sidcache_size)
		return(0);

	return(1);
}

/*
* 4600sc - basic interactive test routines
* 	enable_scache() - also invalidates
*	disable_scache() 
*	invalidate_line(addr) - addr = index of line
*/

u_int
flush_pd_cache(void)
{

	int index, i;
	u_int *base_addr;

	base_addr = (u_int *)(SC_BASE);
	msg_printf(DBG,"flush primary starting from %x\n",base_addr);

	for(i=0; i < (SC_SIZE/4); i += 0x08)
		pd_iwbinv(base_addr + i);
	msg_printf(DBG,"last addr = %x\n",(base_addr + i));
}

u_int
inv_pd_cache(void)
{

	int index, i;
	u_int *base_addr;

	base_addr = (u_int *)(SC_BASE);
	msg_printf(DBG,"invalidate primary starting from %x\n",base_addr);

	for(i=0; i < (SC_SIZE/4); i += 0x08)
		pd_HINV(base_addr + i);
}

u_int
sc_restore_vectors(void)
{
	u_int *p1;
	int i;

	p1 = (u_int *)PHYS_TO_K1(0);

	for (i=0; i<0x200; i++)
		*(p1+(i/4)) = sc_save_area[i];
}

u_int
sc_save_vectors(void)
{
	u_int *p1;
	int i;

	p1 = (u_int *)PHYS_TO_K1(0);

	for (i=0; i<0x200; i++)
		sc_save_area[i] = *(p1+(i/4));
}


/* to enable need to generate an address with bit<31> set 
*  write one wired TLB entry to map va 0xe0000000 to physad 0xe000000 
* and use it to enble the cache
*
* also map 1 meg of kuseg space for cache testing and uncache kseg0
*/
u_int
enable_scache(void)
{

	char *scache_enable;
	u_int tlblo0, tlblo1, tlblo_attribute;
	u_int vaddr, paddr, temp, offset1, offset2;
	u_int *p1, *p2;
	int index;

	set_pgmask(SC_PAGEMASK);	/* 256k size pages */

	/* map sc control */
	tlblo_attribute = (TLBLO_D | TLBLO_V | TLBLO_G | TLBLO_UNCACHED);
	vaddr = SC_CNTRL_VADDR;	/* virtual address of scache control */
	paddr = SC_CNTRL_PADDR;	
	
	tlblo0 = ((paddr >> 6) | tlblo_attribute);
	tlblo1 = (((paddr+SC_PAGESZ) >> 6) | tlblo_attribute);

	msg_printf(DBG,"tlblo0 = 0x%x\n",tlblo0);
	msg_printf(DBG,"tlblo1 = 0x%x\n",tlblo1);

	tlbwired_sc(0, 0, vaddr, tlblo0, tlblo1); /* map cache control addr */
		/* note that the wired register is not set by this routine */
	scache_enable = (char *)vaddr; /* init control ptr */

	/* map all the kuseg space needed in 256K pages */
	tlblo_attribute = (TLBLO_D | TLBLO_V | TLBLO_G | TLBLO_NONCOHRNT);

	vaddr = SC_BASE;	/* virtual address of scache */
	paddr = SC_BASE;	
	
	tlblo0 = ((paddr >> 6) | tlblo_attribute);
	tlblo1 = (((paddr+SC_PAGESZ) >> 6) | tlblo_attribute);

	msg_printf(DBG,"tlblo0 = 0x%x\n",tlblo0);
	msg_printf(DBG,"tlblo1 = 0x%x\n",tlblo1);

	tlbwired_sc(1, 0, vaddr, tlblo0, tlblo1); /* map cache control addr */

	vaddr = SC_BASE + (SC_PAGESZ * 2);/* virtual address of scache */
	paddr = SC_BASE + (SC_PAGESZ * 2);
	
	tlblo0 = ((paddr >> 6) | tlblo_attribute);
	tlblo1 = (((paddr+SC_PAGESZ) >> 6) | tlblo_attribute);

	msg_printf(DBG,"tlblo0 = 0x%x\n",tlblo0);
	msg_printf(DBG,"tlblo1 = 0x%x\n",tlblo1);

	tlbwired_sc(2, 0, vaddr, tlblo0, tlblo1); /* map cache control addr */


	index = 3;	/* start with tlb entry 3 */	
	/* map all addrs with two bits set in tag for tag test */
	for(offset1 = SC_OFFSET; offset1 < memsize; offset1 = offset1<<1)
	{
	   for(offset2 = SC_OFFSET; offset2 < memsize; offset2 = offset2<<1)
	   {
		
		vaddr = (SC_BASE | offset1 | offset2);	/* scache test addrs */
		paddr = (SC_BASE | offset1 | offset2);	
		tlblo0 = ((paddr >> 6) | tlblo_attribute);
		tlblo1 = (((paddr+SC_PAGESZ) >> 6) | tlblo_attribute);
	
		msg_printf(DBG,"tlblo0 = 0x%x\n",tlblo0);
		msg_printf(DBG,"tlblo1 = 0x%x, index = %x\n",tlblo1,index);

		tlbwired_sc(index++, 0, vaddr, tlblo0, tlblo1);
	   }
	}

	tlb_index_save = index;
	/* flush primary data cache, turn off kseg0 cacheing, 
		turn on secondary cache */

	p1 = (u_int *) (SC_BASE);
	p2 = (u_int *) (SC_BASE + SC_SIZE);
	for(;p1<p2;p1+=4)	/* read in every fourth word */
		temp &= *p1;	/* to flush out and clean pd cache */
	
	flush_pd_cache(); 
	uncache_K0();	/* turn off cacheing for kseg0 */

	*scache_enable = 0;		/* byte write enables cache */
	return(0);			/* should have setjmp return also */
}
 
/***************************************************************************
*	disable the cache, clear out tlb and turn on kseg0 cacheing
*
***************************************************************************/

u_int
disable_scache(void)
{

	int i;
	short *scache_disable;

	scache_disable = (short *) SC_CNTRL_VADDR;

	*scache_disable = 0; /* halfword write disables caceh */
	flush_pd_cache(); 
	for(i=0;i<tlb_index_save;i++) /* invlaidate tlb entries used */
	{
	  invaltlb(i);
	}

	set_pgmask(TLBPGMASK_MASK); /* restore page mask register */

	cache_K0();	/* restore K0 space cacheing */

	return(0);

}

u_int
inv_line(int index)
{

	u_int *inv_line;

	if (index & 0xfff80000)
	{
		msg_printf(VRB,"Index %x out of range\n", index);
		return(-1);		/* offset out of range */
	}

	inv_line = (u_int *)(SC_CNTRL_VADDR | index);
					 /*or in index for line index */

	*inv_line = 0;	/*word write invalidates line at index offset */
	return(0);


}


u_int
invalidate_line_scache(int argc, char *argv[])
{

	int index;

	if (argc != 2)
		return(-1);
	index = atoi(argv[1]);

	return(inv_line(index));


}
u_int
write_block(int argc, char *argv[])
{

	u_int addr;

	if (argc != 2)
		return(-1);

	addr = atoi(argv[1]);

	/* fill line to insure an I-cache hit */
	fill_ipline((volatile uint *)addr);
	write_ipline((volatile uint *)addr);
	return(0);
}		

u_int
read_block(int argc, char *argv[])
{
	u_int addr;

	if (argc != 2)
		return(-1);

	addr = atoi(argv[1]);

	fill_ipline((volatile uint *)addr);
	return(0);
}
u_int
sc_mem_test(int argc, char *argv[])
{

	register u_int	*p1, *p2, *base;
	register u_int	i, pattern[4];
	u_int	status = 0;

	sc_save_vectors();

	if (argc == 3)
	{
		p1 = base = (u_int *)atoi(argv[1]);
		p2 = (u_int *)atoi(argv[2]);
	}
	else
	{
		p1 = base = (u_int *)(SC_BASE);
		p2 = (u_int *)(SC_BASE+SC_SIZE);
	}
	msg_printf(VRB,"Start addr = %x, end addr = %x\n",p1,p2);
		

	pattern[0] = 0x12345678;
	pattern[1] = 0xedcba987;
	pattern[2] = 0xaa5533cc;
	pattern[3] = 0x55aacc33;

	for(; p1<p2; p1+=4)
	{
	  for(i=0; i<4; i++)
	  {
	    p1[i]=pattern[i];
	  }
	}	

	flush_pd_cache();

	p1 = base;

	for(; p1<p2; p1+=4)
	{
	  for(i=0; i<4; i++)
	  {
	    if(p1[i] != pattern[i])
	    {
		msg_printf(ERR,"Error at %x, expect = %x, actual = %x\n",
			(p1+(i*4)),pattern[i],p1[i]);
		status=-1;
	    }
	  }
	}

	sc_restore_vectors();

	return(status);
}
u_int
sc_hit_test(int argc, char *argv[])
{

	register u_int	*p1, *p2;
	u_int	status = 0, done = FALSE;

	sc_save_vectors();

	/* write to memory and initialize the cache contents */
	p1 = (u_int *) (SC_BASE);
	p2 = (u_int *) (SC_BASE + SC_SIZE);
	msg_printf(VRB,"test from %x to %x\n",p1,p2);

	for(; p1<p2; p1++)
	{
	    *p1= (u_int)p1;	/* write the address as data */
	}	
	flush_pd_cache();  /* invalidate and flush the primary cache */

	/* now write behind the cache thru uncached alias */
	p1 = (u_int *) PHYS_TO_K1(SC_BASE);
	p2 = (u_int *) PHYS_TO_K1(SC_BASE + SC_SIZE);

	for(; p1<p2; p1++)
	{
	    *p1= ~(u_int)p1;	/* write the address complement as data */
	}	

	/* loop forever on cache reads, a miss will return the wrong data */
	while(!done || (argc >2 && !status))
	{
	busy(1);
	p1 = (u_int *) (SC_BASE);
	p2 = (u_int *) (SC_BASE + SC_SIZE);
	for(; p1<p2; p1++)
	{
	    if(*p1 != (u_int)p1)
	    {
		msg_printf(ERR,"Error at %x, expect = %x, actual = %x\n",
			p1,p1,*p1);
		status=-1;
	    }
	}
	done = TRUE;
	} /*end while*/

	sc_restore_vectors();

	busy(0);
	return(status);
}
u_int
sc_hit_n_miss(int argc, char *argv[])
{

	volatile register u_int	*p1, *p2;
	register u_int	temp; 
	u_int	offset, status = 0, done = FALSE;

	sc_save_vectors();

	/* initialize phys addr for inv */
	offset = 0;

	while(!done || (argc > 2 && !status))
	{
	busy(1);
	/* write to memory and initialize the cache contents */
	p1 = (u_int *) (SC_BASE);
	p2 = (u_int *) (SC_BASE + SC_SIZE);

	for(; p1<p2; p1++)
	{
	    *p1= (u_int)p1;	/* write the address as data */
	}	

	flush_pd_cache();

	/* now write behind the cache thru uncached alias - KSEG1 */
	p1 = (u_int *) PHYS_TO_K1(SC_BASE);
	p2 = (u_int *) PHYS_TO_K1(SC_BASE + SC_SIZE);

	temp = (SC_BASE); /* KUSEG addr as data */

	for(; p1<p2; p1++)
	{
	    *p1= ~temp;	/* write the K0 address complement as data */
	    temp += 4;
	}	

	/* invalidate one line in the cache */
	if (inv_line(offset))
		msg_printf(VRB,"failed to inv line at %x\n",offset);

	/* do cache reads, a miss will return the wrong data */
	p1 = (u_int *) (SC_BASE);
	p2 = (u_int *) (SC_BASE + SC_SIZE);
	for(; p1<p2; p1++)
	{
	    temp = *p1;
	    if(offset != ((u_int)p1 & 0x0007ffe0)) /* check for inv line */
	    {
		if(temp != (u_int)p1)
		{
		   msg_printf(ERR,"Error at %x, expect = %x, actual = %x\n",
			p1,p1,temp);
		   status=-1;
		}
	     }
	     else /* for inv line, miss gets addr complement as data */
	     {
		   if(temp != ~(u_int)(p1))
		   {
		      msg_printf(ERR,"Error at %x, expect = %x, actual = %x\n",
			p1,~(u_int)p1,temp);
		    /*  status=-1;  */
		   }
	     }
	}
	switch(argc)
	{
	case 3:
		if(offset <= 0)
			offset += 0x7ffe0;
		else
			offset -= 0x20;
		
		break;
	case 2:
		offset += 0x20;
		break;
	default:
		if(offset == 0)
			offset += 0x20;
		else
			offset = offset << 1;
		break;

	} /*end switch*/

	if(offset >= (0x0007ffff))
	{
		offset = 0; 
		done = TRUE;
	}
	} /*end while*/

	sc_restore_vectors();

	busy(0);
	return(status);
}

u_int
sc_miss_test(int argc, char *argv[])
{

	register u_int	*p1, *p2;
	u_int	temp, status = 0, done = FALSE;

	sc_save_vectors();

	p1 = (u_int *) (SC_BASE);
	p2 = (u_int *) (SC_BASE + (SC_SIZE * 2));


	for(; p1<p2; p1++)
	{
	    *p1 = (u_int)p1;
	}	

	while(!done || (argc>2 && !status))
	{
	busy(1);
	p1 = (u_int *) (SC_BASE);
	for(; p1<p2; p1++)
	{
	    temp = *p1;	/* save the first read from this address */
	    if(temp != (u_int)p1)
	    {
		msg_printf(ERR,"Error at %x, expect = %x, actual = %x\n",
			p1,p1,temp);
		status=-1;
	    }
	}
	done = TRUE;
	} /*end while*/

	sc_restore_vectors();
	busy(0);

	return(status);
}
/************************************************************************/
/******** this dosn't work, no exceptions generated when running ide ***/
u_int sc_probe_tag(u_int *base_addr)
{
        jmp_buf faultbuf;
	u_int temp, pattern = 0x87654321;


        if (setjmp(faultbuf)) /* expect exception on 1st access */
	{
		msg_printf(DBG,"write\n");
		if (setjmp(faultbuf))
	   	{
		   msg_printf(DBG,"read\n");
               	   nofault = 0;
               	   return(FALSE);
	   	}

		temp = *base_addr;
		nofault = 0;
		return(TRUE);
	}
        nofault = faultbuf;

	msg_printf(DBG,"write to %x\n",base_addr);

        *base_addr = pattern;	
	pd_iwbinv(base_addr);	/*write back from primary cache*/
	/* always end up here, no exceptions generated */
        nofault = 0;
        return(FALSE);
}



u_int
sc_tag_ram(int argc, char *argv[])
{

	register u_int	*p1, *p2;
	u_int	*pbase, offset1, offset2, pattern, temp, status=0, done=FALSE;

	sc_save_vectors();

	pattern = 0x87654321;	/* a unique (hopefully) test pattern */

	pbase = (u_int *)(SC_BASE);

	for(offset1 = SC_OFFSET; offset1<memsize; offset1 = offset1 << 1)
	{
		p1 = (u_int *)(SC_BASE | offset1);
		p2 = (u_int *) PHYS_TO_K1(SC_BASE | offset1);
		temp = *p1;
		*p1 = pattern;  /* special test pattern */
		 pd_iwbinv(p1);  /* flush the primary cache */

		/* check for all possible wrong hits in tag ram */

		/* check for offset bit stuck at zero */
		if (*pbase == pattern) /* tag = 0 */
		{
		   msg_printf(ERR,"Tag alias at 0 for %x\n",
						(SC_BASE | offset1));
		   status = -1;
		 }

		
		/* check for right data at wrong addr */
		for(offset2 = SC_OFFSET; offset2<memsize; offset2 = offset2<<1)
		{
		
		/* read from an address with one other bit set in the tag */
		   p1 = (u_int *)(SC_BASE | offset1 | offset2);
		   msg_printf(DBG,"check tag addr %x\n",p1);
		   if ((*p1 == pattern) && 
				(offset1 != offset2))	
		   {
			msg_printf(ERR,"Tag alias at %x for %x\n",
						p1,(SC_BASE | offset1));
			status = -1;
		   }

		}/* end for */

		p1 = (u_int *)(SC_BASE | offset1);
		pd_iwbinv(p1);  /* flush the primary cache */
		*p2 = temp;	/* restore the original value */

	}/* end for */	

	/* now check beyond the end of memsize */
	/* this dosn't work, no exceptions generated when running ide */
/************************************************************************
*	for(;offset1<SC_TAG_SIZE;offset1=offset1<<1)
*	{
*
*		p2 = p1 + (offset1/4);
*		status = sc_probe_tag(p2);
*		if(!status)
*		  msg_printf(ERR,"Tag error at offset %x\n",
*				offset1);
*	}
************************************************************************/
	sc_restore_vectors();

	return(status);
}

#endif	/* IP22 */
