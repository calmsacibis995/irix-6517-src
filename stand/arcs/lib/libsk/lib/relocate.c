/* this is from     /jake/d1/cmplrs.src/v4.00/ld3264/relocate.c */

/* 
 * This file does relocation of ELF objects 
 */

#include <sys/mips_addrspace.h>
#include <arcs/errno.h>
#include <setjmp.h>
#include <elf.h>
#include "ld_defs.h"
#include "obj_file.h"
#include "loadelf.h"
#include <libsc.h>

/*#define DEBUG 1*/
#define sexchange 0

#ifdef DEBUG
static const char thisfile[] = __FILE__;
#endif

static int fixup(ADDR, ELF_WORD, int);

#define RLC_NOERROR 0
#define RLC_BADTYPE 1
#define RLC_HALFOVL 2
#define RLC_MISSLO  3
#define RLC_JMPOVL  4
#define RLC_GPOVL   5
#define RLC_GOTOVL  6

typedef struct reloc_attr {
    /* number of bytes the relocation value p4-18 of ABI-mips */
    char val_size;
    char need_dynrel;		    /* need runtime relocation? */
    char skip_if_r_ext;		    /* if relocatable, no fixup if extern  */
    char skip_if_r_lcl;		    /* if relocatable, no fixup if local */
    char val_type;		    /* specifics of how to get value */
#define      VAL_NORMAL  0   
#define      VAL_LOGPDP  1    /* when sym is _gp_disp and LO16 */
#define      VAL_GOTOFS  2    /* if local, local page */
#define	     VAL_GOTPGE  3    /* use GOT page for both extern and local */
#define      VAL_MRGOFS  4    /* consult merged scn */
#define      VAL_GOTDSP  5    /* use GOT entry for both extern and local */
#define	     VAL_SCNDSP  6    /* use relative offset from section */
    char addend_type;
#define      ADEND_NONE  0
#define      ADEND_HALF  1
#define      ADEND_WORD  2
#define      ADEND_JAL   3
#define      ADEND_HI    4
#define      ADEND_GOTHI 5
#define      ADEND_LO    6
#define      ADEND_GPREL 7
#define      ADEND_XWORD 8
#define      ADEND_BAD   9
#define	     ADEND_GPWORD 10
#define	     ADEND_ADDR 11    
} RELC_ATTR;

