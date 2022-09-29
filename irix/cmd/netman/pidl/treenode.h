#ifndef TREENODE_H
#define TREENODE_H
/*
 * PIDL abstract syntax tree nodes.
 */
#include "address.h"
#include "scope.h"
#include "type.h"

enum operator {
	opNil,
	opList,
	opImport, opExport, opPragma,
	opProtocol, opQualify, opBase, opNest,
	opDeclaration,
	opIfElse,					/* if-else */
	opSwitch, opCase, opDefault,
	opStruct,
	opMacro,
	opEnum, opBitSet, opEnumerator,
	opBlock,					/* {,,} */
	opComma,					/* , */
	opAssign,					/* = */
	opCond,						/* ?: */
	opOr,						/* || */
	opAnd,						/* && */
	opBitOr,					/* | */
	opBitXor,					/* ^ */
	opBitAnd,					/* & */
	opEQ, opNE,					/* == != */
	opLT, opLE, opGT, opGE,				/* < <= > >= */
	opLsh, opRsh,					/* << >> */
	opAdd, opSub,					/* + - */
	opMul, opDiv, opMod,				/* * / % */
	opDeref, opAddrOf,				/* * & */
	opNot, opBitNot, opNeg, opSizeOf, opCast,	/* ! ~ - sizeof */
	opIndex, opMember, opArrow, opCall,		/* [] . -> () */
	opName, opNumber, opAddress, opString,
	opDataStream, opProtoStack,			/* $ds, $ps */
	opStackRef, opProtoId, opSlinkRef,
	opBitSeek, opBitRewind
};

extern char	*opnames[];

typedef struct treenode {
	enum operator		tn_op;		/* operator code */
	unsigned short		tn_refcnt;	/* pointer reference count */
	unsigned short		tn_flags;	/* flags, see below */
	typeword_t		tn_type;	/* type of this node */
	unsigned short		tn_arity;	/* number of kids */
	unsigned short		tn_lineno;	/* defining line number */
	char			*tn_module;	/* and module name */
	struct treenode		*tn_next;	/* next node if in a list */
	short			tn_width;	/* bit size, < 0 if variable */
	unsigned short		tn_bitoff;	/* bit offset in frame */
	struct treenode		*tn_guard;	/* guarded member condition */
	union {
	    struct {
		typeword_t	decltype;	/* original declared type */
		struct treenode	*overload;	/* overloaded declarations */
		char		*suffix;	/* type suffix if overloaded */
	    } decl;
	    struct {
		typeword_t	enumtype;	/* enum representative type */
		unsigned int	maxnamlen;	/* max enumerator name length */
	    } enumrep;
	    Symbol		*elemsym;	/* element descriptor symbol */
	} tn_var;
	union {
	    struct {
		struct treenode	*kid3;		/* 3rd kid if ternary */
		struct treenode	*kid2;		/* 2nd kid or left if binary */
		struct treenode	*kid1;		/* 1st kid, right if binary */
	    } kids;
	    struct {
		Symbol		*sym;		/* macro name, overlays kid3 */
		Symbol		*def;		/* and definition */
	    } mac;
	    struct {
		Scope		*inner;		/* scope of scope reference */
		int		depth;		/* no. of frames to target */
	    } sref;
	    struct pragma	*pragma;	/* pragma from source file */
	    Symbol		*sym;		/* symbol, overlays kid3 */
	    long		val;		/* number value over kid3 */
	    Address		addr;		/* address value */
	} tn_data;
} TreeNode;

#define	tn_decltype	tn_var.decl.decltype
#define	tn_overload	tn_var.decl.overload
#define	tn_suffix	tn_var.decl.suffix
#define	tn_enumtype	tn_var.enumrep.enumtype
#define	tn_maxnamlen	tn_var.enumrep.maxnamlen
#define	tn_elemsym	tn_var.elemsym

