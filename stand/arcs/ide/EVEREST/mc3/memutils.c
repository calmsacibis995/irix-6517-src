/*
 * mc3/memutils.c
 *
 *
 * Copyright 1991, 1992, Silicon Graphics, Inc.
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

#include <sys/types.h>
#include <ctype.h>
#include <sys/cpu.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/evconfig.h>
#include "libsc.h"
#include "libsk.h"
#include "exp_parser.h"
#include "pattern.h"
#ifdef IP19
#include "ip19.h"
#elif IP21
#include "ip21.h"
#elif IP25
#include "ip25.h"
#endif
#include "mc3_reg.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include "uif.h"
#include "prototypes.h"

#define SIMM0BAD  1
#define SIMM1BAD  2
#define SIMM2BAD  4
#define SIMM3BAD  8

extern unsigned int mem_slots;
extern unsigned _digit(char);
extern char *atob_ptr(const char *, __psint_t *);
extern __psunsigned_t vir2phys(__psunsigned_t);
extern void SetCompare(uint);
extern void set_count(uint);

void StopTimer(void);
int TimerHandler(void);
void EnableTimer(uint);
int enable_disable_bnk(int);
uint decode_address(unsigned long long, unsigned long long, int, int *);
int in_simm(unsigned long long, uint, int);
int myin_bank(__psunsigned_t, uint, uint, uint, uint);
int addr2slbs(__psunsigned_t, uint, int *, int *, int *, int *, int);
char *atobll(register char *, unsigned long long *);
uint mc3slots(void);
void en_ecc_intr(__psunsigned_t);
void dis_ecc_intr(__psunsigned_t);
void handle_ecc_intr(void);
void memtest_init(void);

/*
 * Array used to find the actual size of a particular
 * memory bank.  The size is in blocks of 256 bytes, since
 * the memory banks base address drops the least significant
 * 8 bits to fit the base addr in one word.
 */
unsigned long MemSizesMC3[] = {
    0x10000,    /* 0 = 16 megs   */
    0x40000,    /* 1 = 64 megs   */
    0x80000,    /* 2 = 128 megs  */
    0x100000,   /* 3 = 256 megs  */
    0x100000,   /* 4 = 256 megs  */
    0x400000,   /* 5 = 1024 megs */
    0x400000,   /* 6 = 1024 megs */
    0x0         /* 7 = no SIMM   */
};

jmp_buf mem_buf;


/*
 * Parse a range expression having one of the following forms:
 *
 *      base:limit      base and limit are byte-addresses
 *      base#count      base is a byte-address, count is in size-units
 *      base            byte-address, implicit max size
 *
 * Return 1 on syntax error, 0 otherwise.  Return in lo  the base
 * address, truncated to be size-aligned.  Return in hi  The last
 * address, and the sizemask in addr->mask
 *
 * NB: size must be a power of 2.
 */
int
parse_addr(register char *ptr, struct mc3_range *rptr)
{
        register unsigned long long sizemask;
        char size;
        __psunsigned_t lo, hi;
        unsigned long long tmp;

        size = rptr->size;
        ptr = atobu_ptr(ptr, &lo);
        sizemask = ~(size - 1);
        rptr->lo = lo & sizemask;
        switch (*ptr) {
          case ':':
                if (*atobu_ptr(++ptr, &hi))
                        return 1;
                break;

          case '#':
                if (*atobu_ptr(++ptr, &hi))
                        return 1;
                /*
                 * hi holds the range's count, so turn it into
                 * limit address.
                 */
                tmp= hi*size + lo;
                hi = tmp & sizemask;
                break;

          case '\0':
                hi = lo + size;
                break;

          default:
                return 1;
        }
        rptr->hi = hi;
        return 0;
}

/* from IP19prom/pod.c */
uint read_reg(uint slot, uint reg_num)
{
    return load_double_lo((long long *)(EV_CONFIGREG_BASE + (slot << 11) + (reg_num << 3)));
}