static const RELC_ATTR relc_attr[_R_MIPS_COUNT_ + 1] = {
    0,  0, 1, 1, VAL_NORMAL, ADEND_NONE,	/* R_MIPS_NONE */
    2,  0, 0, 0, VAL_NORMAL, ADEND_HALF,	/* R_MIPS_16 */
    4,  1, 0, 0, VAL_NORMAL, ADEND_WORD,	/* R_MIPS_32 */
    4,  0, 0, 0, VAL_NORMAL, ADEND_NONE,	/* R_MIPS_REL32 */
    4,  0, 0, 0, VAL_NORMAL, ADEND_JAL,		/* R_MIPS_26 */
    4,  0, 0, 0, VAL_NORMAL, ADEND_HI,		/* R_MIPS_HI16 */
    4,  0, 0, 0, VAL_LOGPDP, ADEND_LO,		/* R_MIPS_LO16 */
    4,  0, 1, 0, VAL_NORMAL, ADEND_GPREL,	/* R_MIPS_GPREL16 */
    4,  0, 1, 0, VAL_MRGOFS, ADEND_GPREL,	/* R_MIPS_LITERAL */
    4,  0, 1, 0, VAL_GOTOFS, ADEND_GOTHI,	/* R_MIPS_GOT16 */
    4,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/* R_MIPS_PC16 */
    4,  0, 1, 1, VAL_GOTOFS, ADEND_NONE,	/* R_MIPS_CALL16 */
    4,  0, 1, 0, VAL_NORMAL, ADEND_GPWORD,	/* R_MIPS_GPREL32 */
    0,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/*  */
    0,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/*  */
    0,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/*  */
    4,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/* R_MIPS_SHIFT5 */
    4,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/* R_MIPS_SHIFT6 */
    8,  1, 0, 0, VAL_NORMAL, ADEND_XWORD,	/* R_MIPS_64 */
    4,  0, 1, 0, VAL_GOTDSP, ADEND_ADDR,	/* R_MIPS_GOT_DISP */
    4,  0, 0, 0, VAL_GOTPGE, ADEND_ADDR,	/* R_MIPS_GOT_PAGE */
    4,  0, 0, 0, VAL_NORMAL, ADEND_ADDR,	/* R_MIPS_GOT_OFST */
    4,  0, 1, 1, VAL_GOTDSP, ADEND_ADDR,	/* R_MIPS_GOT_HI16 */
    4,  0, 1, 1, VAL_GOTDSP, ADEND_LO,		/* R_MIPS_GOT_LO16 */
    8,  0, 0, 0, VAL_NORMAL, ADEND_XWORD,	/* R_MIPS_SUB */
    4,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/* R_MIPS_INSERT_A */
    4,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/* R_MIPS_INSERT_B */
    4,  0, 0, 0, VAL_NORMAL, ADEND_BAD,		/* R_MIPS_DELETE */
    4,  0, 0, 0, VAL_NORMAL, ADEND_ADDR,	/* R_MIPS_HIGHER */
    4,  0, 0, 0, VAL_NORMAL, ADEND_ADDR,	/* R_MIPS_HIGHEST */
    4,  0, 1, 1, VAL_GOTDSP, ADEND_ADDR,	/* R_MIPS_CALL_HI16 */
    4,  0, 1, 1, VAL_GOTDSP, ADEND_LO,		/* R_MIPS_CALL_LO16 */
    4,	0, 1, 0, VAL_SCNDSP, ADEND_WORD, 	/* R_MIPS_SCN_DISP */
    0,  0, 0, 0, VAL_NORMAL, ADEND_BAD,   /* should NOT have gotten here, check sys/elf.h */
};


#pragma pack(1)
struct unalign_word {
    uint32 w;
};
struct unalign_half {
    uint16 h;
};
struct unalign_longlong {
    uint64 x;
};
#pragma pack(0)

static uint32 word;
static uint16 half;
static uint64 xword;

static OBJECT_FILE *stat_curobj;

#define relocatable 0
#define sharable 0

#define SWAPHALF(_i)  \
            if (sexchange) { \
                 (_i) = ((_i >> 8) & 0xff) | ((_i << 8) & 0xff00); }

#define SWAP32WORD(_i)  \
            if (sexchange) { \
	        (_i) = (((_i<<24)&0xff000000) | ((_i << 8)&0xff0000) | \
                        ((_i >> 8) & 0xff00) | ((_i >> 24) & 0xff)); }

#define SWAP64WORD(_ll)  \
            if (sexchange) { \
		(_ll) = (((_ll<<56)&0xff00000000000000LL) |\
			 ((_ll<<48)&0x00ff000000000000LL) |\
			 ((_ll<<40)&0x0000ff0000000000LL) |\
			 ((_ll<<32)&0x000000ff00000000LL) |\
			 ((_ll>>32)&0x00000000ff000000LL) |\
			 ((_ll>>40)&0x0000000000ff0000LL) |\
			 ((_ll>>48)&0x000000000000ff00LL) |\
			 ((_ll>>56)&0x00000000000000ffLL));}
#ifdef _64BIT_OBJECTS
#define SWAPADDR(_a) SWAP64WORD(_a)
#else
#define SWAPADDR(_a) SWAP32WORD(_a)
#endif

#ifdef _64BIT_OBJECTS
struct ks *ks;
#endif

#define scn_vaddr_disp(a) (ks[a].ks_raw)

#define OVERFLOW_16(_x) \
    (((int32)(_x) < (int32)0xffff8000) || ((int32)(_x) > (int32)0x00007fff))


static ELF_INT addend;

static ELF_SYM *stat_symtab;

static int islcl;		    /* if symbol local ? */
static ELF_ADDR fixup_addr;

static jmp_buf jbuf;

static
void
abort(int err)
{
	longjmp(jbuf, err);
}

