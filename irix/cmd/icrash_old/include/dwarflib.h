#include <filehdr.h>

/* 
 * Some structures for Dwarf operations
 */
typedef struct dw_die_s {
	struct dw_die_s	 	 *d_next;
	Dwarf_Half		 	  d_tag;		    /* Dwarf tag value */
	char 				 *d_name;			/* typedef, type, etc. name */
	Dwarf_Off	 	 	  d_off;			/* die offset used for hashing */
	Dwarf_Off 	 	 	  d_type_off;		/* The useful attributes ... */
	Dwarf_Off 	 	 	  d_sibling_off;
	Dwarf_Unsigned 	 	  d_loc;
	Dwarf_Unsigned 	 	  d_byte_size;
	Dwarf_Unsigned	 	  d_bit_offset;
	Dwarf_Unsigned	 	  d_bit_size;
	Dwarf_Unsigned	 	  d_address_class;
	Dwarf_Unsigned		  d_encoding;
	Dwarf_Unsigned	 	  d_start_scope;
	Dwarf_Unsigned	 	  d_prototyped;
	Dwarf_Unsigned	 	  d_upper_bound;
	Dwarf_Unsigned	 	  d_lower_bound;
	Dwarf_Unsigned	 	  d_const;
	Dwarf_Unsigned		  d_count;
	Dwarf_Unsigned		  d_external;
	Dwarf_Unsigned		  d_declaration;
} dw_die_t;

/* The dw_type_s struct is actually a superset of the symdef_s struct.
 * It contains additional information of interest only to Dwarf routines.
 * By having the symdef_s struct located first, generic symbol table
 * routines and Dwarf specific routines can use the same set of data.
 */
typedef struct dw_type_s {
	symdef_t			  dt_sd;	/* Must be first */
    Dwarf_Half            dt_tag;
    Dwarf_Off		      dt_off;
    Dwarf_Off             dt_type_off;
} dw_type_t;

#define dt_left 	dt_sd.sd_left
#define dt_right 	dt_sd.sd_right
#define dt_name 	dt_sd.sd_name
#define dt_depth 	dt_sd.sd_depth
#define dt_type 	dt_sd.sd_type
#define dt_state 	dt_sd.sd_state
#define dt_size		dt_sd.sd_size
#define dt_offset	dt_sd.sd_offset
#define dt_member	dt_sd.sd_member
#define dt_next		dt_sd.sd_next
#define dt_nmlist	dt_sd.sd_nmlist

#define DW_TYPE(X) ((dw_type_t *)(X))

typedef struct dw_type_info_s {
	char					*t_name;
	Dwarf_Half		 		 t_tag;
	Dwarf_Off	 	 		 t_off;
	Dwarf_Unsigned	 		 t_loc;
	Dwarf_Unsigned	 		 t_byte_size;
	Dwarf_Unsigned   		 t_bit_offset;
	Dwarf_Unsigned   		 t_bit_size;
	Dwarf_Unsigned 	 		 t_encoding;
	Dwarf_Off		 		 t_member_off;
	Dwarf_Off		 		 t_sibling_off;
	char					*t_type_str;
	dw_die_t				*t_type;
	dw_die_t				*t_actual_type;
	dw_die_t				*t_dlist;
	struct dw_type_info_s  	*t_link;
} dw_type_info_t;

#define BASE_TYPE			0x001
#define	STRUCTURE_TYPE		0x002
#define UNION_TYPE			0x004
#define POINTER_TYPE		0x008
#define ARRAY_TYPE			0x010
#define SUBROUTINE_TYPE		0x020
#define ENUMERATION_TYPE	0x040
#define SUBRANGE_TYPE		0x080
#define PROTOTYPED			0x100
#define TYPEDEF				0x200

#define NO_INDENT			0x10000000
#define SUPPRESS_NAME		0x20000000
#define SUPPRESS_NL     	0x40000000
#define SUPPRESS_SEMI_COLON	0x80000000

#define DBG nmlist[curnmlist].ptr

/* Dwarf specific function prototypes
 */