unsigned long long readconfig_reg(unsigned int queryType, unsigned int fullMem)
{
    register int bank, i, leaf, slot;
    __psunsigned_t value[30];
    register unsigned long long mem_size_in_bytes = 0x0;
    register uint bank_size_in_bytes = 0x0;
    register uint smallmem = 0x801000;

    for (i = 0; i <= 25; i++)
	value[i] = 0;

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
        if (board_type(slot) == EVTYPE_MC3) {
	    if (queryType & RDCONFIG_INFO) {
            	msg_printf(SUM,"The board in slot %d is memory\n", slot);
           /*
            * Read per board registers
            *
            * MC3_BANKENB     0x00     8  RW Specifies enabled banks
            * MC3_TYPE        0x01     32 R  Contains board type for MC3
            * MC3_REVLEVEL    0x02     32 R  Revision level of board
            * MC3_ACCESS      0x03     3  RW Access Control
            * MC3_MEMERRINT   0x04     17 RW Memory Err Interrrupt Register
            * MC3_EBUSERRINT  0x05     17 RW EBUS Error Interrupt Register
            * MC3_BISTRESULT  0x06     20 RW BIST result register
            * MC3_DRSCTIMEOUT 0x07     20 RW DRSC timeout register
            * MC3_REFRESHCNT  0x09     0  W  Refresh count initialize
            * MC3_LEAFCTLENB  0x0a     4  RW Leaf Control enable
            * MC3_RESET       0x0f     0  W  Board reset
            */
            value[0] = read_reg(slot, MC3_BANKENB);
            value[1] = read_reg(slot, MC3_TYPE);
            value[2] = read_reg(slot, MC3_REVLEVEL);
            value[3] = read_reg(slot, MC3_ACCESS);
            value[4] = read_reg(slot, MC3_MEMERRINT);
            value[5] = read_reg(slot, MC3_EBUSERRINT);
            value[6] = read_reg(slot, MC3_BISTRESULT);
            value[7] = read_reg(slot, MC3_DRSCTIMEOUT);
            value[8] = read_reg(slot, MC3_LEAFCTLENB);
            msg_printf(VRB,"MC3 Enable Register = 0x%x\n", (uint)value[0]);
            msg_printf(SUM,"MC3 Board type = 0x%x\n", (uint)value[1]);
            msg_printf(SUM,"MC3 Board rev level = 0x%x\n", (uint)value[2]);
            msg_printf(VRB,"Access control register = 0x%x\n", (uint)value[3]);
            msg_printf(VRB,"Memory Error Interrupt reg = 0x%x\n",
			 (uint)value[4]);
            msg_printf(VRB,"EBUS Error Interrupt reg = 0x%x\n",(uint)value[5]);
            msg_printf(VRB,"BIST result register = 0x%x\n", (uint)value[6]);
            msg_printf(VRB,"DRSC timeout register = 0x%x\n", (uint)value[7]);
            msg_printf(VRB,"Leaf Control enable = 0x%x\n", (uint)value[8]);
	    }

            /* It's a memory board, do leaf loop */
            for (leaf = 0; leaf < MC3_NUM_LEAVES; leaf++) {
		msg_printf(DBG,"leaf is 0x%x\n", leaf);
		if (queryType & RDCONFIG_INFO) {
               /*
                * Read the per leaf registers
                *
                * MC3LF_ERROR     0x20     4  R  Error status register
                * MC3LF_ERRORCLR  0x21     4  R  Error status, clear on read
                * MC3LF_ERRADDRHI 0x22     8  R  ErrorAddressHigh
                * MC3LF_ERRADDRLO 0x23     32 R  ErrorAddressLo
                * MC3LF_BIST      0x24     24 RW Bist status register
                * MC3LF_SYNDROME0 0x30     16 R  Syndrome slice register 0
                * MC3LF_SYNDROME1 0x31     16 R  Sundrome slice register 1
                * MC3LF_SYNDROME2 0x32     16 R  Syndrome slice register 2
                * MC3LF_SYNDROME3 0x33     16 R  Syndrome slice register 3
                */
		msg_printf(DBG,"Start to read vals 9-16\n");
                value[9] = read_reg(slot, MC3_LEAF(leaf, MC3LF_ERROR));
		msg_printf(DBG,"Start to read vals 10\n");
                value[10] = read_reg(slot, MC3_LEAF(leaf, MC3LF_ERRADDRHI));
		msg_printf(DBG,"Start to read vals 11\n");
                value[11] = read_reg(slot, MC3_LEAF(leaf, MC3LF_ERRADDRLO));
		msg_printf(DBG,"Start to read vals 12\n");
                value[12] = read_reg(slot, MC3_LEAF(leaf, MC3LF_BIST));
/*
		if (banks_populated(slot, leaf)) {
		   msg_printf(DBG,"Start to read vals 13\n");
                   value[13] = read_reg(slot, MC3_LEAF(leaf, MC3LF_SYNDROME0));
		   msg_printf(DBG,"Start to read vals 14\n");
                   value[14] = read_reg(slot, MC3_LEAF(leaf, MC3LF_SYNDROME1));
		   msg_printf(DBG,"Start to read vals 15\n");
                   value[15] = read_reg(slot, MC3_LEAF(leaf, MC3LF_SYNDROME2));
		   msg_printf(DBG,"Start to read vals 16\n");
                   value[16] = read_reg(slot, MC3_LEAF(leaf, MC3LF_SYNDROME3));
		}
*/
		msg_printf(DBG,"Done reading 9-16\n");
                    msg_printf(VRB,"Leaf %d Err status reg = 0x%x\n",
			 leaf, (uint)value[9]);
                    msg_printf(VRB,"Leaf %d Err address high = 0x%x\n",
			 leaf, (uint)value[10]);
                    msg_printf(VRB,"Leaf %d Err address low = 0x%x\n",
			 leaf, (uint)value[11]);
                    msg_printf(VRB,"Leaf %d Bist status reg = 0x%x\n",
			 leaf, (uint)value[12]);
/*
		    if (banks_populated(slot, leaf)) {
                    	msg_printf(VRB,"Leaf %d Syndrome reg 0 = 0x%x\n",
			 leaf, (uint)value[13]);
                    	msg_printf(VRB,"Leaf %d Syndrome reg 1 = 0x%x\n",
			 leaf, (uint)value[14]);
                    	msg_printf(VRB,"Leaf %d Syndrome reg 2 = 0x%x\n",
			 leaf, (uint)value[15]);
                    	msg_printf(VRB,"Leaf %d Syndrome reg 3 = 0x%x\n",
			 leaf, (uint)value[16]);
		    }
		    else
		    	msg_printf(VRB,
			  "No banks populated. Skipping syndrome read.\n");
*/

		} /* if querytype = RDCONFIGINFO */

                /* Do the bank loop */
                for (bank = 0; bank < MC3_BANKS_PER_LEAF; bank++) {
                  /* Nonzero means enabled, nonzero == enabled */
	msg_printf(DBG,"bank is 0x%x\n", bank);
                  if (read_reg(slot, MC3_BANKENB) & MC3_BENB(leaf, bank))
                  {
                    	msg_printf(DBG,"Leaf %d, Bank %d is enabled\n", 
				leaf, bank);
                    /*
                     * Read the per bank registers
                     *
                     * BANK_SIZE  0  3  RW Encoded bank size reg
                     * BANK_BASE  1  32 RW Base Address for Bank's interleave
                     * BANK_IF    2  3  RW Interleave Factor register
                     * BANK_IP    3  4  RW Interleave Position register
                     */
                    value[17] = read_reg(slot, MC3_BANK(leaf,bank,BANK_SIZE));
                    value[18] = read_reg(slot, MC3_BANK(leaf, bank, BANK_IF));
                    value[19] = read_reg(slot, MC3_BANK(leaf, bank, BANK_IP));
                    value[20] = read_reg(slot, MC3_BANK(leaf,bank,BANK_BASE));
		       msg_printf(DBG,"Leaf %d, ", leaf);
                       msg_printf(DBG,"bank %d SIMM type code = 0x%x\n", 
				bank, (uint)value[17]);
		       msg_printf(DBG,"Leaf %d, ", leaf);
                       msg_printf(DBG,"bank %d IF = 0x%x\n", 
				bank, (uint)value[18]);
		       msg_printf(DBG,"Leaf %d, ", leaf);
                       msg_printf(DBG,"bank %d IP = 0x%x\n",
				bank,(uint)value[19]);
		       msg_printf(DBG,"Leaf %d, ", leaf);
                       msg_printf(DBG,"bank %d Base addr = 0x%x\n",
				bank,(uint)value[20]);
	value[29] = (queryType & RDCONFIG_MEMSIZE);
		    if (queryType & RDCONFIG_MEMSIZE) {
			/*
			 * code	DRAM type	size in MegBytes
			 * ----------------------------------------
			 * 0	256Kx4		not available
			 * 1	1Mx4		16M
			 * 2  	2Mx9		not available
			 * 3	4Mx4		not available (was before 5.0)
			 * 4 	4Mx4		64MB (new since 5.0)
			 * 5-6	unavaliable
			 * 7	unoccupied
			 */
			switch (value[17]) {
				case 0:
				    bank_size_in_bytes = (0x400000 << 2);
				    break;

				case 1:
				    bank_size_in_bytes = (0x1000000 << 2);
				    break;

				case 3: 
				    bank_size_in_bytes = (0x4000000 << 2);
				    break;

				case 4: 
				    bank_size_in_bytes = (0x4000000 << 2);
				    break;

				case 7: 
				    bank_size_in_bytes = 0x0;
				    break;

				default : 
				    bank_size_in_bytes = 0x0;
			}
			mem_size_in_bytes += bank_size_in_bytes;
		msg_printf(DBG,"banksize var is 0x%x\n", bank_size_in_bytes);
		msg_printf(DBG,"Cumulative memory including ");
		msg_printf(DBG,"leaf %d,bank%d:0x%llx MB\n",
			leaf,bank, mem_size_in_bytes);
		    }   /* if RDCONFIG_MEMSIZE */
                  }   /* if bank is enabled */
                  else {
		     if (queryType & RDCONFIG_INFO)
                     msg_printf(SUM,"Leaf %d, Bank %d is NOT enabled\n", 
			leaf,bank);
			} /* else not enabled */
                }   /* for bank */
            }    /* for leaf */
        }    /* if mc3 boards */
    }    /* for all slots */
    if (queryType & RDCONFIG_MEMSIZE) {
	value[21] = mem_size_in_bytes;
	msg_printf(VRB,"Memory size for system is 0x%llx bytes\n", 
		 mem_size_in_bytes);
#ifdef SMALLMEMSIZE
	if (fullMem) 
	msg_printf(VRB,"Overriding SMALLMEMSIZE switch. Using full memory.\n");
	else {
	  msg_printf(VRB,"Resetting memsize to be 0x%x bytes\n",smallmem);
		value[21] = smallmem;
	}
#endif
    }
        msg_printf(DBG,"end of READCONFIG routine\n");
    return(value[21]);
} /* readconfig_reg */