static ELF_REL *
find_lo16 (int hiidx, int numrlc, ELF_REL *prlc)
{
    register int i, hisymidx;

    hisymidx = REL_sym(prlc[hiidx]);

    for (i = hiidx+1; i < numrlc; i++) {
	if ( REL_type(prlc[i]) == R_MIPS_LO16 &&
	    hisymidx == REL_sym(prlc[i]))
	    return &prlc[i];
    }
    return (ELF_REL *)0;
} /* find_lo16 */

static int
jmp_out_of_range(ADDR target, ADDR pc)
{
    /* jmp target must have high bits the same */
    /* i.e. same 256 megabyte segment */
#ifdef _64BIT_OBJECTS
#define JAL_MASK (0xfffffffff0000000LL)
#else
#define JAL_MASK (0xf0000000)
#endif /* _64BIT_OBJECTS */
    
    if ((target & JAL_MASK) != ((pc+4) & JAL_MASK)) {
	return TRUE;
    }

    return FALSE;
}


/*
 * SIDE EFFECT: sets the set of statics: "half, word, lword"
 *     their values is used to compute the correct value
 *     for relocation. They should stay the same while working
 *     on the same relocation entry.
 */
static void
extract_relc_val(int size, pointer raw, ADDR offset)
{
char *src = (char *)raw + offset, *dst;
int i;
    switch (size) {
    case 4:
	    dst = (char *)&word;
	    break;
    case 8:
	    dst = (char *)&xword;
	    break;
    case 0:
	    break;
    case 2:
	    dst = (char *)&half;
	    break;
    case -1:
    default:
	    printf ("val_rel_size table inconsistent with number of relocation entries"); 
	    break;
    }

    for(i = 0; i < size; i++) {
	*dst++ = *src++;
    }

    switch (size) {
    case 4:
	    SWAP32WORD(word);
	    break;
    case 8:
	    SWAP64WORD(xword);
	    break;
    case 2:
	    SWAPHALF(half);
	    break;
    default:
	    break;
    }
}


static void
put_relc_val(int size, pointer raw, ADDR offset)
{
char *src, *dst = (char *)raw + offset;
int i;
    switch (size) {
    case 4:
	    SWAP32WORD(word);
#if DEBUG
	    printf("4 0x%x 0x%x", word,  dst );
#endif
	    src = (char *)&word;
	    break;
    case 8:
	    SWAP64WORD(xword);
#if DEBUG
	    printf("8 0x%llx 0x%x", xword,  ((char *)raw+offset) );
#endif
	    src = (char *)&xword;
	    break;
    case 0:
	    break;
    case 2:
	    SWAPHALF(half);
#if DEBUG
	    printf("2 0x%x 0x%x", half,  ((char *)raw+offset) );
#endif
	    src = (char *)&half;
	    break;
    case -1:
    default:
	    /*val_rel_size table inconsistent w/ # of relocation entries*/
	    abort(EINVAL);
	    break;
    }
    for(i = 0; i < size; i++) {
	*dst++ = *src++;
    }
}


/* 
 * SIDE EFFECT: 
 * QUICKSTART flag is set here.
 */
static ADDR
get_target_value(int idx, int val_type)
{
    ADDR value;
    register ELF_SYM *psym = stat_symtab + idx;

	value = scn_vaddr_disp (psym->st_shndx);
	if (ELF_ST_TYPE(psym->st_info) == STT_OBJECT ||
	    ELF_ST_TYPE(psym->st_info) == STT_FUNC)
	    value += psym->st_value;
#define get_shflags_scn(a, stat_cur_obj) stat_curobj->section_table[a].sh_flags
	if (get_shflags_scn (psym->st_shndx, stat_curobj) & SHF_MIPS_MERGE)
	    val_type = VAL_MRGOFS;

    switch (val_type) {
    case VAL_MRGOFS:
    case VAL_GOTPGE:
    case VAL_GOTOFS:
    case VAL_GOTDSP:
	abort(EINVAL);
	break;
    case VAL_SCNDSP:
	value -= scn_vaddr_disp (psym->st_shndx);
	break;
    }

    return value;
}


