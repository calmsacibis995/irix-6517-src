#ident  "$Header: "

/* Structure to hold info on "tokens" extracted from eval and print
 * command input strings.
 */
typedef struct token_s {
	short	 		 type;
	short    		 operator;	/* if token is an operator */
	char            *string;	/* string containing value or identifier */
	char            *ptr;		/* pointer to start of token in cmd string */
	struct token_s  *next;		/* next token in the chain */
} token_t;

/* Structure returned by the eval() function containing the result
 * of an expression evaluation. This struct is also used to build the
 * parse tree for the expression.
 */
typedef struct node_s {
    unsigned char    node_type;	/* type of node */
    unsigned short   flags;		/* see below */
    unsigned char    operator;	/* operator if node is type OPERATOR */
	unsigned char	 byte_size; /* byte_size of base_type values */
	char			*name;		/* name of variable or struct|union member */
    k_uint_t         value;		/* numeric value or pointer */
    k_uint_t         address;	/* address (could be same as pointer)  */
	type_t			*type;  	/* pointer to type related info */
    char            *tok_ptr;	/* pointer to token in cmd string */
    struct node_s   *left;		/* pointer to left child */
    struct node_s   *right;		/* pointer to right child */
} node_t;

/* Token and Node types
 */
#define OPERATOR         1
#define NUMBER     		 2
#define INDEX     		 3
#define TYPE_DEF		 4
#define VARIABLE     	 5
#define VADDR        	 6
#define MEMBER			 7
#define STRING			 8
#define TEXT			 9
#define CHARACTER		10
#define EVAL_VAR    	11

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

/* Flag value for print_eval_error() function
 */
#define CMD_NAME					1  /* cmdname is not name of a command */
#define CMD_STRING					2  /* cmdname is not name of a command */

/* Expression operators in order of precedence. 
 */
#define CONDITIONAL				    1
#define CONDITIONAL_ELSE            2
#define LOGICAL_OR				    3
#define LOGICAL_AND				    4
#define BITWISE_OR				    5
#define BITWISE_EXCLUSIVE_OR        6
#define BITWISE_AND				    7
#define EQUAL					    8
#define NOT_EQUAL				    9
#define LESS_THAN				   10
#define GREATER_THAN			   11
#define LESS_THAN_OR_EQUAL		   12
#define GREATER_THAN_OR_EQUAL	   13
#define RIGHT_SHIFT				   14
#define LEFT_SHIFT 				   15
#define ADD						   16	
#define SUBTRACT				   17	
#define MULTIPLY				   18
#define DIVIDE					   19
#define MODULUS					   20
#define LOGICAL_NEGATION		   21
#define ONES_COMPLEMENT			   22
#define PREFIX_INCREMENT		   23
#define PREFIX_DECREMENT		   24
#define POSTFIX_INCREMENT		   25
#define POSTFIX_DECREMENT		   26
#define CAST					   27
#define UNARY_MINUS				   28
#define UNARY_PLUS				   29
#define INDIRECTION				   30
#define ADDRESS					   31
#define SIZEOF					   32
#define RIGHT_ARROW				   33
#define DOT 					   34
#define OPEN_PAREN 				  100	
#define CLOSE_PAREN	 			  101	
#define OPEN_SQUARE_BRACKET 	  102	
#define CLOSE_SQUARE_BRACKET	  103 
#define SEMI_COLON				  104
#define NOT_YET 				   -1

/* Function prototypes
 */
base_t *get_base_type(char *);
type_t *get_type(char *, int);
void free_type(type_t *);
type_t *member_to_type(void *, int, int);
int is_keyword(char *, int);
token_t *get_token_list(char *, int *);
int is_unary(int);
int is_binary(int);
int precedence(int);
node_t *make_node(token_t *, int);
node_t *get_node(token_t **, int);
int add_node(node_t *, node_t *);
int add_rchild(node_t *, node_t *);
int add_lchild(node_t *, node_t *);
char *node_to_typestr(node_t *, int);
void free_tokens(token_t *);
void free_nodes(node_t *);
node_t *get_sizeof(token_t **, int *);
node_t *do_eval(token_t **, int, int *);
node_t *get_array_index(token_t **, int *);
int replace_cast(node_t *, int *, int);
int apply_unary(node_t *, k_uint_t *, int *);
int replace_unary(node_t *, int *, int);
int apply(node_t *, k_uint_t *, int *, int);
node_t *replace(node_t *, int *, int);
int replace_variable(node_t *, int *, int);
void array_to_element(node_t*, node_t*, int*);
int type_to_number(node_t *, int *);
type_t *eval_type(node_t *);
node_t *eval(char **, int, int *);
void print_eval_error(char *, char *, char *, int, int);
