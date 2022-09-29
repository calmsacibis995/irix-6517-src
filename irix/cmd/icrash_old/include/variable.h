#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/variable.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

/* Struct to hold information about eval variables
 */
typedef struct variable_s {
    btnode_t             v_bt;  /* Must be first */
    int                  v_flags;
	char				*v_exp;     /* What was entered on command line      */
	char				*v_typestr; /* Actual type string after eval() call  */
    node_t              *v_node;
} variable_t;

#define v_left v_bt.bt_left
#define v_right v_bt.bt_right
#define v_name v_bt.bt_key

/* Flag values
 */
#define V_PERM         0x001  /* can't be unset - can be modified            */
#define V_DEFAULT      0x002  /* set at startup                              */
#define V_NOMOD        0x004  /* cannot be modified                          */
#define V_TYPEDEF      0x008  /* contains typed data                         */
#define V_REC_STRUCT   0x010  /* direct ref to struct/member (not pointer)   */
#define V_STRING	   0x020  /* contains ASCII string (no type)             */
#define V_COMMAND	   0x040  /* contains command string (no type)           */
#define V_OPTION 	   0x080  /* contains option flag (e.g., $hexints)       */
#define V_PERM_NODE    0x100  /* Don't free node after setting variable      */

/* Variable table struct
 */
typedef struct vtab_s {
    variable_t          *vt_root;
    int                  vt_count;
} vtab_t;

extern vtab_t *vtab;            /* Pointer to table of eval variable info    */

/* Function Prototypes
 */
variable_t *make_variable(char *, char *, node_t *, int);
void clean_variable(variable_t *);
void free_variable(variable_t *);
void init_variables(vtab_t *);
int set_variable(vtab_t *, char *, char *, node_t *, int);
int unset_variable(vtab_t *, variable_t *);
variable_t *find_variable(vtab_t *, char *, int);