uint dump_mc3(uint slot)
{
        uint mem;
        int i, j;

        mem = mc3slots();
	msg_printf(DBG,"SLOT %d\n", slot);
        if (slot > EV_MAX_SLOTS) {
                msg_printf(SUM,"*** Slot out of range\n");
                return (1);
        }

        if (!((1 << slot) & mem)) {
                msg_printf(SUM,"*** Slot %d has no memory board.\n", slot);
                return (1);
        }
        msg_printf(SUM,"Configuration of the memory board in slot %x\n", slot);
        msg_printf(SUM," EBus Error:   %x\n", read_reg(slot, MC3_EBUSERROR));
        msg_printf(SUM," Leaf Enable:  %x\n", read_reg(slot, MC3_LEAFCTLENB));
        msg_printf(SUM," Bank Enable:  %x\n", read_reg(slot, MC3_BANKENB));
        msg_printf(SUM," BIST Result:  %x\n", read_reg(slot, MC3_BISTRESULT));
        for (i = 0; i < 2; i++) {
                msg_printf(SUM," Leaf %d:\n", i);
                msg_printf(SUM,
		  "  BIST = %x, Error = %x, ErrAddrHi = %x, ErrAddrLo = %x\n",
				read_reg(slot, MC3_LEAF(i, MC3LF_BIST)),
                                read_reg(slot, MC3_LEAF(i, MC3LF_ERROR)),
                                read_reg(slot, MC3_LEAF(i, MC3LF_ERRADDRHI)),
                                read_reg(slot, MC3_LEAF(i, MC3LF_ERRADDRLO)));
/*
		if (banks_populated(slot, i)) {
                	msg_printf(SUM,
	  "  Syndrome 0: %x, Syndrome 1: %x, Syndrome 2: %x, Syndrome 3: %x\n",
                                read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME0)),
                                read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME1)),
                                read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME2)),
                                read_reg(slot, MC3_LEAF(i, MC3LF_SYNDROME3)));
		}
		else {
			msg_printf(SUM,
			"No populated banks. Skipping syndrome read.\n");
		}
*/
                for (j = 0; j < 4; j++) {
                        msg_printf(SUM,"  Bank %d: ", j);
                        msg_printf(SUM,
				 "Size = %x, Base = %x, IF = %x, IP = %x\n",
                                read_reg(slot, MC3_BANK(i, j, BANK_SIZE)),
                                read_reg(slot, MC3_BANK(i, j, BANK_BASE)),
                                read_reg(slot, MC3_BANK(i, j, BANK_IF)),
                                read_reg(slot, MC3_BANK(i, j, BANK_IP)));
                }
        }
	return (0);
}

uint dump_mc3_syndrome(uint slot)
{
        uint mem;

        mem = mc3slots();
	msg_printf(DBG,"SLOT %d\n", slot);
        if (slot > EV_MAX_SLOTS) {
                msg_printf(SUM,"*** Slot out of range\n");
                return (1);
        }

        if (!((1 << slot) & mem)) {
                msg_printf(SUM,"*** Slot %d has no memory board.\n", slot);
                return (1);
        }

	return (0);
}


dump_memreg()
{
	uint slot;
	uint fail = 0;

	for (slot = 0; slot < EV_MAX_SLOTS; slot++) 
        	if (board_type(slot) == EVTYPE_MC3) {
			msg_printf(DBG,"dump_memreg, slot is %d\n", slot);
			fail = dump_mc3(slot);
		}
	return (fail);
} /* dump_memreg */


dump_syndrome()
{
        uint slot;
        uint fail = 0;

        for (slot = 0; slot < EV_MAX_SLOTS; slot++)
                if (board_type(slot) == EVTYPE_MC3) {
                        msg_printf(DBG,"dump_memreg, slot is %d\n", slot);
                        fail = dump_mc3_syndrome(slot);
                }
        return (fail);
} /* dump_syndrome */


#if 0
int banks_populated(int slot, int leaf)
{
	uint leaf0populated = 0x33;
	uint leaf1populated = 0xcc;	
	uint populated = 0;

	if (leaf)  {
		populated = leaf1populated & read_reg(slot, MC3_BANKENB);
	}
	else {
		populated = leaf0populated & read_reg(slot, MC3_BANKENB);
	}

	return(populated);

} /* banks_populated */
#endif


/* given a physical address, read_word converts it to its equivalent
 * in the specified kernel space and reads the value there.
 */
#if 0
uint
read_word(enum mem_space accessmode, uint phys_addr)
      /* accessmode = k0seg, k1seg, or k2seg */
      /* phys_addr = read from this address */
{
        uint vaddr;
#ifdef R4K_UNCACHED_BUG
        tag_regs_t PITagSave,PDTagSave,STagSave,TagZero;
        uint uval;

        /* If R4K is rev 1.2 and this read is via k1seg we must
         * ensure that the pdcache doesn't currently have this
         * line as valid (1.2 sometimes read/wrote the k0 value
         * instead of k1 to memory); we must invalidate line to
         * force R4K to read from memory, not cache.  But, check
         * the processor id before wasting this time. */
        if ((accessmode == k1seg) && (get_prid() & 0xF)) {
                TagZero.tag_lo = 0;
                read_tag(PRIMARYI, phys_addr, &PITagSave);
                read_tag(PRIMARYD, phys_addr, &PDTagSave);
                read_tag(SECONDARY, phys_addr, &STagSave);
                write_tag(PRIMARYI, phys_addr, &TagZero);
                write_tag(PRIMARYD, phys_addr, &TagZero);
                write_tag(SECONDARY, phys_addr, &TagZero);
        }
#endif

        switch(accessmode) {
        case k0seg:
                        vaddr = PHYS_TO_K0(phys_addr);
                        break;
        case k1seg:
                        vaddr = PHYS_TO_MEM(phys_addr);
                        break;
        case k2seg:
                        vaddr = phys_addr+K2BASE;
                        break;
        }

#ifdef R4K_UNCACHED_BUG
        uval = *(uint *)vaddr;
        if ((accessmode == k1seg) && (get_prid() & 0xF)) {
                write_tag(PRIMARYI, phys_addr, &PITagSave);
                write_tag(PRIMARYD, phys_addr, &PDTagSave);
                write_tag(SECONDARY, phys_addr, &STagSave);
        }
        return (uval);
#else
        return (*(uint *)vaddr);
#endif

} /* end fn read_word */

#endif

 /*
 * atobll -- convert ascii to binary.  Accepts all C numeric formats.
 * from IP17diags/interface/dlib.c 
 */
char *
atobll(register char *cp, unsigned long long *iptr)
{
        register unsigned base = 10;
        register unsigned long long value = 0;
        register unsigned d;
	unsigned long long eight_Gig = 0x80000000; /* 512MB decimal */
        int minus = 0;
        int overflow = 0;

	eight_Gig <<= 2; /* make it 8GB decimal */
        *iptr = 0;
        if (!cp)
                return(0);

        while (isspace(*cp))
                cp++;

        while (*cp == '-') {
                cp++;
                minus = !minus;
        }

        /*
         * Determine base by looking at first 2 characters
         */
        if (*cp == '0') {
                switch (*++cp) {
                case 'X':
                case 'x':
                        base = 16;
                        cp++;
                        break;

                case 'B':       /* a frill: allow binary base */
                case 'b':
                        base = 2;
                        cp++;
                        break;

                default:
                        base = 8;
                        break;
                }
        }

        while ((d = _digit(*cp)) < base) {
                if (value > ((unsigned long long)-1/base))
                        overflow++;
                value *= base;
                if (value > (unsigned long long)(-1 - d))
                        overflow++;
                value += d;
                cp++;
        }
/*
        if (overflow || (value == eight_Gig && minus))
                msg_printf(SUM,"WARNING: conversion overflow\n");
*/
        if (minus)
                value = -value;

        *iptr = value;
        return(cp);
}