/*
 * SIDE EFFECT: sets the static: addend
 *     It value us used to calculate the 
 *     correct value for relocation. They should remain the same
 *     while working on the same relocation entry.
 */
static void
rela_get_addend (int type, ELF_RELA *prlc)
{
    Elf64_Sxword r_addend = REL_addend (*prlc);
    
    switch (type) {
	
    case ADEND_NONE:
	break;
	
    case ADEND_HI: 
    case ADEND_WORD:
    case ADEND_JAL:
    case ADEND_LO:
    case ADEND_HALF:
    case ADEND_XWORD:
    case ADEND_ADDR:
	addend += r_addend;
	break;
	
    case ADEND_GOTHI:
    case ADEND_GPWORD:
    case ADEND_GPREL:
    case ADEND_BAD:
	abort(EINVAL);
	break;
	
    }
}


static void
rel_get_addend (int type, int idx, int nrel, pointer raw, ELF_REL *reloc)
{
    short addend_lo;
    ELF_REL *rlc_lo;
    
    switch(type) {
    case ADEND_NONE:
	break;
	
    case ADEND_HALF:
	addend += half;
	break;
	
    case ADEND_WORD:
	addend += word;
	break;
	
    case ADEND_JAL:
	addend += (word & 0x3ffffff) << 2;
	break;
    case ADEND_GOTHI:
	if (!islcl) {
	    break;
	}
	/* else  FALL thru */
    case ADEND_HI: 
	{
	    struct unalign_word lo_word;
	    
	    /* calculate addend from lo portion */
	    rlc_lo = find_lo16(idx, nrel, reloc);
	    if (rlc_lo == (ELF_REL *)0) 
		abort(EINVAL);
	    
	    lo_word = *(struct unalign_word *)
		((char *)raw + REL_offset (*rlc_lo));
	    SWAP32WORD(lo_word.w);
	    
	    addend_lo = (short)lo_word.w;
	    
	    addend += (word << 16) + addend_lo;
	    
	    break;
	}
	
    case ADEND_LO:
	addend += (int16) word;
	break;
	
    case ADEND_GPREL:
    case ADEND_GPWORD:
	abort(EINVAL);
	break;
	
    case ADEND_XWORD:
	addend += xword;
	break;
	
    case ADEND_ADDR:
	addend += (short) (word & 0xffff);
	break;

    case ADEND_BAD:
	abort(EINVAL);
	break;
    }
}


