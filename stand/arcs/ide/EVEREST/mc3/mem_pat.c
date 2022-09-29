
/*
 * mc3/mem_pat.c
 *
 *
 * Copyright 1991, 1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.32 $"

/*
 * mem_pat -- 	mem_pat [-bhw] [-r] [-l] [-c] [-v 0xpattern] RANGE  
 *
 *   This is a traditional, hueristic, memory test . User can specify
 *   the first address, last address and test pattern.
 *		
 *   Nearly immediately after detecting a miscompare, the expected data,
 *   actual data and the address at an error occured are displayed.
 *
 *  INPUTS:  first_address - address of the first meory location to be tested
 *           last_address - address of the last 32 bit word to be tested.
 *	     pattern - test pattern.
 *  OUTPUTS: expected_data - the expected data
 *           actual_data - the data value actually returned
 *           ret_error_addr - addreess at an error occured
 *  RETURNS: 0 if the memory has passed the test
 *           1 if an error was detected 
 *
 */

#include <sys/types.h>
#include <sys/cpu.h>
#ifdef IP19
#include "ip19.h"
#elif IP21
#include "ip21.h"
#elif IP25
#include "ip25.h"
#endif
#include "pattern.h"
#include "mc3_reg.h"
#include "libsk.h"
#include "exp_parser.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/EVEREST/evconfig.h>
#include "uif.h"
#include "prototypes.h"

extern char *atob_L();
extern char *atob();
extern uint mem_slots;
extern long long vir2phys(__psunsigned_t);
extern int parse_addr(register char *, struct mc3_range *);
jmp_buf mempat_buf;
static int valspecd = 0;

void fillmem_ch(__psunsigned_t chlo, __psunsigned_t chhi, unsigned int mask, 
		int size, uint value)
{
	__psunsigned_t chptr;
	__psunsigned_t badaddr;
	unsigned int badbits;	
	int slot;
	
	for (chptr = chlo; chptr < chhi; chptr += size) {
		if (valspecd)
			*(char *)chptr = value & mask;
		else
               		*(char *)chptr = (char)(chptr & mask);
/*		if ((chptr % 0x1000) == 0)
       			if (cpu_intr_pending()) {
			   badaddr = vir2phys(chptr);
                           decode_address(badaddr,0, SW_WORD, &slot);
                           err_msg(MC3_INTR_ERR, &slot, badaddr);
			   if (chk_memereg(mem_slots)) {
				check_memereg(mem_slots);
				mem_err_clear(mem_slots);
			   }
			}
*/
	        msg_printf(VRB,"Writing 0x%x to addr 0x%llx\r", 
			*(char *)chptr, chptr);
	}
        for (chptr = chlo; chptr < chhi; chptr += size) {
		if (valspecd)
		{
			if (*(char *)chptr != (value & mask))
			{
			    badbits = *(char *)chptr ^ (char)(chptr & mask);
			    decode_address(K1_TO_PHYS(chptr),badbits, SW_BYTE, &slot);
			    err_msg(MEM_PAT_ERR,&slot, chptr,value & mask,
                                    *(char *)chptr);
			    if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
			    }
			}
                }
		else
		{
		    if (*(char *)chptr != (chptr & mask))
		    {
			  	badbits = *(char *)chptr ^ (char)(chptr & mask);
        		  	decode_address(K1_TO_PHYS(chptr), badbits, SW_BYTE, &slot);
			  	err_msg(MEM_PAT_ERR,&slot, chptr,
					chptr & mask, *(char *)chptr);
				if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
				}
			}
              	}
        }    
}           