/*
 * Convert an alphanumeric string to binary (int), converting from the units
 * specified by a suffix into bytes.
 * -- from IP17diags/interface/dlib.c
 */
char *
atobllu(register char *cp, register unsigned long long *intp)
{

        cp = atobll(cp, intp);
        switch (*cp) {
          default:                      /* unknown */
                return cp;
          case 'k':                     /* kilobytes */
          case 'K':
                *intp *= 0x400;
                break;
          case 'm':                     /* megabytes */
          case 'M':
                *intp *= 0x100000;
                break;
       /*   case 'p':                   --   pages --
          case 'P':
                *intp <<= PGSHIFT;       XXX this is from sys/param.h --
                break;
	*/
        }
        return cp + 1;
}

pow(unsigned int base, unsigned int exp)
{
	unsigned int i;
	unsigned int b = base;
	unsigned int e = exp;
	unsigned int result = 1;


	msg_printf(DBG,"b is %x, e is %x\n", b, e);

	for (i = 1; i <= e; i++) {
		result *= b;
	msg_printf(DBG,"result is %x and i is %x\n", result, i);
	}

	return (result);
}


/* lifted and modified from IP19prom/pod_mem */

/* decode_your_addr() reads input from the user.
 * decode_address() reads input from other functions.
 */
uint
decode_your_addr(int argc, char **argv)
{
	unsigned long long addr, phys_lo;
	__psunsigned_t badbits = 0;
	int size = SW_WORD; /* default to word */
	int dovirtual_addr = 0;
	__psunsigned_t virtual_addr, myvirt_addr;
	int slot;
	/* extern char *atob();
	extern char *atob_L(); */

        msg_printf(DBG,"argc is %d\n", argc);
        msg_printf(DBG,"argv is %s\n", *argv);

    	if (argc == 1)
    	{
        msg_printf(SUM,"Usage: mem12 [-a 0xPhysAddress] [-b 0xBBB] [-s x]\n");
	msg_printf(SUM,"   -b expects a hex number showing\n");
	msg_printf(SUM,"      which bits are bad. \n");
	msg_printf(SUM,"      e.g. If bits 0 and 2 are bad, enter:\n");
	msg_printf(SUM,"      -b 0x5\n");
	msg_printf(SUM,"   -s 1, 2, or 4 for byte, half-word or word\n");
	msg_printf(SUM,"   -b defaults to 0x0 and -s defaults to 4\n\n");
	msg_printf(SUM,"For example, to decode physical address 0x4000 \n");
	msg_printf(SUM,"with bad bits 0 and 2 and it's a word, type:\n\n");
	msg_printf(SUM,"\t\tmem12 -a 0x4000 -b 0x5 -s 4\n");
	return (0);
    	}

    /*
     * from stand/arcs/lib/libsk/ml/cache.s
     * invalidates all caches. Added because of the R4000 uncached writeback
     * bug - makes *sure* that there are no valid cache lines
     */
    	ide_invalidate_caches();

    /* get arguments */

     	while (--argc > 0)
	{
        	msg_printf(DBG,"argc loop. argc is %d\n", argc);
        	if (**++argv == '-')
        	{
        		msg_printf(DBG,"argv is -\n");
        		msg_printf(DBG,"*argv[1] is %c\n", (*argv)[1]);
        		msg_printf(DBG,"argc loop2. argc is %d\n", argc);
            		switch ((*argv)[1])
            		{
			   case 'a':
                    	      if (argc-- <= 1) {
                        	msg_printf(SUM,	
				   "The -a switch requires an address\n");
                       		return (1);
                    	      }
                    	      if (*atob_ptr(*++argv, (__psint_t *)&addr)) {
                        	msg_printf(SUM,
				   "Unable to understand the -a address\n");
                        	return (1);
                    	      }
				msg_printf(DBG,"addr at first is 0x%llx\n",
					addr);
                    	      break;
			   case 'b':
                    	      if (argc-- <= 1) {
                        	msg_printf(SUM,	
				   "The -b switch requires an argument\n");
                       		return (1);
                    	      }
                    	      if (*atob(*++argv, (int *)&badbits)) {
                        	msg_printf(SUM,
				   "Unable to understand the -b argument\n");
                        	return (1);
                    	      }
                    	      break;
			   case 's':
                    	      if (argc-- <= 1) {
                        	msg_printf(SUM,	
				   "The -s switch requires an argument\n");
                       		return (1);
                    	      }
                    	      if (*atob(*++argv, (int *)&size)) {
                        	msg_printf(SUM,
				   "Unable to understand the -s argument\n");
                        	return (1);
                    	      }
                    	      break;
			   case 'v':
				dovirtual_addr = 1;
				break;
                           case 'p':
                              if (argc-- <= 1) {
                                msg_printf(SUM,
                                   "The -p switch requires an argument\n");
                                return (1);
                              }
                              if (*atob_ptr(*++argv, (__psint_t *)&phys_lo)) {
                                msg_printf(SUM,
                                   "Unable to understand the -p argument\n");
                                return (1);
                              }
                              break;
                           case 'w':
                              if (argc-- <= 1) {
                                msg_printf(SUM,
                                   "The -w switch requires an argument\n");
                                return (1);
                              }
                              if (*atob(*++argv, (int *)&myvirt_addr)) {
                                msg_printf(SUM,
                                   "Unable to understand the -w argument\n");
                                return (1);
                              }
                              break;
			   default:
                    		msg_printf(SUM,
				   "Unknown switch: %c\n", (*argv)[1]);
                    		return (1);
            		} /* switch */
		}
		else {
			msg_printf(SUM,"Unknown command line input\n");
			return (1);
		}
	} /* while */
        msg_printf(DBG,"addr is 0x%llx \n", addr);
	msg_printf(DBG,"addr anded with badbits is 0x%llx\n",
		addr & badbits);
	msg_printf(DBG,"addr anded with (long l)badbits is 0x%llx\n",
		addr & (unsigned long long)badbits);
        msg_printf(DBG,"badbits is 0x%x \n", badbits);
	if (dovirtual_addr) {
	   virtual_addr = map_addr(addr, 0x20000000, UNCACHED);
	   msg_printf(SUM,"virtual addr, uncached: 0x%x\n", virtual_addr);
	   virtual_addr = map_addr(addr, 0x20000000, CACHED);
	   msg_printf(SUM,"virtual addr, cached: 0x%x\n", virtual_addr);
	}
	if (decode_address(addr, badbits, size, &slot))
		return (1);
	else
		return (0);
} /* decode_your_addr */



