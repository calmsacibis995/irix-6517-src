#ident "$Header: "
/*
 * SN0 Address Analyzer
 * This file mostly comes from stand/arcs/tools/hwreg/main.c
 * but has also local things needed for icrash.
 * It contains things that are common for several 'hwreg' type
 * commands such as 'hubreg' and 'dirmem'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "icrash.h"
#include "extern.h"
#include "hwregcmd.h"
#include "hwreg.h"

char    opt_addr[80];
int     opt_nmode;
int     opt_hub;
int     opt_router;
int     opt_xbow;
int     opt_c0;
int     opt_dir;
int     opt_list;
int	opt_base	= 16;
int     opt_node;
int     opt_node_given;
int     opt_short;
i64	opt_calias	= 16 * MB;
int     opt_sn00;
int     opt_bit;
int     opt_premium;
i64     opt_syn;
int     opt_syn_given;
i64     winsize;


/*
 * Syndrome to bit mapping
 *
 *   A syndrome of zero indicates no errors.  A syndrome in the table below
 *   indicates a single check bit or data bit correctable error.  A syndrome
 *   not in the table indicates a multiple-bit uncorrectable error.
 */

typedef struct synent_s {
    uchar_t		syn, bit;
} synent_t;

static synent_t synmap_mem[] = {
    /* Data bits */
    { 0xc8, 71 }, { 0xc4, 70 }, { 0xc2, 69 }, { 0xc1, 68 },
    { 0xf4, 67 }, { 0x8f, 66 }, { 0xe0, 65 }, { 0xb0, 64 },
    { 0x0e, 63 }, { 0x0b, 62 }, { 0xf2, 61 }, { 0x1f, 60 },
    { 0x86, 59 }, { 0x46, 58 }, { 0x26, 57 }, { 0x16, 56 },
    { 0x38, 55 }, { 0x34, 54 }, { 0x32, 53 }, { 0x31, 52 },
    { 0xa8, 51 }, { 0xa4, 50 }, { 0xa2, 49 }, { 0xa1, 48 },
    { 0x98, 47 }, { 0x94, 46 }, { 0x92, 45 }, { 0x91, 44 },
    { 0x58, 43 }, { 0x54, 42 }, { 0x52, 41 }, { 0x51, 40 },
    { 0x8a, 39 }, { 0x4a, 38 }, { 0x2a, 37 }, { 0x1a, 36 },
    { 0x89, 35 }, { 0x49, 34 }, { 0x29, 33 }, { 0x19, 32 },
    { 0x85, 31 }, { 0x45, 30 }, { 0x25, 29 }, { 0x15, 28 },
    { 0x8c, 27 }, { 0x4c, 26 }, { 0x2c, 25 }, { 0x1c, 24 },
    { 0x68, 23 }, { 0x64, 22 }, { 0x62, 21 }, { 0x61, 20 },
    { 0xf8, 19 }, { 0x4f, 18 }, { 0x70,	17 }, { 0xd0, 16 },
    { 0x07, 15 }, { 0x0d, 14 }, { 0xf1,	13 }, { 0x2f, 12 },
    { 0x83, 11 }, { 0x43, 10 }, { 0x23,	 9 }, { 0x13,  8 },
    /* Check bits */
    { 0x80, 128 | 7 }, { 0x40, 128 | 6 }, { 0x20, 128 | 5 },
    { 0x10, 128 | 4 }, { 0x08, 128 | 3 }, { 0x04, 128 | 2 },
    { 0x02, 128 | 1 }, { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

static synent_t synmap_pdir[] = {
    /* Data bits */
    { 0x70, 47 }, { 0x0b, 46 }, { 0x07, 45 }, { 0x43, 44 },
    { 0x7c, 43 }, { 0x1f, 42 }, { 0x45, 41 }, { 0x4c, 40 },
    { 0x51, 39 }, { 0x61, 38 }, { 0x46, 37 }, { 0x4a, 36 },
    { 0x64, 35 }, { 0x23, 34 }, { 0x73, 33 }, { 0x6e, 32 },
    { 0x25, 31 }, { 0x29, 30 }, { 0x31, 29 }, { 0x26, 28 },
    { 0x2a, 27 }, { 0x62, 26 }, { 0x2c, 25 }, { 0x1a, 24 },
    { 0x5d, 23 }, { 0x2f, 22 }, { 0x13, 21 }, { 0x32, 20 },
    { 0x52, 19 }, { 0x16, 18 }, { 0x34, 17 }, { 0x54, 16 },
    { 0x15, 15 }, { 0x68, 14 }, { 0x0d, 13 }, { 0x1c, 12 },
    { 0x19, 11 }, { 0x0e, 10 }, { 0x38,	 9 }, { 0x58,  8 },
    { 0x49,  7 },
    /* Check bits */
    { 0x40, 128 | 6 }, { 0x20, 128 | 5 }, { 0x10, 128 | 4 },
    { 0x08, 128 | 3 }, { 0x04, 128 | 2 }, { 0x02, 128 | 1 },
    { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

static synent_t synmap_sdir[] = {
    /* Data bits */
    { 0x1f, 15 }, { 0x1a, 14 }, { 0x13, 13 }, { 0x07, 12 },
    { 0x0e, 11 }, { 0x0b, 10 }, { 0x16,	 9 }, { 0x15,  8 },
    { 0x19,  7 }, { 0x1c,  6 }, { 0x0d,	 5 },
    /* Check bits */
    { 0x10, 128 | 4 }, { 0x08, 128 | 3 }, { 0x04, 128 | 2 },
    { 0x02, 128 | 1 }, { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

void listacc(void)
{
    int		i;

    P0("Access  Description\n");

    for (i = 0; hwreg_accmodes[i].name; i++)
	P2("%-6s%s\n",
	       hwreg_accmodes[i].name, hwreg_accmodes[i].desc);

    return;
}

void listreg(hwreg_set_t *regset, hwreg_t *r)
{
    P2("%-24s (%06llx)\n", regset->strtab + r->nameoff, r->address);
}

void listreg_matches(hwreg_set_t *regset, char *name)
{
    char	caps[256];
    int		len	= strlen(name);
    int		i;

    hwreg_upcase(caps, name);

    for (i = 0; regset->regs[i].nameoff; i++)
	if (strncmp(caps,
		    regset->strtab + regset->regs[i].nameoff,
		    len) == 0)
	    listreg(regset, &regset->regs[i]);
}

/*
 * mdir_xlate_syndrome
 *
 *   Translates a syndrome into a string. Input "type" should be 0 for
 *   memory, 1 for premium directory, or 2 for standard directory.
 *
 *   - For no errors, says "good".
 *
 *   - For a single bit error in the data, says "data00" to "data63", "dir00"
 *     to "dir40", or "dir00" to "dir10", according to type 0, 1, or 2,
 *     respectively.
 *
 *   - For a single bit error in check bits, says "check0" to "check7",
 *     "check0" to "check6", or "check0" to "check4", according to type
 *     0, 1, or 2, respectively.
 *
 *   - For a multiple-bit uncorrectable error, says "multi".
 *
 *   Returns the SIMM bit number for single bit memory errors (71 to 0).
 */

int mdir_xlate_syndrome(char *buf, int type, __uint64_t syn)
{
    int			r	= -1;
    synent_t	       *table;
    char	       *bitname;
    int			bitnum, bit;
    __uint64_t		forced, x;

    switch (type) {
    case 0:
	table = synmap_mem;
	bitname = "data";
	bitnum = 8;
	forced = 0xff;
	break;
    case 1:
	table = synmap_pdir;
	bitname = "dir";
	bitnum = 7;
	forced = 0x7f;
	break;
    case 2:
	table = synmap_sdir;
	bitname = "dir";
	bitnum = 5;
	forced = 0x1f;
	break;
    }

    sprintf(buf, "0x%02llx (", syn);
    while (*buf)
	buf++;

    if (syn == 0)
	strcpy(buf, "good");
    else if (syn == forced)
	strcpy(buf, "forced_multi");
    else {
	int		i;

	for (i = 0; (x = (__uint64_t) table[i].syn) != 0; i++)
	    if (x == syn)
		break;

	bit = table[i].bit;

	if (x == 0)
	    strcpy(buf, "multi");
	else if (bit & 128) {
	    r = bit & 127;
	    sprintf(buf, "check_%d", r);
	} else {
	    r = bit;
	    sprintf(buf, "%s_%02d", bitname, bit - bitnum);
	}
    }

    strcat(buf, ")");

    return r;
}

/*
 * mdir_xlate_addr_mem
 *
 *   Expects a physical memory address and a SIMM bit from 71:0, where bits
 *   71:8 correspond to data 63:0 and 7:0 correspond to the check bits.
 */

void mdir_xlate_addr_mem(char *buf, __uint64_t address, int bit)
{
    __uint64_t		bank_dimm, bank_lgcl, bitbase;

    bank_dimm = BANK(address);
    bank_lgcl = (bit >= 36);

    bitbase = (((address & 8) ? 18 : 0) +
	       (bit >= 18 && bit < 36 || bit >= 54 ? 36 : 0));

    if (opt_sn00) {
	if (bit < 0)
	    sprintf(buf, "SLOT%lld and/or SLOT%lld",
		    bank_dimm * 2 + 1, bank_dimm * 2 + 2);
	else
	    sprintf(buf, "SLOT%lld line %lld",
		    bank_dimm * 2 + bank_lgcl + 1,
		    bitbase + (bit % 18));
    } else {
	if (bit < 0)
	    sprintf(buf, "MM%cH%c and/or MM%cL%c",
		    bank_dimm < 4 ? 'X' : 'Y',
		    (int) (bank_dimm + '0'),
		    bank_dimm < 4 ? 'X' : 'Y',
		    (int) (bank_dimm + '0'));
	else
	    sprintf(buf, "MM%c%c%c line %lld",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_lgcl ? 'H' : 'L',
		    (int) (bank_dimm + '0'),
		    bitbase + (bit % 18));
    }
}

/*
 * mdir_xlate_addr_pdir
 *
 *   Expects a physical memory address and a SIMM bit from 47:0, where bits
 *   47:7 correspond to data 40:0 and 6:0 correspond to the check bits.
 */

void mdir_xlate_addr_pdir(char *buf, __uint64_t address, int bit,
			  int dimm0_sel)
{
    __uint64_t		bank_dimm, bank_phys;

    if (opt_nmode)
	bank_dimm = (address >> 27) & 3;
    else
	bank_dimm = (address >> 27) & 7;

    bank_phys = (address >> 20 ^ address >> 18 ^
		 address >> 10 ^ dimm0_sel >> 1) & 1;

    if (bit < 0)
	sprintf(buf, "MM%c%c%c and/or DIR%c%c",
		bank_dimm < 4 ? 'X' : 'Y',
		bank_phys ? 'H' : 'L',
		(int) (bank_dimm + '0'),
		bank_dimm < 4 ? 'X' : 'Y',
		(int) (bank_dimm + '0'));
    else if (bit < 16)
	sprintf(buf, "MM%c%c%c line %d",
		bank_dimm < 4 ? 'X' : 'Y',
		bank_phys ? 'H' : 'L',
		(int) (bank_dimm + '0'),
		bit);
    else
	sprintf(buf, "DIR%c%c line %d",
		bank_dimm < 4 ? 'X' : 'Y',
		(int) (bank_dimm + '0'),
		bit - 16);
}

/*
 * mdir_xlate_addr_sdir
 *
 *   Expects a physical memory address and a SIMM bit from 15:0, where bits
 *   15:5 correspond to data 10:0 and 4:0 correspond to the check bits.
 */

void mdir_xlate_addr_sdir(char *buf, __uint64_t address, int bit,
			  int dimm0_sel)
{
    __uint64_t		bank_dimm, bank_phys;

    if (opt_nmode)
	bank_dimm = (address >> 27) & 3;
    else
	bank_dimm = (address >> 27) & 7;

    bank_phys = (address >> 20 ^ address >> 18 ^
		 address >> 10 ^ dimm0_sel >> 1) & 1;

    if (opt_sn00) {
	if (bit < 0)
	    sprintf(buf, "SLOT%lld",
		    bank_dimm * 2 + bank_phys + 1);
	else
	    sprintf(buf, "SLOT%lld line %d",
		    bank_dimm * 2 + bank_phys + 1,
		    bit);
    } else {
	if (bit < 0)
	    sprintf(buf, "MM%c%c%c",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_phys ? 'H' : 'L',
		    (int) (bank_dimm + '0'));
	else
	    sprintf(buf, "MM%c%c%c line %d",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_phys ? 'H' : 'L',
		    (int) (bank_dimm + '0'),
		    bit);
    }
}

void do_physaddr(i64 a)
{
    char		buf[80];

    P1("Local offset:   0x%016llx\n", NODEOFF(a));

    P1("Bank:           %d\n", BANK(a));
    P1("Bank offset:    0x%016llx\n", BANKOFF(a));

    P1("Dir entry lo:   0x%016llx\n", BDDIR_ENTRY_LO(a));
    P1("Dir entry hi:   0x%016llx\n", BDDIR_ENTRY_HI(a));
    P1("Region 0 prot:  0x%016llx\n", BDPRT_ENTRY(a, 0));
    P1("ECC:            0x%016llx\n", BDECC_ENTRY(a));

    P1("Bit:            %d\n", opt_bit);

    mdir_xlate_addr_mem(buf, a, opt_bit);

    P1("Phys location:  %s\n", buf);

    if (opt_syn_given) {
	mdir_xlate_syndrome(buf, 0, opt_syn);
	P1("Syndrome bit:   %s\n", buf);
    }
}

void do_cac(i64 a)
{
    i64		pa	= PHYSADDR(a);

    P0("Cached space\n");

    if (NODE(a) == 0 && pa < opt_calias)
	P1("Calias, offset 0x%016llx\n", pa);

    do_physaddr(pa);
}

void do_uncac(i64 a)
{
    i64		pa	= PHYSADDR(a);

    P0("Uncached space\n");

    do_physaddr(pa);
}

void do_bdecc(i64 a)
{
    P0("Back door ECC\n");

    P1("Corresponding memory address: 0x%016llx\n",
	   BDECC_TO_MEM(a));
}

void do_bddir(i64 a)
{
    char		buf[80];

    if (BDADDR_IS_DIR(a)) {
	P0("Back door directory\n");
	P1("Corresponding memory address: 0x%016llx\n",
	       BDDIR_TO_MEM(a));
    } else {
	P0("Back door protection\n");
	P1("Corresponding memory address: 0x%016llx\n",
	       BDPRT_TO_MEM(a));
    }

    P1("Bit:            %d\n", opt_bit);

    if (opt_premium)
	mdir_xlate_addr_pdir(buf, a, opt_bit, 0);
    else
	mdir_xlate_addr_sdir(buf, a, opt_bit ,0);

    P1("Phys location:  %s\n", buf);

    if (opt_syn_given) {
	mdir_xlate_syndrome(buf, 2 - opt_premium, opt_syn);
	P1("Syndrome loc:   %s\n", buf);
    }
}

void do_hspec(i64 a)
{
    int		node;
    i64		na	= NODEOFF(a);

    P0("HSPEC space\n");

    if (NODE(a) == 0) {
	if (na < 256 * MB) {
	    P1("Ualias, offset 0x%016llx\n", na);
	    return;
	}

	if (na < 512 * MB) {
	    P1("Lboot, offset 0x%016llx\n", na);
	    return;
	}

	if (na < 768 * MB) {
	    P1("Reserved area "
		   "(node 0 HSPEC 512 MB to 768 MB), "
		   "offset 0x%016llx\n", na);
	    return;
	}
    }

    if (na < 768 * MB) {
	P1("Ualias reserved area, offset 0x%016llx\n", na);
	return;
    }

    if (na < 1024 * MB) {
	P1("Rboot, offset 0x%016llx\n", na);
	return;
    }

    if (! opt_nmode && na < 2048 * MB) {
	P1("Reserved area "
	       "(HSPEC 1024 MB to 2048 MB), "
	       "offset 0x%016llx\n", na);
	return;
    }

    if (opt_nmode) {
	if (na < 1536 * MB)
	    do_bdecc(a);		/* 1024 MB to 1535 MB */
	else
	    do_bddir(a);		/* 1536 MB to 2047 MB */
    } else {
	if (na < 3072 * MB)
	    do_bdecc(a);		/* 2048 MB to 3071 MB */
	else
	    do_bddir(a);		/* 3072 MB to 4095 MB */
    }
}

void do_hubreg(i64 a)
{
    i64		ha	= HUBOFF(a);

    P0("Hub space\n");

#if 0
    hubdiv = HUBDIV(a);

    switch (hubdiv) {
    case HUBDIV_PI:
	do_hubpi(a);
	break;
    case HUBDIV_MD:
	do_hubmd(a);
	break;
    case HUBDIV_IO:
	do_hubio(a);
	break;
    case HUBDIV_NI:
	do_hubni(a);
	break;
    }
#endif

    if (hwreg_lookup_addr(&hwreg_hub, ha) == 0) {
	P1("Undefined hub register address, offset 0x%016llx\n", ha);
	return;
    }
}

void do_io(i64 a)
{
    int		bwin, swin;
    i64		na	= NODEOFF(a);

    bwin = BWIN(a);
    swin = SWIN(a);

    if (na >= 256 * MB) {	/* Can happen in M-mode only */
	P0("*** Warning: upper 256M of big window 0 "
	       "is an alias for lower 256M\n");
	na -= 256 * MB;
    }

    P0("IO space\n");
    P1("Big window %d\n", bwin);
    P1("Small window (widget) %d\n", swin);

    if (REMOTE_BIT(a))
	P0("Remote bit set\n");

    if (bwin != 0 || swin != 1) {
	P1("Offset: 0x%016llx\n", na);
	return;
    }

    do_hubreg(a);
}

void do_mspec(i64 a)
{
    i64		na	= NODEOFF(na);

    P0("MSPEC space\n");
    P1("Offset: 0x%016llx\n", na);
}

void do_address(i64 a)
{
    int		node;

    P1("Address: 0x%016llx\n", a);
    P1("Node: %d\n", NODE(a));

    switch (REGION(a)) {
    case CAC_BASE:
	do_cac(a);
	break;
    case HSPEC_BASE:
	do_hspec(a);
	break;
    case IO_BASE:
	do_io(a);
	break;
    case MSPEC_BASE:
	do_mspec(a);
	break;
    case UNCAC_BASE:
	do_uncac(a);
	break;
    case 0:
	do_physaddr(a);
	break;
    default:
	P0("Bits 63:40 do not correspond to a known region.\n");
    }
}

/*
 * This is a tricky routine that replaces 'printf' in calls to certain
 * hwreg routines to allow for output on other descriptors than stdout.
 */
int PRINTF(const char *fmt, ...) {
#include <stdarg.h>
    va_list ap;
    va_start(ap, fmt);
    vfprintf(ofp, fmt, ap);
    va_end(ap);
    return(1);
}