void
fillmem_shi(__psunsigned_t shilo, __psunsigned_t shihi, 
	unsigned int mask, int size, uint value)
{
	__psunsigned_t shiptr;
	__psunsigned_t badaddr;
	unsigned int badbits;	
	int slot;
	
	for (shiptr = shilo; shiptr < shihi; shiptr += size) {
		if (valspecd)
			*(short int *)shiptr = value & mask;
		else
			  *(short int *)shiptr = (short int)(shiptr & mask);
/*		if ((shiptr % 0x1000) == 0)
       			if (cpu_intr_pending()) {
			   badaddr = vir2phys(shiptr);
                           decode_address(badaddr,0, SW_WORD, &slot);
                           err_msg(MC3_INTR_ERR, &slot, badaddr);
			    if (chk_memereg(mem_slots)) {
				check_memereg(mem_slots);
				mem_err_clear(mem_slots);
			    }
			}
*/
	        msg_printf(VRB,"Writing 0x%x to addr 0x%llx\r", 
			*(short int *)shiptr, shiptr);
	}
        for (shiptr = shilo; shiptr < shihi; shiptr += size) {
		if (valspecd)
		{
			if (*(short int *)shiptr != value & mask)
			{
			    badbits = *(short int *)shiptr ^ (short int)(shiptr & mask);
			    decode_address(K1_TO_PHYS(shiptr),badbits, SW_BYTE, &slot);
                       	    err_msg(MEM_PAT_ERR,&slot, shiptr,value & mask,
                                                        *(short int *)shiptr);
				if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
				}
			}
                }
		else
		{
		    if (*(short int *)shiptr != (shiptr & mask))
		    {
			  	badbits = *(short int *)shiptr ^ (short int)(shiptr & mask);
        		  	decode_address(K1_TO_PHYS(shiptr),badbits, SW_BYTE, &slot);
			  	err_msg(MEM_PAT_ERR,&slot, shiptr,
					shiptr & mask, *(short int *)shiptr);
				if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
				}
			}
		  }
        }    
}


void
fillmem_int(__psunsigned_t lo, __psunsigned_t hi, unsigned int mask, 
		int size, uint value)
{
	__psunsigned_t ptr;
	__psunsigned_t badaddr;
	unsigned int badbits;	
	int slot;
	
	for (ptr = lo; ptr < hi; ptr += size) {
		if (valspecd)
			*(uint *)ptr = value & mask;
		else
               		*(uint *)ptr = (uint)(ptr & mask);
/*		if ((ptr % 0x1000) == 0)
       			if (cpu_intr_pending()) {
			   badaddr = vir2phys(ptr);
                           decode_address(badaddr,0, SW_WORD, &slot);
                           err_msg(MC3_INTR_ERR, &slot, badaddr);
			   if (chk_memereg(mem_slots)) {
				check_memereg(mem_slots);
				mem_err_clear(mem_slots);
			   }
	        msg_printf(VRB,"Writing 0x%x to addr 0x%llx\r", *(uint *)ptr, ptr);
			}
*/
	}
        for (ptr = lo; ptr < hi; ptr += size) {
		if (valspecd)
		{
			if (*(uint *)ptr != (value & mask))
			{
			    badbits = *(uint *)ptr ^ (uint)(ptr & mask);
			    decode_address(K1_TO_PHYS(ptr),badbits, SW_BYTE, &slot);
			    err_msg(MEM_PAT_ERR,&slot, ptr,value & mask,
                                                        *(uint *)ptr);
				if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
				}
			}
		}
		else
		{
		    if (*(uint *)ptr != (uint)(ptr & mask))
		    {
			  	badbits = *(uint *)ptr ^ (uint)(ptr & mask);
        		  	decode_address(K1_TO_PHYS(ptr), badbits, SW_BYTE, &slot);
			  	err_msg(MEM_PAT_ERR,&slot, ptr,
					(uint)(ptr & mask), *(uint *)ptr);
				if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
				}
			}
		  }
        }    
}

void fillmem_dw(__psunsigned_t lo, __psunsigned_t hi, __psunsigned_t mask, 
	   int size, uint value)
{
	__psunsigned_t ptr;
	unsigned int badbits;	
	int slot;
	
	for (ptr = lo; ptr < hi; ptr += size) {
		if (valspecd)
			*(long long *)ptr = (long long)(value & mask);
		else
               		*(long long *)ptr = (long long)(ptr & mask);
/*		if ((ptr % 0x1000) == 0)
			    if (chk_memereg(mem_slots)) {
				check_memereg(mem_slots);
				mem_err_clear(mem_slots);
			    }
*/
		if (VRB <= *Reportlevel)
	        msg_printf(VRB,"Writing 0x%llx to addr 0x%llx\r", *(long long *)ptr, ptr);
	}
        for (ptr = lo; ptr < hi; ptr += size) {
		if (valspecd)
		{
			if (*(long long *)ptr != (long long)(value & mask))
			{
			    badbits = *(long long *)ptr ^ (ptr & mask);
			    decode_address(K1_TO_PHYS(ptr),badbits, SW_BYTE, &slot);
			    err_msg(MEM_PAT_ERR,&slot, ptr,value & mask, *(long long *)ptr);
				if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
				}
			}
		}
		else
		{
		    if (*(long long *)ptr != (ptr & mask))
		    {
			  	badbits = *(long long *)ptr ^ (ptr & mask);
        		  	decode_address(K1_TO_PHYS(ptr), badbits, SW_BYTE, &slot);
			  	err_msg(MEM_PAT_ERR,&slot, ptr,
					ptr & mask, *(long long *)ptr);
				if (chk_memereg(mem_slots)) {
				    check_memereg(mem_slots);
				    mem_err_clear(mem_slots);
				}
			}
		  }
        }    
}

