/* ripped off from linker  for use in relocate.c */

/* data structures for information related to object files */

#ifndef __OBJ_FILE_H__
#define __OBJ_FILE_H__

#include <sys/types.h>

#include "ld_elf.h"
#include "ld_defs.h"

/* Special Elf sections */
typedef enum {
    SEC_FIRST_ENUM,
    SEC_AUXSYM,	SEC_BSS,	SEC_COMMENT,	SEC_CONFLICT,	SEC_DATA,
    SEC_DATA1,	SEC_DEBUG,	SEC_DENSE,	SEC_DYNAMIC,	SEC_DYNSTR,
    SEC_DYNSYM,	SEC_EXTSTR,	SEC_EXTSYM,	SEC_FDESC,	SEC_FINI,
    SEC_GOT,	SEC_HASH,	SEC_INIT,	SEC_INTERFACES, SEC_INTERP,
    SEC_LIB,	SEC_LIBLIST,	SEC_LINE,	SEC_LIT16,	SEC_LIT4,
    SEC_LIT8,	SEC_LOCSTR,	SEC_LOCSYM,	SEC_MDEBUG,	SEC_NOTE,
    SEC_OPTIONS,SEC_OPTSYM,	SEC_PDESC,	SEC_PLT,	SEC_REGINFO,
    SEC_RELDYN,	SEC_RFDESC,	SEC_RODATA,	SEC_RODATA1,	SEC_SBSS,
    SEC_SDATA,	SEC_SHSTRTAB,	SEC_STRTAB,	SEC_SYMHEADER,	SEC_SYMTAB,
    SEC_UCODE,	SEC_TEXT,	SEC_LDATA,	SEC_REL_LDATA,	SEC_RELA_LDATA,
    SEC_LBSS,	SEC_MSYM,
    SEC_LAST_ENUM
} section_hash_index;

/* table of memory requirement for special sections */
typedef struct {
    string name;
    section_hash_index sec;	    /* special section */
} key_section_info;

extern key_section_info *special_section[];

#define SEC_NAME(x) special_section[(x)]->name

/* object file type */
#define FT_UNKNOWN	0	    /* unknown type */
#define FT_UCODE	1	    /* ucode object */
#define FT_OBJ		2	    /* regular object file */
#define FT_AR		3	    /* archives */
#define FT_SO		4	    /* shared objects */
#define FT_IGNORE	5	    /* don't process */
#define FT_DUPLICATE_SO	6	    /* duplicated dso */

/* file access methods */
#define AM_MMAP		0	    /* use mmap */
#define AM_READ		1	    /* use malloc-read for the entire file */

/* bit patterns for SCNINFO.state */
#define STATE_INFILE	0	    /* not in core */
#define STATE_MMAPPED	1	    /* in core via mmap */
#define STATE_MALLOCED	2	    /* has a copy in core (via malloc) */
#define STATE_OUTFILE	4	    /* already written out to file */
#define STATE_MEM_MASK	0x3	    /* mask for the first three states */
#define STATE_UNALIGNED	0x10	    /* set if buffer is unaligned */
#define STATE_SWAPPED	0x20	    /* set if byte-sex already swapped */
#define STATE_WRITABLE	0X40	    /* set if buffer is writable */

/* bit patterns for placement field */
#define PL_UNPLACED 0               /* has not been placed layout yet */
#define PL_GENERIC  1               /* current placement due to generic */
#define PL_SPECIFIC 2               /* current placement due to specific */
    
#if 0
typedef struct {
    string	name;		    /* section name */
    pointer	p_raw;		    /* pointer to raw data of this section */
    int        *ofs_map;            /* offset map from old to merged */
    struct sec_class *class;	    /* corresponding SECTION_CLASS struct */
    int 	cmp_rel;	    /* size of compact relocation events */
    int 	class_idx;	    /* index to the SECTION_ARRAY in class */
    int		n_rel;		    /* number of relocation entries */
    int		n_rela;		    /* number of relocation with addends */
    ELF_WORD	rel_idx;	    /* index to corresponding rel section */
    ELF_WORD	rela_idx;	    /* index to corresponding rela section */
    int32	state;		    /* state of this buffer STATE_... */
    char        placement;          /* placement state, see lspec.c */
    int		align_count;	    /* number of alignment specs */
    int		align_max;          /* number of align specs allocated */
    palignspec  asTable;            /* array of alignment specs (align.h) */
} SCNINFO, *pSCNINFO;
#endif


/* We have one OBJECT_FILE structure for each input object file read.
   The "section_table" field points to the section header table in the
   actual object file, and the "sections" field points to an array of
   structures such as the one above (SCNINFO).  When an input section is
   determined to belong to a particular "kind" of output section, the
   "class" field of the SCNINFO record is filled out as well as the
   "class_idx" field, locating the particular input section in the array
   of input sections grouped together as a class.
   */

typedef struct object_file {
    
    ELF_SHDR	*section_table;	    /* raw section headers table */

} OBJECT_FILE, *pOBJECT_FILE;