/*ARGSUSED4*/
int
do_elf_reloc32(ELF_SHDR *scnhdr, int rel_indx, ELF_RELA *preloca, string strtab, ELF_SYM *symtab, unsigned long long scn_addr)
{
    /*
    register pOBJECT_FILE pobj = psec->obj;
    register pointer raw = psec->out_raw;
    */

#ifdef IP28	/* see d$ speculation comment in loadelf.c */
    pointer raw = (void *)PHYS_TO_K1(scn_addr);
#else
    pointer raw = (void *)PHYS_TO_K0(scn_addr);
#endif
    unsigned int symtab_size;
    ELF_SHDR *sp = scnhdr + rel_indx;
    int i, j, idx, type;
    ELF_REL  *prlc, *reloc;
    ELF_RELA *prlca, *reloca;
    const RELC_ATTR *rlc_attr;
    ADDR value;
    register ELF_INT last_addend = 0;
#if 0
    register ELF_INT local_addend;
#endif
    ELF_SIZE section_size = scnhdr[sp->sh_info].sh_size;
    ELF_SYM *this_sym;
    int is_rela;

    OBJECT_FILE obj;

    int nrel = (int)(scnhdr[rel_indx].sh_size / scnhdr[rel_indx].sh_entsize);
    int err;


    stat_curobj = &obj;

    if((err = setjmp(jbuf)) != 0) {
	return err;
    }


    symtab_size = (int)(scnhdr[sp->sh_link].sh_size / scnhdr[sp->sh_link].sh_entsize);
#if DEBUG
	printf("nrel is 0x%x\n", nrel);
	printf("symtab_size is 0x%x\n", symtab_size);
	printf("symtabis 0x%x\n", symtab);
	printf("strtabis 0x%x\n", strtab);
	printf("reltabis 0x%x\n", preloca);
	printf("scn is 0x%llx\n", scn_addr);
	printf("in do_elf_relocate");
#if _64BIT_OBJECTS
	printf("64");
#endif
#endif
    
    stat_symtab = symtab;
    addend = 0;
    stat_curobj->section_table = scnhdr;
    
    reloca = preloca;
    reloc = (ELF_REL *)reloca;

    is_rela = (sp->sh_type == SHT_RELA);

    for (i = 0; i < nrel; i++) {
	register int is_composite;
#if DEBUG
	printf("\n");
#endif

	if (is_rela) {
	    prlca = &reloca[i];
	    prlc = (ELF_REL *)prlca;
	    is_composite = (i + 1 < nrel &&
			    REL_offset(reloca[i+1]) == REL_offset (*prlca));
#if 0
	    /* same logic for getting addend as in pass1.c for relocations
	     * like R_MIPS_GOT_DISP or R_MIPS_GOT_LO16
	     */
	    local_addend = REL_addend (*prlca);
#endif

	}
	else {
	    prlc = &reloc[i];
	    prlca = (ELF_RELA *)prlc;
	    is_composite = (i + 1 < nrel &&
			    REL_offset (reloc[i+1]) == REL_offset (*prlc));
#if 0
	    local_addend = 0;
#endif
	}


#if DEBUG
#ifdef _64BIT_OBJECTS
	printf("type %d ", prlc->r_type);
	if(prlc->r_type2) {
		printf("type %d ", prlc->r_type2);
		if(prlc->r_type3) {
			printf("type %d ", prlc->r_type3);
		}
	}
#endif
#endif
        idx = REL_sym(*prlc);
	if (idx < 0 || idx >= symtab_size) {
	    return ENOEXEC;
	}

	this_sym = stat_symtab + idx;
#if DEBUG
	printf(" %s ", strtab + this_sym->st_name);
#endif
	if (IS_COMMON(this_sym->st_shndx))
	    abort(EIO);

	addend = last_addend;

	islcl = ELF_ST_BIND(this_sym->st_info) == STB_LOCAL;

	type = REL_type(*prlc);

	if(!(type <= _R_MIPS_COUNT_))
		abort(EINVAL);
	
	rlc_attr = &relc_attr[type];

	/* get raw data to be fixup */
	extract_relc_val(rlc_attr->val_size, raw, REL_offset (*prlc));

	/* find addend, i.e.  constant portion of  "sym+const" */
	/* in relocatable object */
	if (is_rela)
	    rela_get_addend(rlc_attr->addend_type, prlca);
	else
	    rel_get_addend(rlc_attr->addend_type, i, nrel, raw, reloc);

	last_addend = 0;

	/* do we really need to relocate? */
	if (relocatable && ((rlc_attr->skip_if_r_ext && !islcl) ||
		      (rlc_attr->skip_if_r_lcl && islcl)))
	    continue;

	/* get final value of sym, as in "sym+const" */
	    value = get_target_value(idx, rlc_attr->val_type);
#if DEBUG
	printf("v 0x%llx ", value);
#endif

	/* error checking */
	if (section_size <= REL_offset(*prlc)) {
#if DEBUG
	printf("section_size == 0x%x > REL_offset(*prlc) == 0x%x\n", section_size, REL_offset(*prlc));
#endif
	    return ENOEXEC;
	}
    
	/* the real thing */
	fixup_addr = scn_addr + REL_offset (*prlc);
#ifdef _64BIT_OBJECTS
	j = fixup(value, type, REL64_type2(*prlc) &&
		  (!relocatable || is_rela));
	if (!relocatable && j == RLC_NOERROR && REL64_type2(*prlc)) {
	    switch (REL64_ssym(*prlc)) {
	    case RSS_UNDEF:
		value = 0;
		break;
	    case RSS_LOC:
		value = REL_offset(*prlc);
		break;
	    case RSS_GP:
	    case RSS_GP0:
	    default:
		abort(EINVAL);
		break;
	    }
	    j = fixup (value, REL64_type2(*prlc), REL64_type3(*prlc));

	    if (j == RLC_NOERROR && REL64_type3(*prlc)) {
		j = fixup (0, REL64_type3(*prlc), is_composite);
	    }
	}
#else
	j = fixup(value, type, is_composite && (!relocatable || is_rela));

#endif /* _64BIT_OBJECTS */

	if (is_composite && j == RLC_NOERROR) {
	    if (relocatable) {
		last_addend = 0;
		if (is_rela)
		    REL_addend (*prlca) = addend;
	    } else
		last_addend = addend;
	    continue;
	} else
	    last_addend = 0;
	    

	/* put the final bit back into raw pool, or error */
	switch (j) {
	case RLC_NOERROR:
	    if (relocatable && is_rela)
		REL_addend (*prlca) = addend;
	    else
		put_relc_val(rlc_attr->val_size, raw, REL_offset(*prlc));
	    break;

	case RLC_BADTYPE:
	case RLC_HALFOVL:
	case RLC_JMPOVL:
	    return ENOEXEC;
	}
    }
    return 0;
}