int mem_pat(int argc, char **argv)
{

    __psunsigned_t ptr, lo, hi;
    __psunsigned_t himem;
    __psunsigned_t paddr;
    int first_time = 1;
    uint i, value = 0xdeadbeef;
    register int size = SW_WORD;
    register __psunsigned_t mask = 0xffffffff;
    struct mc3_range addr_range;
    register int read_only = 0;
    int scope_loop = 0;	
    int continue_on_bad_range = 0;	
    register int fail = 0;
    int deadvalue;
    __psunsigned_t max_mem;
    int find_word = 0;
    unsigned int datatosearch = 0xdeadbeef;
    int val;
    int cached = 0;
    char answ[4];

    valspecd = 0;
    msg_printf(SUM, "Memory test with user specified input\n");

	msg_printf(DBG, "argc is %d\n", argc);
	msg_printf(DBG, "argv is %s\n", *argv);

    if (argc == 1)
    {
msg_printf(SUM,"Usage: mem11 [-b|h|w|d] [-r] [-l] [-c] [-v 0xpattern] RANGE\n");
msg_printf(SUM,"\twhere b = byte, h = half-word, w = word, d = double-word. Default is w.\n");
msg_printf(SUM,"\tr = read only. This will ignore anything set with -v.\n");
msg_printf(SUM,"\t\tReads are all word reads.\n");
msg_printf(SUM,"\t\tThe default is write/read (-r not set)\n");
msg_printf(SUM,"\tl = scope loop forever. Default is non-scope loop.\n");
msg_printf(SUM,"\tc = run in cached memory space (default is uncached)\n");
msg_printf(SUM,"\tv = specify a pattern, e.g. -v 0xab writes pattern 0xab.\n");
msg_printf(SUM,"\t\tThe default pattern is address in address.\n");
msg_printf(SUM,"\tRANGE = e.g. 0x400000:0x400100 or\n");
msg_printf(SUM,"\t\t0x400000#10   -- will write 10 items\n");
msg_printf(SUM,"\t\t0x400000	-- will write 1 item\n\n");
#if _MIPS_SIM == _ABI64
msg_printf(SUM,"\tb|h|w|d, r, l and v will use defaults if not specified.\n");
#else
msg_printf(SUM,"\tb|h|w, r, l and v will use defaults if not specified.\n");
#endif
msg_printf(SUM,"\tRANGE must be specified\n");
	return (2);
    }

    mem_slots = mc3slots();
    mem_err_clear(mem_slots);
    memtest_init();
    if (val = setjmp(mempat_buf)) {
        if (val == 1){
                msg_printf(ERR, "Unexpected exception during memory test\n");
                show_fault();
        }
	mem_err_log();
	mem_err_show();
	fail = 1;
	goto SLOTFAILED;
    }
    else {
	set_nofault(mempat_buf);

    /* get arguments */

     while (--argc > 0) 
/*    for (i = 1; i < argc; i++) */
    {
	msg_printf(DBG, "argc loop. argc is %d\n", i);
	if (**++argv == '-') 
	/* if (argv[i][0] = '-') */
	{
	msg_printf(DBG,"argv is -\n");
	msg_printf(DBG,"*argv[1] is %c\n", (*argv)[1]);
	/* msg_printf(DBG,"argv[%d][0] is -\n", i); */
	/* msg_printf(DBG,"argv[%d][1] is %c\n", argv[i][1]); */
	    switch ((*argv)[1]) 
	    /* switch (argv[i][1]) */
	    {
		case 'b':
		    size = SW_BYTE;
		    mask = 0xff;
		    break;

		case 'h':
		    size = SW_HALFWORD;
		    mask = 0xffff;
		    break;

		case 'w':

		case 'c':
                    cached = 1;
		    size = SW_WORD;
		    break;

#if _MIPS_SIM == _ABI64
		case 'd':
		    size = SW_DBLWORD;
		    mask = 0xffffffffffffffff;
		    break;
#endif

		case 'v':
		    if (argc-- < 1) {
			msg_printf(SUM,"The -v switch requires a pattern\n");
			return (1);
		    }
		    if (*atobu(*++argv, &value)) {
		       msg_printf(SUM,"Unable to understand the -v pattern\n");
			return (1);
		    }
		    valspecd = 1;
		    break;
		case 'f':
		    find_word = 1;
		    if (argc-- < 1) {
			msg_printf(SUM,"The -f switch requires a pattern\n");
			return (1);
		    }
		    msg_printf(DBG,
			"datatosearch before is : 0x%x\n", datatosearch);

		    if (*atobu(*++argv, &value)) {
		       msg_printf(SUM,"Unable to understand the -f pattern\n");
			return (1);
		    }
		    msg_printf(DBG,
			"datatosearch after is : 0x%x\n", datatosearch);
		    break;
		case 'r':
		    read_only = 1;
		    break;
		case 'l':
		    scope_loop = 1;
		    break;
		case 't':
		    continue_on_bad_range = 1;
		    break;
		default:
		    msg_printf(SUM,"Unknown switch: %c\n", (*argv)[1]);
		    return (1);
	    }
	    /* argv++; */
	}
	else
	{ /* parse range */
	    addr_range.size = size;
	     if (parse_addr(*argv, &addr_range)) {
	 	 msg_printf(DBG,"parse_addr's argv is \n"); 
		msg_printf(SUM,"Unable to understand the range specified\n");
		return (1);
	    }
	    max_mem = readconfig_reg(RDCONFIG_MEMSIZE,1);
            msg_printf(DBG,"RETURN FROM READCONFIG\n");
	    if (addr_range.hi > max_mem) {
	      msg_printf(SUM,"ERROR: Range specified exceeds max memory\n");
		if (!continue_on_bad_range) {
			msg_printf(VRB, "Exiting...\n");
			return (1);
		}
		else {
			msg_printf(VRB, "About to continue with bad addrs\n");
			/* msg_printf(VRB, "About to do a longjmp!\n");
			longjmp(mempat_buf, 2); */
		}
	    } /* if (addr_range.hi > max_mem) */
	} /* else ... parse range */
    } /* while ... checking args */
	msg_printf(DBG,"check for too low of range\n");
    if (addr_range.lo < PHYS_CHECK_LO) {
	msg_printf(SUM,"Specified range is below 0x%x\n", PHYS_CHECK_LO);
	msg_printf(SUM,"Accessing addresses below 0x%x may hang or crash ",
			PHYS_CHECK_LO);
	msg_printf(SUM,"your system\n\n");
questloop:
	msg_printf(SUM,"Do you want to continue? (y=1, n=0): ");
	gets(answ);
        if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                        if (!strcmp(answ, "0")) {
                                msg_printf(SUM,"Exiting now...\n");
                                return (1);
                        }
        }
        else {
                        msg_printf(SUM,"Please enter a 1 or a 0\n");
                        goto questloop;
        }
questloop2:
        msg_printf(SUM,"Are you sure? (y=1, n=0): ");
        gets(answ);
        if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                        if (!strcmp(answ, "0")) {
                                msg_printf(SUM,"Exiting now...\n");
                                return (1);
                        }
        }
        else {
                        msg_printf(SUM,"Please enter a 1 or a 0\n");
                        goto questloop2;
        }
    }
    if (addr_range.hi < addr_range.lo) {
	msg_printf(SUM,
	   "ERROR: Specified range has low address above high address.\n");
	msg_printf(SUM,"Re-enter range\n");
	return (1);
    }
    if (find_word) {
	find_word = 0;
	msg_printf(SUM,"Data to search for is 0x%x\n", datatosearch);
	lo = PHYS_TO_MEM(addr_range.lo);
	hi = PHYS_TO_MEM(addr_range.hi);
	deadvalue = 0;
	if (datatosearch == 0)
		msg_printf(DBG,"DATA is equal to zero!\n");
	msg_printf(DBG,"done setting k1 space for find\n");
	msg_printf(DBG,"LO IS 0x%x, hi is 0x%x\n\n", lo, hi);
	for (ptr = lo; ptr < hi; ptr++)
	{
	  msg_printf(VRB,"Checking 0x%x, has data of 0x%x\r",ptr, *(uint *)ptr);
	  if (*(uint *)ptr == (uint)datatosearch) {
		  msg_printf(SUM,
		   "\n\n0x%x found at 0x%x\n\n",(uint)datatosearch,ptr);
		 find_word = 1;
	  }
	}
	if (!find_word)
		msg_printf(SUM,"Data not found\n");	
	return (0);
    } /* find_word */
	msg_printf(DBG,"Starting to set up vaddr\n");
