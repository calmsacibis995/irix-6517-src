#ifndef UNIFDEF_H
#define UNIFDEF_H
/*
 * Unifdef declarations.
 */
#ifdef METERING
#define	METER(x)	x
#else
#define	METER(x)
#endif

/*
 * Symbol table stuff.  A symbol can be a reserved word, a "dont-care"
 * identifier tested in an ifdef that should be passed unchanged to output,
 * or a symbol defined or undefined to control stripping of ifdefs.
 */
enum symtype { SRESERVED, SDONTCARE, SDEFINED, SUNDEFINED };

struct symbol {
	char		*name;		/* malloc'd copy of symbol name */
	enum symtype	type;		/* symbol type code */
	long		value;		/* value if defined, else 0 */
	char		*defn;		/* definition string if -Dname=defn */
	unsigned int	refcnt;		/* number of pointer references */
	struct symbol	*next;		/* symbol table linkage */
};

struct symbol	*lookup(char *);
void		install(char *, enum symtype, long, char *);
void		dropsym(struct symbol *);

/*
 * Expression tree declarations.  An expression tree node may be a binary
 * or unary operator with kids, or a leaf pointing into the symbol table,
 * or a number.  Symbol table pointers add references to the pointed-at
 * symbol, and destroyexpr drops references via dropsym.
 */
enum operator {
	EIFELSE,			/* ?: */
	EOR,				/* || */
	EAND,				/* && */
	EBITOR,				/* | */
	EBITXOR,			/* ^ */
	EBITAND,			/* & */
	EEQ, ENE,			/* ==, != */
	ELT, ELE, EGT, EGE,		/* <, <=, >, >= */
	ELSH, ERSH,			/* <<, >> */
	EADD, ESUB,			/* +, - */
	EMUL, EDIV, EMOD,		/* *, /, % */
	ENOT, EBITNOT, ENEG,		/* !, ~, - */
	EDEFINED, EIDENT, ENUMBER
};

struct expr {
	enum operator	    e_op;	/* operator code */
	short		    e_arity;	/* number of kids */
	char		    e_paren;	/* whether to parenthesize */
	char		    e_reduced;	/* were we reduced by evalexpr? */
	union {
	    struct {
		struct expr *cond;	/* condition before ? */
		struct expr *true;	/* expression evaluated if true */
		struct expr *false;	/* false expression, after : */
	    } ifelse;
	    struct {
		struct expr *left;	/* binary tree linkage */
		struct expr *right;	/* also, kid if unary */
	    } links;
	    struct symbol   *sym;	/* symbol if EDEFINED or EIDENT */
	    long	    val;	/* value if ENUMBER */
	} e_u;
};
#define	e_cond	e_u.ifelse.cond
#define	e_true	e_u.ifelse.true
#define	e_false	e_u.ifelse.false
#define	e_left	e_u.links.left
#define	e_right	e_u.links.right
#define	e_kid	e_right
#define	e_sym	e_u.sym
#define	e_val	e_u.val

struct expr	*parseexpr(char *);
void		destroyexpr(struct expr *);
enum symtype	evalexpr(struct expr *);
void		yyerror(const char *);

/*
 * Type-oriented malloc interface.
 */
#define	new(t)		((t *) _new(sizeof(t)))
#define	newvec(n,t)	((t *) _new((n) * sizeof(t)))

void		*_new(int);
void		delete(void *);

/*
 * Error handling stuff.
 */
void		Perror(char *);
void		terminate(void);

extern char	*progname;
extern int	status;

#endif
