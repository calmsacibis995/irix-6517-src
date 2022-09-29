#ident "$Header: "

#include <libelf.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <filehdr.h>

typedef struct base_s {
	char	*name;		   /* name of type */
	char	*actual_name;  /* Actual name of type if alias */
	int		 byte_size;    /* number of bytes required for storage */
	int		 encoding;	   /* char, signed, unsigned, etc. */
} base_t;

typedef struct type_s {
	int		 flag;
	union {
		struct type_s   *next;
		void			*ptr;
	} un;
} type_t;

#define t_next un.next 
#define t_ptr un.ptr 

/* Flag values
 */
#define STRING_FLAG         0x001
#define ADDRESS_FLAG        0x002
#define INDIRECTION_FLAG    0x004
#define POINTER_FLAG        0x008
#define MEMBER_FLAG         0x010
#define BOOLIAN_FLAG        0x020
#define BASE_TYPE_FLAG      0x040
#define DWARF_TYPE_FLAG     0x080
#define NOTYPE_FLAG         0x100
#define UNSIGNED_FLAG       0x200
#define VOID_FLAG           0x400

#define SYM_SLOTS   		511

#define	ST_64BIT			0x1
#define	ST_32BIT			0x2

typedef struct syment {
	char 				*n_name;
	kaddr_t 			 n_value;
	int 				 n_type;
	int  				 n_scnum;
	struct procent 		*n_proc;
} syment_t;

/* The symdef_s struct is include at the start of the dw_type_s struct. 
 * The first field in symdef_t is a btnode_t sruct. This allows the 
 * generic binary search tree routines, insert_tnode() and find_tnode(), 
 * to be used.
 */
typedef struct symdef_s {
	btnode_t			 sd_bt;
	int					 sd_type;	/* type, variable, etc. */
	int					 sd_state;	/* state of the info (init/complete) */
	int		 	     	 sd_size;	/* size in bytes */
	int		 	     	 sd_offset;	/* offset of field in struct/union */
	int					 sd_bit_size; /* size of member in bits */
	int					 sd_bit_offset; /* bit offset to data */
	struct symdef_s 	*sd_member;	/* link to struct/union members */
	struct symdef_s 	*sd_next;	/* list link */
	int					 sd_nmlist;	/* namelist type info is from */
} symdef_t;

#define sd_left sd_bt.bt_left
#define sd_right sd_bt.bt_right
#define sd_parent sd_bt.bt_parent
#define sd_name sd_bt.bt_key
#define sd_depth sd_bt.bt_height

#define SYMDEF(X) ((symdef_t *)(X))

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
	int          	   s_nstruct;		/* Number of structure types */
	symdef_t    	  *s_struct;		/* Link list of struct types */
	int          	   s_nunion;		/* Number of union types */
	symdef_t    	  *s_union;			/* Link list of union types */
	int          	   s_ntypedef;		/* Number of typedefs */
	symdef_t    	  *s_typedef;		/* Link list of tyepdefs */
	int          	   s_nenumeration;	/* Number of enumeration types */
	symdef_t    	  *s_enumeration;	/* Link list of enumeration types */
	int          	   s_nbase;			/* Number of base types */
	symdef_t    	  *s_base;			/* Link list of base types */
	int          	   s_nvariable;		/* Number of variables */
	symdef_t    	  *s_variable;		/* Link list of variables */
} symlist_t;

/*
 * Useful symbol table information
 */
typedef struct syminfo_s {
	int			       st_flags;
	nmlist_t	      *st_nmlist;
	int			       st_addr_cnt;
	symaddr_t	      *st_addr_ptr;
	int			       st_func_cnt;
	symaddr_t	      *st_func_ptr;
	int			       st_ml_addr_cnt;  /* Number of mload symbols found */
	symaddr_t	      *st_ml_addr_ptr;  /* Lined list of entries */
	int			       st_type_cnt;		/* Total number of types */
	symdef_t	      *st_type_ptr;		/* Binary search tree for types */
	int			       st_var_cnt;		/* Total number of variables */
	symdef_t	      *st_var_ptr;		/* Binary search tree for variables */
	symlist_t	       st_lists;		/* Linked lists to various types */
} syminfo_t;

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

/* This has to be placed after the basic libsym definitions
 */
