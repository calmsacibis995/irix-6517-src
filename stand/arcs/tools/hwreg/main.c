/*
 * SN0 Address Analyzer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mips_addrspace.h>

#include <sys/SN/SN0/hub.h>
#include <sys/SN/addrs.h>

#include <hwreg.h>

extern	hwreg_set_t		hwreg_hub;
extern	hwreg_set_t		hwreg_router;
extern	hwreg_set_t		hwreg_xbow;
extern	hwreg_set_t		hwreg_c0;
extern	hwreg_set_t		hwreg_dir;

typedef __uint64_t		i64;

#define MB			0x100000ULL

#define P(x)			fprintf(stderr, x)

char	opt_addr[80];
int	opt_nmode;
int	opt_hub;
int	opt_router;
int	opt_xbow;
int	opt_c0;
int	opt_dir;
int	opt_list;
int	opt_base		= 16;
int	opt_node;
int	opt_node_given;
int	opt_short;
i64	opt_calias		= 16 * MB;
int	opt_sn00;
int	opt_bit;
int	opt_premium;
i64	opt_syn;
int	opt_syn_given;

i64	winsize;		/* 256 MB if N-mode, 512 MB if M-mode */

#define REGION(a)		((a) & 0xffffff0000000000)
#define ADD_REGION(a, r)	(a |= (r))

#define NODE(a)			(int) (opt_nmode ?		\
				       (a) >> 31 & 0x1ff :	\
				       (a) >> 32 & 0xff)
#define ADD_NODE(a, n)		(opt_nmode ?			\
				 (a |= (i64) (n) << 31) :	\
				 (a |= (i64) (n) << 32))

#define NODEOFF(a)		(opt_nmode ?			\
				 ((a) & (1ULL << 31) - 1) :	\
				 ((a) & (1ULL << 32) - 1))

#define PHYSADDR(a)		((a) & (1ULL << 40) - 1)

#define BANK(a)			(int) ((a) >> 29 & (opt_nmode ? 7 : 3))
#define BANKOFF(a)		((a) & (1ULL << 29) - 1)

#define BWIN(a)			(int) (opt_nmode ?		\
				       (a) >> 28 & 7 :		\
				       (a) >> 29 & 7)
#define ADD_BWIN(a, w)		(opt_nmode ?			\
				 (a |= (i64) (w) << 28) :	\
				 (a |= (i64) (w) << 29))

#define SWIN(a)			(int) ((a) >> 24 & 0xf)
#define ADD_SWIN(a, w)		(a |= (i64) (w) << 24)

#define REMOTE_BIT(a)		(int) ((a) >> 23 & 1)
#define ADD_REMOTE_BIT(a)	(a |= 1ULL << 23)

#define HUBDIV_PI		0
#define HUBDIV_MD		1
#define HUBDIV_IO		2
#define HUBDIV_NI		3

#define HUBOFF(a)		((a) & 0x7fffff)
#define HUBDIV(a)		(HUBOFF(a) >> 21)
#define HUBDIVOFF(a)		((a) & 0x1fffff)

void usage(void)
{
    P("Usages:\n");
    P("   1. hwreg -list\n");
    P("      Displays all defined register names\n");
    P("   2. hwreg [flag ...] register [value]\n");
    P("      Decodes a register value (if no value, uses reset defaults)\n");
    P("   3. hwreg [flag ...] register field=value [...]\n");
    P("      Encodes a register value from a list of individual fields\n");
    P("Arguments:\n");
    P("   register      Specify register by address or name\n");
    P("   value         Specify register value to be decoded\n");
    P("   field=value   Assign specified value to specified field\n");
    P("                 Values may be in decimal, hex (0x), or octal (0).\n");
    P("                 Registers/fields may be abbreviated/lower case.\n");
    P("Flags:\n");
    P("   -nmode        Assume N-mode instead of M-mode\n");
    P("   -sn0          Assume Origin2000 (default)\n");
    P("   -sn00         Assume Origin200 (default)\n");
    P("   -bit n        Specify bit number (for physical location decode)\n");
    P("   -premium      Specify premium DIMMs (for phys. loc. decode)\n");
    P("   -calias n     Assume different calias size in bytes\n");
    P("                 Print value for specified bit-fields\n");
    P("   -bin          Print fields in binary instead of hex\n");
    P("   -oct          Print fields in octal instead of hex\n");
    P("   -dec          Print fields in decimal instead of hex\n");
    P("                 Use different base for printing info\n");
    P("   -node n       Generate addresses for specified node\n");
    P("   -short        Print decoded fields in short form\n");
    P("   -list         Lists all register names\n");
    P("   -listacc      Display help info on access mode definitions\n");
    P("   -hub          Assume hub reg (only look at address 23:00)\n");
    P("   -router       Assume router reg (only look at address 23:00)\n");
    P("   -xbow         Assume xbow reg (only look at address 23:00)\n");
    P("Examples:\n");
    P("   1. hwreg NI_PORT_ERROR 0x14aa0a0500\n");
    P("   2. hwreg ni_port_e in=1 bad_d=1 cr=10 ta=10 ret=10 cb=5\n");
    P("Notes:\n");
    P("   Physical location decode assumes bank 0 and 1 are not swapped.\n");
    exit(1);
}