/*
 * fixup of raw data due to relocation type.
 */
static
int fixup(ADDR value, ELF_WORD type, int no_update)
{
    int ret = RLC_NOERROR;

    switch (type) {
    case R_MIPS_NONE:
    case R_MIPS_REL32:
	break;
	
    case R_MIPS_32:
    case R_MIPS_SCN_DISP:
	value = (int32) (value + (int32) addend);
	if (!no_update)
	    word = value;
	break;
	
    case R_MIPS_26:
	value += (int32) addend;
	
	if (!no_update) {
	    if (jmp_out_of_range(value, fixup_addr)) {
		ret = RLC_JMPOVL;
		break;
	    }
	       
	    /* TODO: */
	    /*      1. for shared, need to skip gp-prolog */
	
	    word = (word & 0xfc000000) | 
		(0x3ffffff & (value >> 2));
	}
	break;
	
    case R_MIPS_CALL16:
    case R_MIPS_GOT16:
    case R_MIPS_GOT_DISP:
    case R_MIPS_GOT_PAGE:
	ret = RLC_BADTYPE;
	break;
	
    case R_MIPS_HI16:
	value += (int32) addend;
	if (!no_update)
	    word = (word & 0xffff0000) |
		(((value + 0x8000) >> 16) & 0xffff);
	break;
	
    case R_MIPS_GOT_OFST:
	ret = RLC_BADTYPE;
	break;

    case R_MIPS_LO16:
	value += (int16) addend;
	if (!no_update)
	    word = (word & 0xffff0000) | (value & 0xffff);
	break;

    case R_MIPS_CALL_LO16:
    case R_MIPS_GOT_LO16:
    case R_MIPS_LITERAL:
    case R_MIPS_GPREL16:
    case R_MIPS_GPREL32:
	ret = RLC_BADTYPE;
	break;
	
    case R_MIPS_64:
	value = (Elf64_Xword) value + addend;
	if (!no_update)
	    xword = value;
	break;

    case R_MIPS_SUB:
	value = (Elf64_Xword) value - addend;
	if (!no_update)
	    xword = value;
	break;
	
    case R_MIPS_16:
	value = half + value + (int16) addend;
	if (!no_update) {
	    word = value;
	    if (OVERFLOW_16(value))
		ret = RLC_HALFOVL;
	    half = word;
	}
	break;
	
    case R_MIPS_CALL_HI16:
    case R_MIPS_GOT_HI16:
	ret = RLC_BADTYPE;
	break;

    case R_MIPS_HIGHER:
	value += addend;
	if (!no_update)
	    word = (word & 0xffff0000) |
		(((value + 0x80008000LL) >> 32) & 0xffff);
	break;

    case R_MIPS_HIGHEST:
	{
	value += addend;
	if (!no_update) {
	int tmp;
	    tmp = (word & 0xffff0000);
	    tmp |= (((value + 0x800080008000LL) >> 48) & 0xffff);
	    word = tmp;
		}
	}
	break;
	    
    case R_MIPS_PC16:
    default:
	ret = RLC_BADTYPE;
    }
    addend = value;
    return ret;
}