#define	tn_kid3		tn_data.kids.kid3
#define	tn_kid2		tn_data.kids.kid2
#define	tn_kid1		tn_data.kids.kid1
#define	tn_left		tn_data.kids.kid2
#define	tn_right	tn_data.kids.kid1
#define	tn_kid		tn_data.kids.kid1
#define	tn_name		tn_data.mac.name
#define	tn_def		tn_data.mac.def
#define	tn_inner	tn_data.sref.inner
#define	tn_depth	tn_data.sref.depth
#define	tn_pragma	tn_data.pragma
#define	tn_sym		tn_data.sym
#define	tn_val		tn_data.val
#define	tn_addr		tn_data.addr

/* treenode flags */
#define	tfLocalRef	0x0001		/* name refers to field in same bb */
#define	tfBackwardRef	0x0002		/* name refers to predecessor field */
#define	tfForwardRef	0x0004		/* name refers to successor field */
#define	tfEndOfBlock	0x0008		/* last node in basic block list */
#define	tfRecursive	0x0010		/* recursive struct field */
#define	tfExpected	0x0020		/* node is expected value of field */

typedef struct scopenode {
	TreeNode	sn_node;	/* base class state */
	Scope		sn_scope;	/* this node's symbol table */
} ScopeNode;

#define	SN(tn)	((ScopeNode *) (tn))

enum severity { WARNING, ERROR, INTERNAL };

extern TreeNode	*treenode(enum operator, int);
extern ScopeNode *scopenode(enum operator, int, Symbol *);
extern void	tnerror(TreeNode *, enum severity, char *, ...);
extern TreeNode	*list(TreeNode *);
extern TreeNode	*first(TreeNode *);
extern void	destroyfirst(TreeNode **);
extern TreeNode	*append(TreeNode *, TreeNode *);
extern TreeNode	*openprotocol(enum datarep, TreeNode *, TreeNode *, TreeNode *);
extern TreeNode	*closeprotocol(TreeNode *, TreeNode *, TreeNode *);
extern TreeNode *base(Symbol *, TreeNode *);
extern TreeNode	*declaration(Symbol *, char *, enum symtype);
extern void	array(TreeNode *, enum typequal, TreeNode *);
extern void	bitfield(TreeNode *, TreeNode *);
extern TreeNode	*constdefs(typeword_t, TreeNode *);
extern TreeNode	*enumdef(enum operator, enum datarep, Symbol *, TreeNode *);
extern TreeNode	*openstruct(enum datarep, Symbol *);
extern TreeNode	*closestruct(TreeNode *, TreeNode *);
extern TreeNode	*block(Symbol *, TreeNode *);
extern TreeNode	*ternary(enum operator, TreeNode *, TreeNode *, TreeNode *);
extern TreeNode	*binary(enum operator, TreeNode *, TreeNode *);
extern TreeNode	*unary(enum operator, Symbol *, TreeNode *);
extern TreeNode	*macro(Symbol *, Symbol *);
extern TreeNode	*pragma(struct pragma *);
extern TreeNode	*name(Symbol *);
extern TreeNode	*number(long);
extern TreeNode	*address(Address);
extern TreeNode	*string(Symbol *);
extern TreeNode	*scoperef(enum operator, Scope *, int);
extern TreeNode	*bitseek(int);
extern void	walktree(TreeNode *, int (*)(TreeNode *, void *), void *);
extern TreeNode	*copytree(TreeNode *);
extern TreeNode	*_destroytree(TreeNode *);

#define	holdnode(tn)	((tn)->tn_refcnt++, (tn))
#define	destroytree(tn)	((void) _destroytree(tn))

extern int	eval(TreeNode *, typeword_t);
extern int	convert(TreeNode *, typeword_t);
extern int	test(TreeNode *);
extern int	operate(TreeNode *, enum operator, TreeNode *, TreeNode *,
			TreeNode *);

/*
 * Typeword for default conversions.
 */
extern typeword_t niltw;

#endif