void listacc(void)
{
    int		i;

    printf("Access  Description\n");

    for (i = 0; hwreg_accmodes[i].name; i++)
	printf("%-6s%s\n",
	       hwreg_accmodes[i].name, hwreg_accmodes[i].desc);

    exit(0);
}

void listreg(hwreg_set_t *regset, hwreg_t *r)
{
    printf("%-24s (%06llx)\n", regset->strtab + r->nameoff, r->address);
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

void listreg_matches_all(char *name)
{
    listreg_matches(&hwreg_hub, name);
    listreg_matches(&hwreg_router, name);
    listreg_matches(&hwreg_xbow, name);
    listreg_matches(&hwreg_c0, name);
    listreg_matches(&hwreg_dir, name);
}

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

    printf("Local offset:   0x%016llx\n", NODEOFF(a));

    printf("Bank:           %d\n", BANK(a));
    printf("Bank offset:    0x%016llx\n", BANKOFF(a));

    printf("Dir entry lo:   0x%016llx\n", BDDIR_ENTRY_LO(a));
    printf("Dir entry hi:   0x%016llx\n", BDDIR_ENTRY_HI(a));
    printf("Region 0 prot:  0x%016llx\n", BDPRT_ENTRY(a, 0));
    printf("ECC:            0x%016llx\n", BDECC_ENTRY(a));

    printf("Bit:            %d\n", opt_bit);

    mdir_xlate_addr_mem(buf, a, opt_bit);

    printf("Phys location:  %s\n", buf);

    if (opt_syn_given) {
	mdir_xlate_syndrome(buf, 0, opt_syn);
	printf("Syndrome bit:   %s\n", buf);
    }
}

void do_cac(i64 a)
{
    i64		pa	= PHYSADDR(a);

    printf("Cached space\n");

    if (NODE(a) == 0 && pa < opt_calias)
	printf("Calias, offset 0x%016llx\n", pa);

    do_physaddr(pa);
}

void do_uncac(i64 a)
{
    i64		pa	= PHYSADDR(a);

    printf("Uncached space\n");

    do_physaddr(pa);
}

void do_bdecc(i64 a)
{
    printf("Back door ECC\n");

    printf("Corresponding memory address: 0x%016llx\n",
	   BDECC_TO_MEM(a));
}

void do_bddir(i64 a)
{
    char		buf[80];

    if (BDADDR_IS_DIR(a)) {
	printf("Back door directory\n");
	printf("Corresponding memory address: 0x%016llx\n",
	       BDDIR_TO_MEM(a));
    } else {
	printf("Back door protection\n");
	printf("Corresponding memory address: 0x%016llx\n",
	       BDPRT_TO_MEM(a));
    }

    printf("Bit:            %d\n", opt_bit);

    if (opt_premium)
	mdir_xlate_addr_pdir(buf, a, opt_bit, 0);
    else
	mdir_xlate_addr_sdir(buf, a, opt_bit ,0);

    printf("Phys location:  %s\n", buf);

    if (opt_syn_given) {
	mdir_xlate_syndrome(buf, 2 - opt_premium, opt_syn);
	printf("Syndrome loc:   %s\n", buf);
    }
}

