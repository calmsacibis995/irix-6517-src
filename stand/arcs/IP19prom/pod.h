#ifndef _IP19PROM_POD_H_ 
#define _IP19PROM_POD_H_

/* This is the status register value POD mode will load.  */
#define POD_SR	(SR_IMASK & ~(SR_IBIT8|SR_IBIT1|SR_IBIT2)) | SR_BEV|SR_DE|SR_KX|SR_FR|SR_CU1

/* Take error interrupts right away */
#define POD_ILE	8

#define POD_SCACHEADDR	0x80000000

/* Defines for the parser. */
#define LINESIZE   		128
#define SHORTBUFF_LENGTH	32
#define MAX_NEST		6

/* These came from saioctl.h. */
#define EOF	(-1)
#define NULL	0

/* These defines are also for the parser */
#define CTRL(_x)	((_x) & 0x1f)
#define DEL             0x7f
#define INTR            CTRL('C')
#define BELL            0x7
#define isprint(c)      ((c >= 0x20) && (c <= 0x7e) || c == 0x09 || c == 0x08)
#define isdigit(c)      ((c >= '0') && (c <= '9'))

/* Operation and size constants for the mem() function */
#define READ    0
#define WRITE   1
#define BYTE    0
#define HALF    1
#define WORD    2
#define DOUBLE  3

/* POD mode command tokens: */
#define	WRITE_BYTE	1
#define	WRITE_HALF	2
#define	WRITE_WORD	3
#define	WRITE_DOUBLE	4
#define	DISP_BYTE	5
#define	DISP_HALF	6
#define	DISP_WORD	7
#define	DISP_DOUBLE	8
#define	SERIAL_LOAD	9
#define	LOAD_IOPROM	10
#define	RESET_SYSTEM	11
#define	DISP_REG	12
#define	WRITE_REG	13
#define	SCOPE_LOOP	14
#define	FINITE_LOOP	15
#define	DISP_INFO	16
#define	TEST_MEM	17
#define	JUMP_ADDR	18
#define	DISP_HELP	19
#define LEFT_PAREN	20
#define DISP_CONF	21
#define NO_COMMAND	22
#define DISP_MC3	23
#define TEST_SCACHE	24
#define BIG_ENDIAN	25
#define NIBLET		26
#define POD_RESUME	27
#define FLUSHI		28
#define FLUSHD		29
#define FLUSHT		30
#define DECODE_ECC	31
#define TLB_DUMP	32
#define WRITE_CONF	33
#define SEND_INT	34
#define FLUSHS		35
#define	SERIAL_RUN	36
#define DISP_IO4	37
#define	CLEAR_STATE	38
#define	JUMP_ADDR1	39
#define	JUMP_ADDR2	40
#define DECODE_ADDR	41
#define SET_DE		42
#define WALK_MEM	43
#define ITAG_DUMP	45
#define DTAG_DUMP	46
#define STAG_DUMP	47
#define SLAVE_MODE	48
#define WRITE_EXTENDED	51
#define DISP_EXTENDED	52
#define DOWNLOAD_IO4	53
#define WHY		54
#define GOTO_MEM	55
#define DISABLE_UNIT	56
#define ENABLE_UNIT	57
#define RECONF_MEM	58
#define GOTO_CACHE	59
#define DO_BIST		60
#define DISP_EVCONFIG	61
#define POD_SELECT	62
#define FDISABLE_UNIT	63
#define FENABLE_UNIT	64
#define PRM_CRD		65
#define	ZAP_INVENTORY	66
#define DISP_MPCONF	67
#define FIX_CHECKSUM	68
#define DISP_CHECKSUM	69

/* The following definitions control where we store GP registers in FP
 *	registers.
 */