/* the semi-guts of decoding */
uint
decode_address(unsigned long long paddr, unsigned long long badbits, int size, int *slotp)
{
   int leaf, bank, simm;
   char letr;

   if (addr2slbs(paddr, badbits, slotp, &leaf, &bank, &simm, size)) {
        msg_printf(ERR,"*** Unable to decode address 0x%llx!\n", paddr);

	err_msg(DECODE_ERR, slotp,  paddr);
	return (1);
   }
   else { /* decode-able */
	msg_printf(ERR,"\n");
	msg_printf(ERR,"Address 0x%llx (bad bits location = ",  paddr);
	msg_printf(ERR,"0x%x) decodes to: \n",badbits);
	msg_printf(ERR,"\tSlot 0x%x, leaf 0x%x, bank 0x%x",*slotp,leaf,bank);
	if (!badbits) /* no simm info */
		msg_printf(ERR,"\n");
   	else { /* has simm info */
	   switch (bank) {
		case 0: letr = (leaf ? 'b': 'a'); break;
		case 1: letr = (leaf ? 'd': 'c'); break;
		case 2: letr = (leaf ? 'f': 'e'); break;
		case 3: letr = (leaf ? 'h': 'g'); break;
		default: break;
	   }
	   if ((simm == SIMM0BAD) || (simm == SIMM1BAD) || (simm==SIMM2BAD)
		|| (simm == SIMM3BAD))  { /*  only 1 simm  bad */
		msg_printf(ERR,", simm ");
		switch (simm) {
			case SIMM0BAD: msg_printf(ERR,"0(%c0)\n",letr); break;
			case SIMM1BAD: msg_printf(ERR,"1(%c1)\n",letr); break;
			case SIMM2BAD: msg_printf(ERR,"2(%c2)\n",letr); break;
			case SIMM3BAD: msg_printf(ERR,"3(%c3)\n",letr); break;
			default: break;
		}
	   }
	   else  {/* multiple SIMMs bad */
		msg_printf(ERR, ", simms: "); 
	   	if (simm & SIMM0BAD)
			msg_printf(ERR,"0(%c0) ",letr);
	   	if (simm & SIMM1BAD)
			msg_printf(ERR,"1(%c1) ",letr);
	   	if (simm & SIMM2BAD)
			msg_printf(ERR,"2(%c2) ",letr);
	   	if (simm & SIMM3BAD)
			msg_printf(ERR,"3(%c3)",letr);
	   	msg_printf(ERR,"\n");
	   }
	} /* has simm info */
   } /* decode-able */
   return (0);
} /* decode_it() */


int addr2slbs(
  __psunsigned_t addr,uint badbits,int *slot,int *leaf,int *bank,
	int *simm,int size)
{
    register int s, l, b;
    register short enable, simm_type, i_factor, i_position;
    register uint base;
    int not_in_any_banks = 1;

    msg_printf(DBG,"addr2slbs starting\n");
    *slot = 0xbad; /* initialize in case no one else sets it! */
    if ((size != SW_BYTE) && (size != SW_HALFWORD) && (size != SW_WORD)) {
	msg_printf(ERR, "Error: specified size, %d, is unrecognized\n",size);
	return (1);
    }
    for (s = 0; s < EV_MAX_SLOTS; s++) {
	if (board_type(s) == EVTYPE_MC3) {
            msg_printf(DBG,"Board %x is memory\n", s);
            /* It's a memory board, do leaf loop */
            for (l = 0; l < MC3_NUM_LEAVES; l++) {
                msg_printf(DBG,"leaf %x\n", l);
                for (b = 0; b < MC3_BANKS_PER_LEAF; b++) {
                    msg_printf(DBG,"bank %x\n", l);
                /* Nonzero means enabled */
                    /* nonzero == enabled */
                    enable = (short)(read_reg(s, MC3_BANKENB) & MC3_BENB(l, b));
                    msg_printf(DBG,"Enable = %x\n", enable);
                    simm_type = (short)(read_reg(s, MC3_BANK(l, b, BANK_SIZE)));
                    msg_printf(DBG,"Type = %x\n", simm_type);
                    i_factor = (short)(read_reg(s, MC3_BANK(l, b, BANK_IF)));
                    i_position = (short)(read_reg(s, MC3_BANK(l, b, BANK_IP)));
                    base = (uint)(read_reg(s, MC3_BANK(l, b, BANK_BASE)));
                    msg_printf(DBG,"Base = %x\n", base);
                    if (enable && myin_bank(addr, base, i_factor, i_position,
                                                                simm_type)) {
                        /* We have a winner! */
                        *slot = s;
                        /* Banks are in order within the slots */
                        *bank = b;
			*leaf = l;
			/* I don't think this old way works ... */
                        /* Check which simm it's in:
                         *      Each SIMM holds an eighth of cache line
                         *  so divide by 128/8 bytes (2^(7-3))
                         */
                        /* *simm = (addr >> 4) & 0x3; */

			/* Returns 4-bit value indicating which simms are
			 * bad: 1=simm0, 2= simm1, 4=simm2, 8=simm3.
			 * simm = simm0 | simm1 | simm2 | simm3
			 * E.G. if simms1,2 are both bad, simm = 6.
			*/
			*simm = in_simm(addr, badbits, size);
			not_in_any_banks = 0;
                        return 0;
                    } else {
                        msg_printf(DBG,"Disabled!");
                    } /* if enable... */
                }
            }
        } /* If board_type(s)... */
    } /* for s */
    if (not_in_any_banks) {
	*slot = 0xbad;
 	msg_printf(ERR, "Address 0x%llx is out of range\n",  addr);
/*	err_msg(DECODE_RANGE_ERR, &slot,  (uint) addr); */
	return (1);
    }
    return 0;
} /* addr2slbs */

/* myin_bank returns 1 if the given address is in the same leaf position
 * as the bank whose interleave factor and position are provided
 * and the address is in the correct range.
 */
int myin_bank(__psunsigned_t address, uint base, uint i_factor, 
		uint i_position,uint simm_type)
{
        __psunsigned_t end;

        end = (__psunsigned_t)(base + MemSizesMC3[simm_type] * MC3_INTERLEAV(i_factor));
	/* left shift in 8 0's since end is in 256-byte chunks */
	end = (end << 8);
        msg_printf(DBG,"in_bank End == %x\n", end);

        /* If it's out of range, return 0 */
        if (address < base || address >= end) {
                /* Nope. */
                return 0;
	}

        /* Is this bank the right position in the "interleave"?
         * See if the "block" address falls in _our_ interleave position
         */
        if (((address >> 0x7) % MC3_INTERLEAV(i_factor)) == i_position)
                /* We're the one */
                return 1;

        /* Nope. */
        return 0;
} /* myin_bank */

/* in_simm returns the simm number (0-3) which correspond to the given
 * badbits location
 * as the bank whose interleave factor and position are provided
 * and the address is in the correct range.
 */

