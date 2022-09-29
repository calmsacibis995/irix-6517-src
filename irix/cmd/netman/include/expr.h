#ifndef EXPR_H
#define EXPR_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Filter expression parsing.
 */
#include "address.h"
#include "strings.h"

struct snooper;
struct protocol;
struct scope;

enum exop {
	EXOP_NULL,		/* no-op */
	EXOP_OR,
	EXOP_AND,
	EXOP_EQ, EXOP_NE,
	EXOP_LE, EXOP_LT, EXOP_GE, EXOP_GT,
	EXOP_BITOR,
	EXOP_BITXOR,
	EXOP_BITAND,
	EXOP_LSH, EXOP_RSH,
	EXOP_ADD, EXOP_SUB,
	EXOP_MUL, EXOP_DIV, EXOP_MOD,
	EXOP_NOT, EXOP_BITNOT, EXOP_NEG,
	EXOP_ARRAY, EXOP_STRUCT,
	EXOP_PROTOCOL, EXOP_FIELD, EXOP_NUMBER, EXOP_ADDRESS, EXOP_STRING,
	EXOP_FETCH, EXOP_CALL
};

enum exarity { EX_NULLARY, EX_UNARY, EX_BINARY };

typedef enum exprtype { ET_SIMPLE, ET_COMPLEX, ET_ERROR } ExprType;

/*
 * Binary tree node linkage.
 */
struct exlinkage {
	struct expr	*exl_left;
	struct expr	*exl_right;
};

/*
 * Special linkage for protocol pathnames.
 */
struct exprotocol {
	struct symbol	*exp_prsym;	/* protocol to left of . */
	struct expr	*exp_member;	/* right hand side of . */
};

/*
 * Fetch pseudo-macro arguments.
 */
struct exfetchargs {
	unsigned int	exf_size;	/* number of bytes to fetch */
	unsigned int	exf_off;	/* offset of first byte */
	enum exop	exf_type;	/* type of expression to fetch */
};

/*
 * Protocol function call information.
 */
#define	MAXCALLARGS	4

struct excallfunc {
	int		(*exc_func)();
	struct expr	*exc_args[MAXCALLARGS];
};

/*
 * The tree node.
 */
typedef struct expr {
	enum exop		ex_op;		/* operation or terminal code */
	enum exarity		ex_arity;	/* number of kids */
	unsigned int		ex_count;	/* reference count */
	char			*ex_token;	/* token ptr for error report */
	struct exprsource	*ex_src;	/* and source info */
	union {
	    struct exlinkage	exu_links;	/* pointers to kids */
	    struct exprotocol	exu_proto;	/* protocol path component */
	    struct protofield	*exu_field;	/* field at end of path */
	    long		exu_val;	/* integer constant */
	    union address	exu_addr;	/* protocol-specific address */
	    struct string	exu_str;	/* protocol string literal */
	    struct exfetchargs	exu_fetch;	/* fetch(size,off) args */
	    struct excallfunc	exu_call;	/* protocol function call */
	} ex_u;
} Expr;

#define	ex_left		ex_u.exu_links.exl_left
#define	ex_right	ex_u.exu_links.exl_right
#define	ex_kid		ex_right
#define	ex_prsym	ex_u.exu_proto.exp_prsym
#define	ex_member	ex_u.exu_proto.exp_member
#define	ex_field	ex_u.exu_field
#define	ex_val		ex_u.exu_val
#define	ex_addr		ex_u.exu_addr
#define	ex_str		ex_u.exu_str
#define	ex_fetch	ex_u.exu_fetch
#define	ex_size		ex_fetch.exf_size
#define	ex_off		ex_fetch.exf_off
#define	ex_type		ex_fetch.exf_type
#define	ex_call		ex_u.exu_call
#define	ex_func		ex_call.exc_func
#define	ex_args		ex_call.exc_args

/*
 * Source record passed to the parser.  If src_path is null, the object is
 * assumed to be dynamically allocated, and src_line counts references from
 * expression nodes created when parsing this source.  When all nodes have
 * been destroyed, their source record is deleted.
 */
typedef struct exprsource {
	char		*src_path;	/* file pathname or envariable name */
	unsigned int	src_line;	/* line number or 0 if not applicable */
	char		*src_buf;	/* buffered, terminated input line */
} ExprSource;

#define	src_refcnt	src_line

#define	newExprSource(src, buf) \
	((src) = new(ExprSource), \
	 (src)->src_path = 0, (src)->src_line = 0, (src)->src_buf = (buf))
#define	holdExprSource(src) \
	((src) && (src)->src_path == 0 && (src)->src_refcnt++, (src))
#define	dropExprSource(src) \
	((src) && (src)->src_path == 0 && --(src)->src_refcnt == 0 ? \
	 delete(src) : (void) 0)

void	ex_holdsrc(Expr *, ExprSource *);
void	ex_dropsrc(Expr *);

/*
 * If ex_parse returns 0, this structure contains error information.
 * Call ex_error to set these members given an expr node.
 */
typedef struct exprerror {
	char		*err_message;	/* diagnostic message */
	char		*err_token;	/* pointer to erroneous token */
	ExprSource	err_source;	/* input source information */
} ExprError;
#define	err_path	err_source.src_path
#define	err_line	err_source.src_line
#define	err_buf		err_source.src_buf

/*
 * Initialize an auto or static Expr to have no value.
 */
#define	ex_null(ex)	bzero((char *)(ex), sizeof *(ex))

/*
 * Expression constructors, destructor, and operations.  Ex_parse, ex_operate,
 * ex_eval, ex_error, and ex_badop raise exceptions.
 */
Expr	*expr(enum exop, enum exarity, char *);
Expr	*ex_parse(ExprSource *, struct snooper *, struct protocol *,
		  ExprError *);
Expr	*ex_string(char *, int);
Expr	*ex_typematch(Expr *, enum exop, struct scope **);
void	ex_destroy(Expr *);

ExprType ex_operate(Expr *, enum exop, Expr *, Expr *, ExprError *);
ExprType ex_eval(Expr *, ExprError *);

int	ex_test(Expr *);
void	ex_error(Expr *, char *, ExprError *);
void	ex_badop(Expr *, ExprError *);

#endif