#define AT_FP	$f6
#define A0_FP	$f7
#define A1_FP	$f8
#define A2_FP	$f9
#define V0_FP	$f10
#define V1_FP	$f11
#define T0_FP	$f12
#define T1_FP	$f13
#define T2_FP	$f14
#define T3_FP	$f15
#define T4_FP	$f16
#define T5_FP	$f17
#define T6_FP	$f18
#define T7_FP	$f19
#define S0_FP	$f20
#define S1_FP	$f21
#define S2_FP	$f22
#define S3_FP	$f23
#define S4_FP	$f24
#define S5_FP	$f25
#define S6_FP	$f26
#define T8_FP	$f27
#define T9_FP	$f28
#define SP_FP	$f29
#define RA_FP	$f30
/* A total of 23 GP registers are used by POD_Handler before calling pod_loop */

/* This is the structure we use to access the stored GPRs aftyer we retrieve
 * them from the FP registers.
 */
#if LANGUAGE_C
struct reg_struct {
	unsigned int 	gpr[32*2];	/* Each GPR is two words long */
	int		sr;
	int		cause;
	unsigned int	epc;
	unsigned int	filler1;
	unsigned int	badva;
	unsigned int	filler2;
};

struct flag_struct {
	int de;			/* state of DE bit. 1 = disable
				 * ECC exceptions.
				 */
	int mem;		/* Are we running from memory? */
	int diagval;		/* Diagval that brought us here. */
        char *diag_string;	/* Why are we in POD mode? */
	int scroll_msg;		/* Scroll long diagnostic string? */
	char slot;		/* Where are we on the backplane? */
	char slice;		/* Where are we on the board? */
	char selected;		/* Who's selected now? */
	char silent;		/* Run in quiet mode. */
};


#endif

/* These are the assembly language offsets into the reg_struct structure */
#define R0_OFF		0x0
#define R1_OFF		0x8
#define R2_OFF		0x10
#define R3_OFF		0x18
#define R4_OFF		0x20
#define R5_OFF		0x28
#define R6_OFF		0x30
#define R7_OFF		0x38
#define R8_OFF		0x40
#define R9_OFF		0x48
#define R10_OFF		0x50
#define R11_OFF		0x58
#define R12_OFF		0x60
#define R13_OFF		0x68
#define R14_OFF		0x70
#define R15_OFF		0x78
#define R16_OFF		0x80
#define R17_OFF		0x88
#define R18_OFF		0x90
#define R19_OFF		0x98
#define R20_OFF		0xa0
#define R21_OFF		0xa8
#define R22_OFF		0xb0
#define R23_OFF		0xb8
#define R24_OFF		0xc0
#define R25_OFF		0xc8
#define R26_OFF		0xd0
#define R27_OFF		0xd8
#define R28_OFF		0xe0
#define R29_OFF		0xe8
#define R30_OFF		0xf0
#define R31_OFF		0xf8
#define SR_OFF		0x100
#define CAUSE_OFF	0x104
#define EPC_OFF		0x108
#define BADVA_OFF	0x110

/*
 * Global register allocation for nested assembly calls
 */

#define Ra_save0        s0      /* level 0 ra save registers */
#define Ra_save1        s1      /* level 1 ra save registers */
#define Ra_save2        s2      /* level 2 ra save registers */


/* Defines used to interpret C0_CONFIG register values */
#define MIN_CACH_POW2   	12
#define MIN_BLKSIZE_SHFT        4


#ifdef LANGUAGE_C
/* tag_regs structs hold the contents of the TagHi and TagLo registers,
 * which cache tag instructions use to read and write the primary and
 * secondary caches.
 */
typedef struct tag_regs {
        unsigned int tag_lo;
        unsigned int tag_hi;
} tag_regs_t;

/* mem_regs structs hold the base, interleave factor, interleave position,
 *	SIMM type, and bank enable for all memory banks. 
 *	non-memory boards always have a zero in bank enable.
 */
typedef struct mem_regs {
	unsigned short enable;
	unsigned short simm_type;
	unsigned short i_factor;
 	unsigned short i_position;
	unsigned int base;
	unsigned int end;
} mem_regs_t;
	
#endif /* LANGUAGE_C */

#endif /* #ifndef _IP19PROM_POD_H_ */