int in_simm(unsigned long long addr, uint badbits, int size)
{
	register uint md3bytes, md2bytes, md1bytes, md0bytes;
	int badsimms = 0;
	uint badbytes = 0x0; /* if a bit, n, in badbytes = 1, byte n is bad */
	register int bytes_per_block = 128;
	register MSbyte_in_block_BE; /* Most significant byte's address */

	/* From the mc3 spec: The following mds control the 
	 * following bytes. The mds correspond to simms, so md3 controls
	 * simm3, md2 controls simm2, etc. 
	 * We then match the badbytes to the bytes controlled by each md
	 * to find which simms are bad
	 *
	 * Big-Endian, SubBlock = 0,1 (difference is in EBus cycle # only)
	 *	md3:			md2:
	 *	120,121,112,113,	122,123,114,115,
	 *	104,105, 96, 97,	106,107, 98, 99,
	 *	 88, 89, 80, 81, 	 90, 91, 82, 83,
	 *	 72, 73, 64, 65,	 74, 75, 66, 67,
	 *	 56, 57, 48, 49, 	 58, 59, 50, 51,
	 *	 40, 41, 32, 33,	 42, 43, 34, 35,
	 *	 24, 25, 16, 17,	 26, 27, 18, 19,
	 *	  8,  9,  0,  1		 10, 11,  2,  3
	 *
	 *	md1:			md0:
	 *	124,125,116,117,	126,127,118,119,
	 *	108,109,100,101,	110,111,102,103,
	 *	 92, 93, 84, 85,	 94, 95, 86, 87,
	 *	 76, 77, 68, 69,	 78, 79, 70, 71,
	 *	 60, 61, 52, 53,	 62, 63, 54, 55,
	 *	 44, 45, 36, 37,	 46, 47, 38, 39,
	 *	 28, 29, 20, 21, 	 30, 31, 22, 23,
	 *	 12, 13,  4,  5		 14, 15,  6,  7
	*/

	/* bit representations */
	md3bytes = 0x03030303;
	md2bytes = 0x0c0c0c0c;
	md1bytes = 0x30303030;
	md0bytes = 0xc0c0c0c0;

	MSbyte_in_block_BE = (unsigned long long)addr % bytes_per_block;

	switch (size) {
		case SW_BYTE: badbits <<= 24; break;
		case SW_HALFWORD: badbits <<= 16; break;
		default: break;
	}

	if (badbits & 0xff000000)
		badbytes |= (1 << MSbyte_in_block_BE);
	if (badbits & 0xff0000)
		badbytes |= (1 << (MSbyte_in_block_BE + 1));
	if (badbits & 0xff00)
		badbytes |= (1 << (MSbyte_in_block_BE + 2));
	if (badbits & 0xff)
		badbytes |= (1 << (MSbyte_in_block_BE + 3));

	msg_printf(DBG,"MSbyte_in_block_BE is 0x%x\n", MSbyte_in_block_BE);
	msg_printf(DBG,"badbits is 0x%x\n", badbits);
	msg_printf(DBG,"badbytes is 0x%x\n", badbytes);
	if (badbytes & md0bytes)
		badsimms |= SIMM0BAD;
	if (badbytes & md1bytes)
		badsimms |= SIMM1BAD;
	if (badbytes & md2bytes)
		badsimms |= SIMM2BAD;
	if (badbytes & md3bytes)
		badsimms |= SIMM3BAD;

	msg_printf(DBG,"badsimms is 0x%x\n", badsimms);
	return (badsimms);
} /* in_simm */

uint mc3slots(void)
{
	register int i;
	register uint slots = 0x0;

	for (i = 0; i < EV_MAX_SLOTS; i++)
        	if (board_type(i) == EVTYPE_MC3) 
			slots |= (1 << i);
	return (slots);
}

void enable_bnk()
{
	enable_disable_bnk(1);
}

void disable_bnk()
{
	enable_disable_bnk(0);
}

int enable_disable_bnk(int enable)
{
	uint bank, leaf, slot;
	int targ_bank, targ_leaf, targ_base, targ_slot;
	int more = 1;
	uint targ_bnkenablreg = 0;
	uint bnkenablbit = 0;	
	uint new_bnkenablreg, current_bnkenablreg;
	char answ[16], str[16];
	int i, j;
	int goodchoice = 1;

	msg_printf(DBG,"enable is %d\n", enable);
	msg_printf(SUM,"\nThis command allows you to ");
	if (enable)
		msg_printf(SUM,"enable ");
	else
		msg_printf(SUM,"disable ");
	msg_printf(SUM,"one bank at a time. To specify more than 1\n");
	msg_printf(SUM,"bank, just run this command again.\n\n");
	msg_printf(SUM,"Memory boards recognized in slot(s): ");
	for (slot = 0; slot < EV_MAX_SLOTS; slot++) 
        	if (board_type(slot) == EVTYPE_MC3)  {
			msg_printf(SUM,"%d ", slot);
		}
	msg_printf(SUM,"\n");
	msg_printf(SUM,"Enter slot number: ");
	gets(str);	
	atob(str, (int *)&targ_slot);
	if (board_type(targ_slot) != EVTYPE_MC3) {
		msg_printf(SUM,"WARNING: The board in slot %d does not ", 
			targ_slot);
		msg_printf(SUM,"appear to be a memory board.\n");
		msg_printf(SUM,"Continuing may cause a system PANIC.\n\n");
questloop:
		msg_printf(SUM,"Do you want to continue? (y=1, n=0): ");
		gets(answ);
                   if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                        if (!strcmp(answ, "0")) {
                                return (0);
                        }
                   }
                   else {
                        msg_printf(SUM,"Please enter a 1 or a 0\n");
                        goto questloop;
                   }
	}
	msg_printf(SUM,"Current bank configuration:\n");
	for (leaf = 0; leaf < MC3_NUM_LEAVES; leaf++)
		for (bank = 0; bank < MC3_BANKS_PER_LEAF; bank++) {
			msg_printf(SUM,"Leaf %d, Bank %d: ", leaf, bank);
			if (read_reg(targ_slot,MC3_BANKENB) & 
						MC3_BENB(leaf,bank))
				msg_printf(SUM,"Enabled\n");
			else
				msg_printf(SUM,"Disabled\n");
		}
	while (more) {
		msg_printf(SUM,"Enter leaf number: ");
		gets(str);
		atob(str, (int *)&targ_leaf);
		msg_printf(SUM,"Enter bank number: ");
                gets(str);
                atob(str, (int *)&targ_bank);
		if (enable) {
		   msg_printf(SUM,"Enter bank base in hex (e.g. 0x2000): ");
			gets(str);
			atob(str, (int *)&targ_base);
		}
		if ((targ_leaf > (MC3_NUM_LEAVES-1)) || 
			(targ_bank > (MC3_BANKS_PER_LEAF-1)))			
			msg_printf(SUM,
			   "ERROR: Leaf or Bank number is out of range.\n");
		else {
			/* check for illegal actions */
			if (enable) 
			{
			   if ((i = (int)(read_reg(targ_slot, 
				MC3_BANK(targ_leaf, targ_bank, BANK_SIZE))))==7) 
			   {
				more = 0;
				msg_printf(SUM,"That bank has no memory. ");
				msg_printf(SUM,
				 "Trying to enable it may hang the system.\n");
				msg_printf(SUM,
				   "Do you still want to try and enable it? (yes=1, no=0): ");
				gets(str);
				atob(str, (int *)&more);
				if (more == 0)
					goodchoice = 0;
			   }
			}
			else { /* disable */
			   if ((i = (int)(read_reg(targ_slot,
                                MC3_BANK(targ_leaf, targ_bank, BANK_IF)))) > 0)
			   {
				more = 0;
				msg_printf(SUM,"That bank is interleaved. ");
				msg_printf(SUM,"Trying to disable it may hang the system.\n");
				msg_printf(SUM,
				   "Do you still want to try and disable it? (yes=1, no=0): ");
				gets(str);
				atob(str, (int *)&more);
				if (more == 0) 
                                	goodchoice = 0;
                           }
                        }
			if (goodchoice) {
			   switch(targ_leaf) {
				case 0: switch (targ_bank) {
					  case 0: bnkenablbit = 0x1; break;
					  case 1: bnkenablbit = 0x2; break;
					  case 2: bnkenablbit = 0x10; break;
					  case 3: bnkenablbit = 0x20; break;
					  default: bnkenablbit = 0; break;
					}
					break;
				case 1: switch (targ_bank) {
                                          case 0: bnkenablbit = 0x4; break;
                                          case 1: bnkenablbit = 0x8; break;
                                          case 2: bnkenablbit = 0x40; break;
                                          case 3: bnkenablbit = 0x80; break;
                                          default: bnkenablbit = 0; break;
                                        }
                                        break;
				default: msg_printf(SUM,
                           	   "ERROR: Leaf number is out of range.\n");
			   } /* switch */
			   targ_bnkenablreg |= bnkenablbit;
			} /* goodchoice */
		} /* else */		
		more = 0;
		/* msg_printf(SUM,"More banks to enter? (yes=1, no=0): ");
		gets(str);
		atob(str, (int *)&more); */
	} /* more */
	/* verify changes with user */
	current_bnkenablreg = (uint)(read_reg(targ_slot, MC3_BANKENB));
	msg_printf(DBG,"Current reg: 0x%x\n", current_bnkenablreg);
	msg_printf(DBG,"Proposed reg changes: 0x%x\n", targ_bnkenablreg);
	if (enable)
		new_bnkenablreg = current_bnkenablreg | targ_bnkenablreg;
	else
		new_bnkenablreg = current_bnkenablreg ^ targ_bnkenablreg;	
	msg_printf(DBG,"Proposed new reg: 0x%x\n", new_bnkenablreg);
	if (targ_bnkenablreg == 0) {
		msg_printf(SUM,"No changes made\n");
		return (0);
	}
	msg_printf(SUM,"You requested the following changes for the memory");
	msg_printf(SUM," board in slot %d: \n", targ_slot);
	if (enable)
		msg_printf(SUM,"Enable:\n");
	else
		msg_printf(SUM,"Disable:\n");
 	for (i = 1, j = 1; j <= 8; j++) {
		msg_printf(DBG,"i is 0x%x\n", i);
		if (i & targ_bnkenablreg) {
			switch (i) {
			  case 0x1: msg_printf(SUM,"\tleaf 0,bank 0\n");break;
			  case 0x2: msg_printf(SUM,"\tleaf 0,bank 1\n");break;
			  case 0x4: msg_printf(SUM,"\tleaf 1,bank 0\n");break;
			  case 0x8: msg_printf(SUM,"\tleaf 1,bank 1\n");break;
			  case 0x10: msg_printf(SUM,"\tleaf 0,bank 2\n");break;
			  case 0x20: msg_printf(SUM,"\tleaf 0,bank 3\n");break;
			  case 0x40: msg_printf(SUM,"\tleaf 1,bank 2\n");break;
			  case 0x80: msg_printf(SUM,"\tleaf 1,bank 3\n");break;
			};
		}
		i <<= 1;
	}
	if (enable) {
		msg_printf(SUM,"\tBank base = 0x%x\n", targ_base);
	}
	more = 0;
	msg_printf(SUM,"Are you sure? (yes=1, no=0): ");
	gets(str);
	atob(str, (int *)&more);
	if (more) {
		msg_printf(DBG,"Writing config reg\n");
    store_double_lo((long long *)EV_CONFIGADDR((targ_slot),0,(MC3_BANKENB)), new_bnkenablreg);
    store_double_lo((long long *)EV_CONFIGADDR((targ_slot),0,(MC3_BANK(targ_leaf, targ_bank, BANK_BASE))), targ_base);
/*
		EV_SET_CONFIG(targ_slot, MC3_BANKENB, new_bnkenablreg);
		EV_SET_CONFIG(targ_slot, 
			MC3_BANK(targ_leaf, targ_bank, BANK_BASE), targ_base);
*/
	}
	return (0);
}