Dwarf_Die dw_die(Dwarf_Off);
Dwarf_Off dw_off(Dwarf_Die);
Dwarf_Half dw_tag(Dwarf_Die);
Dwarf_Die dw_child(Dwarf_Die);
Dwarf_Off dw_child_off(Dwarf_Die);
Dwarf_Die dw_sibling(Dwarf_Die);
Dwarf_Off dw_sibling_off(Dwarf_Die);
int dw_type_off(Dwarf_Die, Dwarf_Off, Dwarf_Off *);
ulong dw_loc_list(Dwarf_Attribute);
dw_die_t *dw_free_die(dw_die_t *);
void dw_clean_die(dw_die_t *);
void dw_free_die_chain(dw_die_t *);
void dw_free_type_info(dw_type_info_t *);
void dw_clean_type_info(dw_type_info_t *t);
Dwarf_Signed dw_attrlist(Dwarf_Die, Dwarf_Attribute **);
Dwarf_Half dw_attr(Dwarf_Attribute);
Dwarf_Unsigned dw_attr_value(Dwarf_Attribute);
char *dw_attr_name(Dwarf_Attribute, int);
Dwarf_Off dw_attr_type(Dwarf_Die, Dwarf_Attribute);
dw_die_t *dw_die_info(Dwarf_Off, dw_die_t *, int);
char *dw_type_str(dw_type_info_t *, int);
dw_type_info_t *dw_type_info(Dwarf_Off, dw_type_info_t *, int);
dw_type_t *dw_make_type(char *);
dw_type_t *dw_type(dw_type_info_t *, int);
int dw_populate_type(dw_type_t *);
int dw_add_type(symtab_t *, dw_type_t *);
int dw_add_variable(symtab_t *, dw_type_t *);
void dw_link_type(symtab_t *, dw_type_t *);
dw_type_t *dw_lkup(symtab_t *, char *, int);
dw_type_t *dw_lkup_link(symtab_t *, char *, int);
dw_type_t *dw_find_member(dw_type_t *, char *);
type_t *dw_lkup_member(symtab_t *, dw_type_info_t *, char *, int);
type_t *dw_var_to_type(dw_type_info_t *, int);
dw_type_t *dw_get_type(Dwarf_Off);
int dw_complete_type(dw_type_t *);
int dw_init_types();
int dw_init_globals();
void dw_init_vars();
int dw_struct(char *);
int dw_field(char *, char *);
int dw_is_field(char *, char *);
int dw_field_sz(char *, char *);
dw_type_t *dw_get_field(char *, char *);
int dw_base_value(k_ptr_t, dw_type_t *, k_uint_t *);
void dw_free_current_module_dwarf_memory(void);
int dw_set_current_module(kaddr_t);
Dwarf_Line dw_get_line(kaddr_t);
char *dw_get_srcfile(kaddr_t);
int dw_get_lineno(kaddr_t);
char *dw_typestr(type_t *, int);
void dw_print_base_value(k_ptr_t, dw_type_info_t *, int, FILE *);
void dw_print_base_type(k_ptr_t, dw_type_info_t *, int, FILE *);
void dw_print_subroutine_type(k_ptr_t, dw_type_info_t *, int, FILE *);
int dw_print_array_type(k_ptr_t, dw_type_info_t *, int, int, FILE *);
void dw_print_enumeration_type(k_ptr_t, dw_type_info_t *, int, FILE *);
void dw_print_pointer(k_ptr_t, dw_type_info_t *, int, FILE *);
void dw_print_pointer_type(k_ptr_t, dw_type_info_t *, int, FILE *);
void dw_print_typedef(k_ptr_t, dw_type_info_t *, int, int, FILE *);
void dw_print_type(k_ptr_t, dw_type_info_t *, int, int, FILE *);
void dw_print_type(k_ptr_t, dw_type_info_t *, int, int, FILE *);
void dw_dump_struct(k_ptr_t, dw_type_info_t *, int, int, FILE *);
void dw_dowhatis(command_t);
int dw_dotype(command_t);
int dw_print_struct(char *, kaddr_t, k_ptr_t, int, FILE *);


/* cmds/cmd_die.c
 */
void dw_print_die(dw_die_t *, FILE *);
int die_cmd(command_t);
void die_usage(command_t);
void die_help(command_t);
int die_parse(command_t);

/* cmds/cmd_type.c
 */
void walk_type_tree(dw_type_t *, FILE *);
int type_cmd(command_t);
void type_usage(command_t);
void type_help(command_t);
int type_parse(command_t);