#include <klib/kl_dwarflib.h>

/**
 ** Symbol table operation function prototypes
 **/

void kl_init_types(
	syminfo_t *	/* syminfo_t pointer */);

symdef_t *kl_sym_lkup(
	syminfo_t *	/* syminfo_t pointer */, 
	char *		/* symbol name */);

symaddr_t *kl_sym_nmtoaddr(
	syminfo_t *	/* syminfo_t pointer */, 
	char *		/* symbol name */);

symaddr_t *kl_sym_addrtonm(
	syminfo_t *	/* syminfo_t pointer */, 
	kaddr_t 	/* symbol address */);

int kl_setup_symtab(
	char*		/* namelist */);

void kl_set_curnmlist(
	int 		/* namelist index */);

int kl_init_nmlist(
	char*		/* namelist */);

/* Given symbol name, returns symbol address. In case of error, NULL 
 * will be returned and klib_error will be set to indicate exactly what 
 * error occurred.
 */
kaddr_t kl_sym_addr(
	char*		/* name of symbol */);

/* Given symbol name, returns the pointer the symbol address points 
 * to. In case of error, klib_error will be set to indicate exactly what
 * error occurred. Note that klib_error must be checked for failure as
 * a NULL return value could be valid.
 */
kaddr_t kl_sym_pointer(
	char*		/* name of symbol */);

/* Given struct name, returns struct length. In case of error, a length
 * of zero is returned and klib_error will be set to indicate exactly
 * what error occurred.
 */
int kl_struct_len(
	char*		/* struct name */);

/* Given struct name and member name, returns byte offset of member in 
 * struct. In case of error, -1 will be returned and klib_error will be
 * set to indicate exactly what error occurred.
 */
int kl_member_offset(
	char*		/* struct name */, 
	char*		/* field name */);

/* Given struct name and member name, return length of member in 
 * bytes. In case of error, 0 will be returned and klib_error will be
 * set to indicate exactly what error occurred.
 */
int kl_is_member(
	char*		/* struct name */, 
	char*		/* field name */);

/* Given struct name and member name, return length of member in 
 * number of bits. In case of error, 0 will be returned and klib_error 
 * will be set to indicate exactly what error occurred.
 */
int kl_member_size(
	char*		/* struct name */, 
	char*		/* field name */);

/* Given struct name and member name, return the base value of
 * member reletive to the struct contained in a buffer. In case 
 * of error, klib_error will be set to indicate exactly what error 
 * occurred. Note that in case of error, the return value will
 * be undefined.
 */
k_uint_t kl_member_baseval(
	k_ptr_t		/* pointer to start of struct */, 
	char*		/* struct name */, 
	char*		/* field name */); 

symdef_t *kl_get_member(
	char*		/* struct name */, 
	char*		/* field name */);

char *kl_funcname(
	kaddr_t 	/* program counter */);

kaddr_t kl_funcaddr(
	kaddr_t 	/* program counter */);

int kl_funcsize(
	kaddr_t 	/* program counter */);

char *kl_srcfile(
	kaddr_t 	/* program counter */);

int kl_lineno(
	kaddr_t 	/* program counter */);

void kl_free_sym(
	struct syment*	/* pointer to struct containing symbol info */);

syment_t *kl_get_sym(
	char*		/* name of symbol */, 
	int 		/* block allocation flag (K_TEMP/K_PERM */);

char *kl_sym_name(
	kaddr_t 	/* address of symbol */);

/**
 ** Loadable module operation function prototyeps
 **/
kaddr_t ml_nmtoaddr(
	char*		/* symbol name */);

char *ml_addrtonm(
	kaddr_t 	/* symbol addrress */);

kaddr_t ml_symaddr(
	kaddr_t 	/* symbol addrress */);

symaddr_t*ml_findname(
	char*		/* symbol name */);

symaddr_t *ml_findaddr(
	kaddr_t 	/* address to look up */, 
	symaddr_t**	/* place to put insert pointer (can be NULL) */, 
	int 		/* flag - if 1 and symbol not found, return last one */);

char *ml_funcname(
	kaddr_t 	/* address from function */);

kaddr_t ml_funcaddr(
	kaddr_t 	/* address from function */);

int ml_funcsize(
	kaddr_t 	/* address from function */);