void do_hspec(i64 a)
{
    int		node;
    i64		na	= NODEOFF(a);

    printf("HSPEC space\n");

    if (NODE(a) == 0) {
	if (na < 256 * MB) {
	    printf("Ualias, offset 0x%016llx\n", na);
	    return;
	}

	if (na < 512 * MB) {
	    printf("Lboot, offset 0x%016llx\n", na);
	    return;
	}

	if (na < 768 * MB) {
	    printf("Reserved area "
		   "(node 0 HSPEC 512 MB to 768 MB), "
		   "offset 0x%016llx\n", na);
	    return;
	}
    }

    if (na < 768 * MB) {
	printf("Ualias reserved area, offset 0x%016llx\n", na);
	return;
    }

    if (na < 1024 * MB) {
	printf("Rboot, offset 0x%016llx\n", na);
	return;
    }

    if (! opt_nmode && na < 2048 * MB) {
	printf("Reserved area "
	       "(HSPEC 1024 MB to 2048 MB), "
	       "offset 0x%016llx\n", na);
	return;
    }

    if (opt_nmode) {
	if (na < 1536 * MB)
	    do_bdecc(a);		/* 1024 MB to 1535 MB *
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

    printf("Hub space\n");

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
	printf("Undefined hub register address, offset 0x%016llx\n", ha);
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
	printf("*** Warning: upper 256M of big window 0 "
	       "is an alias for lower 256M\n");
	na -= 256 * MB;
    }

    printf("IO space\n");
    printf("Big window %d\n", bwin);
    printf("Small window (widget) %d\n", swin);

    if (REMOTE_BIT(a))
	printf("Remote bit set\n");

    if (bwin != 0 || swin != 1) {
	printf("Offset: 0x%016llx\n", na);
	return;
    }

    do_hubreg(a);
}

void do_mspec(i64 a)
{
    i64		na	= NODEOFF(na);

    printf("MSPEC space\n");
    printf("Offset: 0x%016llx\n", na);
}

void do_address(i64 a)
{
    int		node;

    printf("Address: 0x%016llx\n", a);
    printf("Node: %d\n", NODE(a));

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
	printf("Bits 63:40 do not correspond to a known region.\n");
    }
}

void main(int argc, char **argv)
{
    i64			addr;
    hwreg_set_t	       *regset;
    hwreg_t	       *r;
    i64			value;

    argv++, argc--;

    if (argc == 0)
	usage();

    while (argc) {
	if (strcmp(argv[0], "-bin") == 0)
	    opt_base = 2;
	else if (strcmp(argv[0], "-oct") == 0)
	    opt_base = 8;
	else if (strcmp(argv[0], "-dec") == 0)
	    opt_base = 10;
	else if (strcmp(argv[0], "-hex") == 0)
	    opt_base = 16;
	else if (strcmp(argv[0], "-nmode") == 0)
	    opt_nmode = 1;
	else if (strcmp(argv[0], "-hub") == 0)
	    opt_hub = 1;
	else if (strcmp(argv[0], "-router") == 0)
	    opt_router = 1;
	else if (strcmp(argv[0], "-xbow") == 0)
	    opt_xbow = 1;
	else if (strcmp(argv[0], "-sn0") == 0)
	    opt_sn00 = 0;
	else if (strcmp(argv[0], "-sn00") == 0)
	    opt_sn00 = 1;
	else if (strcmp(argv[0], "-premium") == 0)
	    opt_premium = 1;
	else if (strcmp(argv[0], "-syn") == 0) {
	    argc--, argv++;
	    if (argc == 0)
		usage();
	    opt_syn = strtoull(argv[0], 0, 0);
	    opt_syn_given = 1;
        } else if (strcmp(argv[0], "-bit") == 0) {
	    argc--, argv++;
	    if (argc == 0)
		usage();
	    opt_bit = strtoull(argv[0], 0, 0);
	} else if (strcmp(argv[0], "-calias") == 0) {
	    argc--, argv++;
	    if (argc == 0)
		usage();
	    opt_calias = strtoull(argv[0], 0, 0);
	} else if (strcmp(argv[0], "-node") == 0) {
	    argc--, argv++;
	    if (argc == 0)
		usage();
	    opt_node = strtoul(argv[0], 0, 0);
	    opt_node_given = 1;
	} else if (strcmp(argv[0], "-list") == 0)
	    opt_list = 1;
	else if (strcmp(argv[0], "-short") == 0)
	    opt_short = 1;
	else if (strcmp(argv[0], "-listacc") == 0)
	    listacc();
	else if (strcmp(argv[0], "-help") == 0 ||
		 strcmp(argv[0], "-h") == 0)
	    usage();
	else if (argv[0][0] == '-') {
	    fprintf(stderr, "Unknown flag: %s\n", argv[0]);
	    exit(1);
	} else
	    break;

	argc--, argv++;
    }

    winsize = opt_nmode ? 256 * MB : 512 * MB;

    if (argc == 0) {
	if (opt_list) {
	    listreg_matches_all("");
	    exit(0);
	}

	fprintf(stderr, "Address required\n");
	exit(1);
    }

    strcpy(opt_addr, argv[0]);
    argc--, argv++;

    /*
     * Process address argument -- either a number or register name.
     */

    r = 0;

    if (isdigit(opt_addr[0])) {
	addr = strtoull(opt_addr, 0, 0);

	if (REGION(addr) == IO_BASE && BWIN(addr) == 0 && SWIN(addr) == 1)
	    opt_hub = 1;
    } else {
	if ((r = hwreg_lookup_name(&hwreg_hub, opt_addr, 1)) != 0) {
	    regset = &hwreg_hub;
	    opt_hub = 1;
	} else if ((r = hwreg_lookup_name(&hwreg_router, opt_addr, 1)) != 0) {
	    regset = &hwreg_router;
	    opt_router = 1;
	} else if ((r = hwreg_lookup_name(&hwreg_xbow, opt_addr, 1)) != 0) {
	    regset = &hwreg_xbow;
	    opt_xbow = 1;
	} else if ((r = hwreg_lookup_name(&hwreg_c0, opt_addr, 1)) != 0) {
	    regset = &hwreg_c0;
	    opt_c0 = 1;
	} else if ((r = hwreg_lookup_name(&hwreg_dir, opt_addr, 1)) != 0) {
	    regset = &hwreg_dir;
	    opt_dir = 1;
	} else {
	    printf("Unknown or ambiguous register name: %s\n", opt_addr);
	    printf("Use -list to see all registers.  Possible matches:\n");

	    listreg_matches_all(opt_addr);

	    exit(1);
	}

	addr = r->address;
    }

    if (opt_hub) {
	addr = HUBOFF(addr);
	ADD_REGION(addr, IO_BASE);
	if (opt_node_given)
	    ADD_REMOTE_BIT(addr);
	ADD_BWIN(addr, 0);
	ADD_SWIN(addr, 1);
    }

    if (opt_node_given)
	ADD_NODE(addr, opt_node);

    if (opt_hub) {
        do_address(addr);

 	if (r == 0) {
	    regset = &hwreg_hub;
	    r = hwreg_lookup_addr(regset, HUBOFF(addr));
	}
    }

    if (opt_router && r == 0) {
	regset = &hwreg_router;
	r = hwreg_lookup_addr(regset, HUBOFF(addr));
    }

    if (opt_xbow && r == 0) {
	regset = &hwreg_xbow;
	r = hwreg_lookup_addr(regset, HUBOFF(addr));
    }

    if (opt_hub || opt_router || opt_xbow || opt_c0 || opt_dir) {
	if (r)
	    listreg(regset, r);

	if (argc == 0) {		/* No value or fields? */
	    if (r) {
		printf("Using system reset default value: 0x%016llx\n",
		       hwreg_reset_default(regset, r));

		hwreg_decode(regset, r, opt_short, 1,
			     opt_base, 0L,
			     printf);

		if (r->noteoff)
		    printf("Note: %s\n", regset->strtab + r->noteoff);
	    } else
		printf("Register matching address not found.\n");

	    exit(0);
	}

	if (strchr(argv[0], '=') == 0) {
	    /*
	     * Value to be decoded was specified
	     */

	    if (argc != 1) {
		printf("Extra argument(s)\n");
		exit(1);
	    }

	    if (r == 0) {
		printf("Register unknown, unable to determine bit fields\n");
		exit(1);
	    }

	    value = strtoull(argv[0], 0, 0);

	    hwreg_decode(regset, r, opt_short, 0,
			 opt_base, value,
			 printf);

	    if (r->noteoff)
		printf("Note: %s\n", regset->strtab + r->noteoff);

	    exit(0);
	}

	/*
	 * List of arguments of the form "field=value"
	 */

	if (r == 0) {
	    printf("Register unknown, unable to encode bit-fields\n");
	    exit(1);
	}

	value = 0;

	while (argc--)
	    value |= hwreg_encode(regset, r, *argv++, printf);

	hwreg_decode(regset, r, opt_short, 0,
		     opt_base, value,
		     printf);

	if (r->noteoff)
	    printf("Note: %s\n", regset->strtab + r->noteoff);

	printf("Encoded value = %016llx\n", value);
    } else
        do_address(addr);

    exit(0);
}