/* one such structure for each input section that gets copied to the output */
typedef struct sec_array {
    pOBJECT_FILE	obj;	    /* object that defines this section */
    struct sec_list	*out_sec;   /* where in the output section queue */
    OFFSET 		(*addr_fixup) (ELF_ADDR, struct sec_array *);
				    /* further address fixup */
    pointer		stub_raw;   /* initialized stub data */
    pointer		out_stub_raw; /* ptr to the corresponding out buf */
    pointer		pad_raw;    /* initialized padding data */
    pointer		out_pad_raw; /* corresponding output buffer */
    pointer		out_raw;    /* output buf for the section */
    FILE_SZ 		size;	    /* size of original section */
    FILE_SZ		stub_size;  /* size of stub (prepend) */
    FILE_SZ		pad_size;   /* size of padding (append) */
    OFFSET		offset;	    /* output file offset */
    ADDR 		s_vaddr;    /* output starting virtual address */
    ADDR		align;	    /* alignment */
    ELF_INT		idx;	    /* index to the "sections" array */
} SECTION_ARRAY, *pSECTION_ARRAY;


/* One such structure for each "class" of input sections identified by
   name and the attribute flags */
typedef struct sec_class {
    struct sec_class	*next;
    string 		name;
    ELF_INT		flags;
    ELF_INT		type;
    pSECTION_ARRAY 	sec;
    unsigned short	last;	    /* number of elements in sec */
    unsigned short	max;	    /* max number of elements in sec */
} SECTION_CLASS, *pSECTION_CLASS;


#define OBJ_ASSERT(EX, obj, str) \
    if (!(EX)) error (ER_FATAL, ERN_OBJ, obj->name, str);

#define OBJ_ALIGNMENT (16)	    /* alignment for object file */

extern OBJECT_FILE dummy_obj;	    /* holder for ld-defined sections */
extern int num_objects_linked;

#define get_scn_size(shndx, pobj) \
    (pobj)->section_table[(shndx)].sh_size

#define get_shlink_scn(shndx, pobj) \
    (pobj)->section_table[(shndx)].sh_link

#define get_shinfo_scn(shndx, pobj) \
    (pobj)->section_table[(shndx)].sh_info


#define get_scninfo(index, pobj) \
    ((pobj)->sections + (index))

#define idx_to_input_sec(shndx, pobj) \
    ((pobj)->sections[(shndx)].class->sec+(pobj)->sections[(shndx)].class_idx)

#define get_dummy_key_section_raw(hash_index) \
    get_section_raw (&dummy_obj, dummy_obj.key_sections[hash_index])

#define get_section_out_raw(shndx, pobj) \
    (idx_to_input_sec((shndx), (pobj))->out_raw)

#define get_entsize(shndx, pobj) \
    (((pobj)->section_table[(shndx)].sh_flags & SHF_MIPS_MERGE) ? \
     (pobj)->section_table[(shndx)].sh_entsize : eval_entsize((shndx), (pobj)))

#define is_gprel(shndx, pobj) \
    ((pobj)->section_table[(shndx)].sh_type & SHF_MIPS_GPREL)

extern void
rewind_sec_class(void);

extern pSECTION_CLASS
next_sec_class(void);

extern int
new_ld_section (string, ELF_INT, ELF_INT, ELF_SIZE, ELF_SIZE, ELF_SIZE,
		int32, pointer);

extern void
rewind_input_sec (void);

extern pSECTION_CLASS
get_input_sec (string, ELF_INT, ELF_INT, int, int *);

extern ELF_SHDR *
input_sec_to_elf_shdr (pSECTION_CLASS, int);

extern void
enter_sec_array (pOBJECT_FILE, int, ELF_INT, ELF_INT);

extern OFFSET
align_archive_member (pOBJECT_FILE, FILE_SZ, char *, OFFSET);

extern pOBJECT_FILE
alloc_object (string);

extern void
add_object (pOBJECT_FILE);

extern void
dealloc_object (pOBJECT_FILE);

extern void
add_dso (pOBJECT_FILE);    

extern void
add_archive (pOBJECT_FILE);

extern pOBJECT_FILE
duplicate_library (ino_t, dev_t);

extern pOBJECT_FILE
duplicate_so(char *);

extern ADDR
get_obj_gp (pOBJECT_FILE);    

extern ELF_SYM *
reloc_idx_to_symtab (ELF_INT, pOBJECT_FILE, ELF_INT *);

extern string
reloc_idx_to_strtab (ELF_INT, pOBJECT_FILE);

extern pOBJECT_FILE
get_next_so (register pOBJECT_FILE);    

extern pOBJECT_FILE
get_next_obj (register pOBJECT_FILE);

extern ELF_SIZE
eval_entsize (int, pOBJECT_FILE);    

extern void
move_input_section (pOBJECT_FILE, int, ELF_INT, ELF_INT);

extern pointer
get_section_raw (pOBJECT_FILE, int);

#endif /* __OBJ_FILE_H__ */