/* check word writes to memory */
int
wcheck_mem(__psunsigned_t ptr)
{
	uint slot;
	__psunsigned_t badaddr;

        if ((ptr % 0x10000) == 0)
            msg_printf(VRB, "addr 0x%llx data 0x%x\r",
#if _MIPS_SIM == _ABI64
                     ptr, *(uint *)ptr);
#else
                     vir2phys(ptr), *(uint *)ptr);
#endif
        if ((ptr % 0x80) == 0)
            if (cpu_intr_pending()) {
                        badaddr = vir2phys(ptr);
                        decode_address(badaddr,0, SW_WORD, (int *) &slot);
                        err_msg(MC3_INTR_ERR, &slot, badaddr);
			if (chk_memereg(mem_slots)) {
			    check_memereg(mem_slots);
			    mem_err_clear(mem_slots);
			}
            }
	return 0;
} /* wcheck_mem */

/* handle word compare failures */
int
wfail_mem(__psunsigned_t ptr, __psunsigned_t expect, __psunsigned_t actual, int err_type)
{
	uint slot;
	__psunsigned_t badaddr;
	int fail = 0;
	__psunsigned_t badbits = 0;

        fail =1;
        badbits = actual ^ expect;
        badaddr = vir2phys(ptr);
#if _MIPS_SIM == _ABI64
        decode_address(K1_TO_PHYS(badaddr), badbits, SW_WORD, (int *)&slot);
#else
        decode_address(badaddr, badbits, SW_WORD, (int *)&slot);
#endif
        err_msg(err_type, &slot, badaddr, expect, actual);
	if (chk_memereg(mem_slots)) {
	    check_memereg(mem_slots);
	    mem_err_clear(mem_slots);
	}
	return (fail);
} /* wfail_mem */


/* check byte writes to memory */
int
bcheck_mem(__psunsigned_t ptr)
{
	uint slot;
	__psunsigned_t badaddr;

        if ((ptr % 0x10000) == 0)
            msg_printf(VRB, "addr 0x%llx data 0x%x\r",
                     vir2phys(ptr), *(char *)ptr);
        if ((ptr % 0x80) == 0)
            if (cpu_intr_pending()) {
                        badaddr = vir2phys(ptr);
                        decode_address(badaddr,0, SW_BYTE, (int *) &slot);
                        err_msg(MC3_INTR_ERR, &slot, badaddr);
			if (chk_memereg(mem_slots)) {
			    check_memereg(mem_slots);
			    mem_err_clear(mem_slots);
			}
            }
	return 0;
} /* bcheck_mem */

/* handle byte compare failures */
int
bfail_mem(__psunsigned_t ptr, unsigned char expect, unsigned char actual,
	  int err_type)
{
	unsigned char slot;
	__psunsigned_t badaddr;
	int fail = 0;
	unsigned char badbits = 0;

        fail =1;
        badbits = actual ^ expect;
        badaddr = vir2phys(ptr);
#if _MIPS_SIM == _ABI64
        decode_address(K1_TO_PHYS(badaddr), (__psunsigned_t)badbits, SW_BYTE, (int *)&slot);
#else
        decode_address(badaddr, (__psunsigned_t)badbits, SW_BYTE, (int *)&slot);
#endif
        err_msg(err_type, &slot, badaddr, expect, actual);
	if (chk_memereg(mem_slots)) {
	    check_memereg(mem_slots);
	    mem_err_clear(mem_slots);
	}
	return (fail);
} /* bfail_mem */

/* check double word writes to memory */
void dwcheck_mem(__psunsigned_t ptr)
{
	uint slot;
	__psunsigned_t badaddr;

        if ((ptr % 0x100000) == 0)
            msg_printf(VRB, "addr 0x%x data 0x%x\r",
                     ptr, *(unsigned long long *)ptr);

        if ((ptr % 0x80) == 0)
            if (cpu_intr_pending())
	    {
                        badaddr = vir2phys(ptr);
                        decode_address(badaddr,0, SW_WORD, (int *)&slot);
                        err_msg(MC3_INTR_ERR, &slot, badaddr);
			if (chk_memereg(mem_slots)) {
			    check_memereg(mem_slots);
			    mem_err_clear(mem_slots);
			}
            }
} /* dwcheck_mem */