#if _MIPS_SIM == _ABI64
                paddr = PHYS_TO_MEM(addr_range.lo);
                lo = paddr;
		    himem = PHYS_TO_MEM(addr_range.hi);
		    hi = himem;
                msg_printf(DBG, "lo:0x%llx, hi:0x%llx, p: 0x%llx\n",
			lo, hi, paddr);
#else
    first_time = 1;
	msg_printf(DBG,"cached variable is %d\n", cached);
    for (i = addr_range.lo; i < addr_range.hi; i += DIRECT_MAP) {
                paddr = i;
                lo = map_addr(paddr, DIRECT_MAP, cached);
                if (addr_range.hi < (i + DIRECT_MAP)) {
                   himem = (lo + (addr_range.hi - paddr));
		   hi = himem;
                   msg_printf(DBG,"range.hi-paddr is 0x%llx, hi is 0x%x\n",
                         addr_range.hi-paddr, hi);
                }
                else {
                   himem = lo + (DIRECT_MAP-(first_time*PHYS_CHECK_LO));
		   hi = himem;
                   first_time = 0;
                }
                msg_printf(DBG, "lo:0x%x, hi:0x%x, p: 0x%llx\n",
			lo, hi, paddr);
	}
#endif

 /* start the real testing now */

	msg_printf(DBG,"size in bytes is %d \n", size);
	msg_printf(DBG,"value is 0x%x \n", value);
    if (read_only)
    {
	msg_printf(INFO,"Reading data\n");
	if (scope_loop)
	{
		while (1)
			for (ptr = lo; ptr < hi; ptr += size) {
                	   deadvalue = *(uint *)ptr;
			   msg_printf(VRB,"data: 0x%x, addr: 0x%llx\r",
				 deadvalue, ptr);
			}
	}
	else
		for (ptr = lo; ptr < hi; ptr += size) {
                	deadvalue = *(uint *)ptr;
			msg_printf(VRB,"data: 0x%x, addr: 0x%llx\r",
				deadvalue, ptr);
		}
    }
    else { /* write-verify */
	msg_printf(INFO,"Writing/verifying data\n");
	ptr = lo;
	msg_printf(DBG,"ptr gets lo, ptr: 0x%llx, lo:0x%llx\n",ptr,lo);
	switch (size)
	{
	case SW_BYTE:
	   	if (scope_loop) 
			while (1) 
				fillmem_ch(lo, hi, (uint)mask, size, value);
	   	else  
			fillmem_ch(lo, hi, (uint)mask, size, value);
		break;
	case SW_HALFWORD:
	   	if (scope_loop) 
			while (1) 
				fillmem_shi(lo, hi, (uint)mask, size, value);
	   	else 
			fillmem_shi(lo, hi, (uint)mask, size, value);
		break;
	case SW_WORD:
		if (scope_loop)
			while (scope_loop)
				fillmem_int(lo, hi, (uint)mask, size, value);
		else
			fillmem_int(lo, hi, (uint)mask, size, value);
		break;
	case SW_DBLWORD:
		if (scope_loop)
			while (1)
				fillmem_dw(lo, hi, mask, size, value);
		else
			fillmem_dw(lo, hi, mask, size, value);
		break;
	default:
		msg_printf(SUM,"Error! - software glitch. Check code.\n");
	} /* switch */
    } /* write-verify */
    } /* else ... setjmp */

SLOTFAILED:	

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Memory test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }
  
    clear_nofault();

    return (fail);
}
