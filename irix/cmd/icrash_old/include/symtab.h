#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <filehdr.h>

#define SYM_SLOTS   511

#define	ST_64BIT	0x1
#define	ST_32BIT	0x2

struct syment {
    char *n_name;
    kaddr_t n_value;
    int n_type;
    int  n_scnum;
    struct procent *n_proc;
};

#define SYMESZ sizeof(struct syment)
/*
struct syment *symsrch();
*/

/* The symdef_s struct is include at the start of the dw_type_s struct. 
 * The first field in symdef_t is a btnode_t sruct. This allows the 
 * generic binary search tree routines, insert_tnode() and find_tnode(), 
 * to be used.
 */
typedef struct symdef_s {
	struct symdef_s		*sd_left;   /* Must be first */
	struct symdef_s		*sd_right;  /* Must be second */
	char				*sd_name;   /* Must be third */
	int					 sd_depth;  /* Must be forth */
	int					 sd_type;	/* type, variable, etc. */
	int					 sd_state;	/* state of the info (init/complete) */
	int		 	     	 sd_size;	/* size in bytes */
	int		 	     	 sd_offset;	/* offset of field in struct/union */
	struct symdef_s 	*sd_member;	/* link to struct/union members */
	struct symdef_s 	*sd_next;	/* list link */
	int					 sd_nmlist;	/* namelist type info is from */
} symdef_t;

/* Maximum number of open namelists
 */
#define MAXNMLIST 10

typedef struct nmlist_s {
	int						index;
	char                   *namelist;
	Elf                    *elfp;
	struct filehdr          hdr;
	Elf32_Ehdr              ehdr;
	Dwarf_Debug             ptr;
} nmlist_t;

extern nmlist_t nmlist[];
extern int curnmlist;
extern int numnmlist;

#define SYM_STRUCT_UNION	1
#define SYM_ENUMERATION		2
#define SYM_BASE			3
#define SYM_TYPEDEF			4
#define SYM_VARIABLE		5

#define SYM_INIT			1
#define SYM_COMPLETE		2

/*
 * Addresses (including ranges).
 */
typedef struct symaddr_s {
	kaddr_t 			 s_lowpc;
	kaddr_t 			 s_highpc;
	char 			    *s_name;
	struct symaddr_s    *s_next;
	struct symaddr_s    *s_nnext;
} symaddr_t;

typedef struct symlist_s {
	int          s_nstruct;			/* Number of structure types */
	symdef_t    *s_struct;			/* Link list of struct types */
	int          s_nunion;			/* Number of union types */
	symdef_t    *s_union;			/* Link list of union types */
	int          s_ntypedef;		/* Number of typedefs */
	symdef_t    *s_typedef;			/* Link list of tyepdefs */
	int          s_nenumeration;	/* Number of enumeration types */
	symdef_t    *s_enumeration;		/* Link list of enumeration types */
	int          s_nbase;			/* Number of base types */
	symdef_t    *s_base;			/* Link list of base types */
	int          s_nvariable;		/* Number of variables */
	symdef_t    *s_variable;		/* Link list of variables */
} symlist_t;

/*
 * Useful symbol table information
 */
typedef struct symtab_s {
	int			 st_flags;
	int			 st_addr_cnt;
	symaddr_t	*st_addr_ptr;
	int			 st_func_cnt;
	symaddr_t	*st_func_ptr;
	int			 st_ml_addr_cnt;    /* Number of mload symbols found */
	symaddr_t	*st_ml_addr_ptr;    /* Lined list of entries */
	int			 st_type_cnt;		/* Total number of types */
	symdef_t	*st_type_ptr;		/* Binary search tree for types */
	int			 st_var_cnt;		/* Total number of variables */
	symdef_t	*st_var_ptr;		/* Binary search tree for variables */
	symlist_t	 st_lists;			/* Linked lists to various types */
} symtab_t;

#define st_nstruct 		st_lists.s_nstruct
#define st_struct 		st_lists.s_struct
#define st_nunion 		st_lists.s_nunion
#define st_union 		st_lists.s_union
#define st_ntypedef 	st_lists.s_ntypedef
#define st_typedef 		st_lists.s_typedef
#define st_nenumeration st_lists.s_nenumeration
#define st_enumeration 	st_lists.s_enumeration
#define st_nbase 		st_lists.s_nbase
#define st_base 		st_lists.s_base
#define st_nvariable 	st_lists.s_nvariable
#define st_variable 	st_lists.s_variable

/* From lib/libsym/symbol.c
 */
void free_sym(struct syment *);
struct syment *get_sym(char *, int);
kaddr_t get_sym_addr(char *);
kaddr_t get_sym_ptr(char *);

/* From lib/libsym/symlib.c
 */
void set_curnmlist(int);
int init_nmlist(char *);
symtab_t *init_symtab(nmlist_t *);
symtab_t *init_symlib(char *);
void init_types(symtab_t *);
void init_globals(symtab_t *);
void init_vars(symtab_t *);
void init_funcnames(Dwarf_Die);
void sym_funcs(symtab_t *);
void sym_addrs(Elf *, symtab_t *);
int sym_populate_type(symdef_t *);
signed int sym_cmp64(symaddr_t *, symaddr_t *);
int sym_hash(char *);
void sym_insert_hash(symaddr_t *);
void sym_sort_list(symtab_t *);
void sym_debug_print();
symdef_t *sym_lkup(symtab_t *, char *);
symaddr_t *sym_nmtoaddr(symtab_t *, char *);
symaddr_t *sym_addrtonm(symtab_t *, kaddr_t);
int STRUCT(char *);
int FIELD(char *, char *);
int IS_FIELD(char *, char *);
int FIELD_SZ(char *, char *);
symdef_t *get_field(char *, char *);
int base_value(k_ptr_t, char *, char *, k_uint_t *);
char *get_funcname(kaddr_t);
kaddr_t get_funcaddr(kaddr_t);
int get_funcsize(kaddr_t);
char *get_srcfile(kaddr_t);
int get_lineno(kaddr_t);

/* From lib/libutil/mload.c
 */
k_ptr_t addr_to_mlinfo(kaddr_t);
k_ptr_t ml_findsym(kaddr_t, char *, k_ptr_t);
k_ptr_t ml_addrtosym(kaddr_t, k_ptr_t);
k_ptr_t ml_nmtosym(char *, k_ptr_t);
symaddr_t *ml_findaddr(kaddr_t, symaddr_t **, int);
symaddr_t *ml_findname(char *);
symaddr_t *ml_insertaddr(kaddr_t, kaddr_t, char *, symaddr_t *);
char *ml_addrtonm(kaddr_t);
kaddr_t ml_nmtoaddr(char *name);
int is_mload(kaddr_t);
char *ml_symname(char *, k_ptr_t, k_ptr_t);
kaddr_t ml_symaddr(kaddr_t);
char *ml_funcname(kaddr_t);
kaddr_t ml_funcaddr(kaddr_t);
kaddr_t ml_funcend(kaddr_t);
int ml_funcsize(kaddr_t);

#define SYMDEF(X) ((symdef_t *)(X))

#define LEVEL_INDENT(level) {\
			int i; \
			for (i = 0; i < level; i++) { \
				fprintf(ofp, "    "); \
				}\
			}