/* handle double word compare failures */
int
dwfail_mem(__psunsigned_t ptr, unsigned long long expect, unsigned long long actual, int err_type)
{
	uint slot;
	__psunsigned_t badaddr;
	int fail = 0;
	unsigned long long badbits = 0;

        fail =1;
        badbits = actual ^ expect;
    	badaddr = (__psunsigned_t)ptr;
#if _MIPS_SIM == _ABI64
    	decode_address(K1_TO_PHYS(badaddr), badbits, SW_WORD, (int *)&slot);
#else
        decode_address(badaddr, badbits, SW_WORD, (int *)&slot);
#endif
        err_msg(err_type, &slot, badaddr, expect, actual);

	if (chk_memereg(mem_slots)) {
	    check_memereg(mem_slots);
	    mem_err_clear(mem_slots);
	}
	return (fail);
} /* dwfail_mem */


int
chk_memereg(int slot_arr)
/* slot_arr : 'i'th bit set for MC3 in slot i.
   Return Value : 2 if error register in any of MC3 specified is set | 
		  1 if the CPU error registers have any bit set.
		  0 Otherwise.
*/
{

    int		i,j, result=0;

    for (i=1; i < EV_MAX_SLOTS; i++){
	if ((slot_arr & (1 << i)) == 0)
	    continue;
	
	for (j = 0; j < MC3_NUM_LEAVES; j++) {
	    result = (int)(MC3_GETLEAFREG(i,j,MC3LF_ERROR));
	}
	if (result) {
	   	break; /* Since there is some error, why continue */
	}

    }
    return(result);
}


static int toggle;
static u_int intrtime;

#define THREESEC 3
#define FOURSEC	 4
#define FIVESEC	5
/*
 * E n a b l e T i m e r - Enables the r4k timer interrupts in the interval
 * passed in by the user. Only a few supported for now.
 */
void
EnableTimer(uint interval)
{
	toggle = 1;

	/* Set counter to zero */
	set_count(0x0);

	/* Don't use a switch statement or the code will jump
	 * into uncached space because the coded is linked in k1 space.
	 * NOTE: Three and five second delays haven't been timed, I guessed.
	 */
	if( interval == THREESEC)
		intrtime = 0x09600000;
	else if( interval == FOURSEC)
		intrtime = 0x0c800000;
	else if( interval == FIVESEC)
		intrtime = 0x0fa00000;
	else
		intrtime = 0x0c800000;


#ifdef IP19			/* no compare on IP21 */
	/* Set Compare reg to user specified intervals */
	SetCompare( intrtime);

	/* Set counter to zero */
	set_count(0x0);
#else
	/* for IP21, count[31] is wired to IP[10] */
	set_count(0x80000000 - intrtime);
#endif

	/* Enable timer interrupts */
	SetSR( GetSR() | SR_IE | SR_IBIT8);

}


/*
 * T i m e r H a n d l e r - Handles the R4k timer interrupt, plus prints
 * the spining wheel to show progress.
 */
int TimerHandler(void)
{
	/* Reset the counter to zero */
	set_count(0x0);

#ifdef IP19
	/* Clear the Interrupt */
	SetCompare(intrtime);
	set_count(0x0);
#else
	set_count(0x80000000 - intrtime);
#endif

	if( toggle == 0) {
		printf("|\b");
		toggle++;
	}
	else {
		if( toggle ==1 ) {
			printf("/\b");
			toggle++;
		}
		else
		if( toggle ==2 ) {
			printf("-\b");
			toggle++;
		}
		else
		if( toggle ==3 ) {
			printf("\\\b");
			toggle++;
		}
		else
		if( toggle ==4 ) {
			printf("|\b");
			toggle++;
		}
		else

		if( toggle ==5 ) {
			printf("/\b");
			toggle++;
		}
		else
		if( toggle ==6 ) {
			printf("-\b");
			toggle++;
		}
		else
		if( toggle ==7 ) {
			printf("\\\b");
			toggle = 0;
		}
	}

	return(0);
}

/* disables the r4k timer interrupt */
void StopTimer(void)
{
#ifdef IP19
	/* Satisfy any pending interrupt */
	/* Use large value to avoid another intr */
	SetCompare(0xff000000);
#endif

	/* Clear interrupt enable bits */
	SetSR( GetSR() & ~( SR_IBIT8 | SR_IE) );

}

void dis_ecc_intr(__psunsigned_t osr)
{
	__psunsigned_t oile;

        SetSR(osr);
        /* Disable level 3 intrs. */
        EV_SET_LOCAL(EV_ILE, oile);
}

void en_ecc_intr(__psunsigned_t osr)
{
	__psunsigned_t oile, tmp;

        SetSR(osr | ~SR_IE);

        if (tmp = EV_GET_LOCAL(EV_ERTOIP))
            /* clear whatever is currently pending */
            EV_SET_LOCAL(EV_CERTOIP, tmp);

	/* Enable level 3 intrs. */
	oile = EV_GET_LOCAL(EV_ILE);
	/* EV_ERTOINT_MASK is 0x8 which sets the CC intr level to 3 */
	EV_SET_LOCAL(EV_ILE, oile | EV_ERTOINT_MASK);
	msg_printf(DBG, "en_ecc_intr:EV_ILE contents set to 0x%llx\n", EV_GET_LOCAL(EV_ILE));

	/* enable interrupt for level 3 */
	set_sr(osr | SR_IBIT6 | SR_IE);
	msg_printf(DBG, "en_ecc_intr:Status register set to 0x%x\n", get_sr());
}

void handle_ecc_intr(void)
{
	__psunsigned_t tmp, cpu_loc[2];

	getcpu_loc(cpu_loc);
	/* check IP in cause register */
	tmp = get_cause();
	msg_printf(DBG, "handle_intr:CAUSE contents = 0x%x\n", tmp);
	if (tmp & CAUSE_IP6)
	    msg_printf(DBG, "handle_intr:Received level 3 interrupt!\n");
	else
	{
	    err_msg(MC3INTR_IP6, cpu_loc);
	}

	/* check ERTOIP register */
	tmp = EV_GET_LOCAL(EV_ERTOIP);
	msg_printf(DBG, "handle_intr:ERTOIP contents = 0x%llx\n", tmp);
	if (tmp & 0x3)
	    err_msg(MC3INTR_ERTOIP, cpu_loc);
	else
	{
	    msg_printf(DBG, "handle_intr:non-ECC error occured\n");
	    show_fault();
	}

	/* clear pending interrupt */
	EV_SET_LOCAL(EV_CERTOIP, tmp);
	tmp = EV_GET_LOCAL(EV_RO_COMPARE); /* delay */
	if (tmp = EV_GET_LOCAL(EV_ERTOIP))
	    err_msg(MC3INTR_CERTOIP, cpu_loc, tmp);

	/* check cause cleared */
	if (get_cause() & CAUSE_IP6)
	    err_msg(MC3INTR_IP6NC, cpu_loc, get_cause());
}

void memtest_init(void)
{
    register int slot;
    /*
     * from stand/arcs/lib/libsk/ml/cache.s
     * invalidates all caches. Added because of the R4000 uncached writeback
     * bug - makes *sure* that there are no valid cache lines
     */
#ifndef IP25
    ide_invalidate_caches();
#endif
    for (slot = 0; slot < EV_MAX_SLOTS; slot++)
        if (board_type(slot) == EVTYPE_MC3)
                setup_err_intr(slot, 0); /* 0 is a no op for mc3 */

    /* clear cause register */
    set_cause(0);

    /* clear fpu status register */
    SetFPSR(0);
}
