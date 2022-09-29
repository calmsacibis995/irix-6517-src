#ifndef PROTOCOL_H
#define PROTOCOL_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Protocol interface.
 */
#include "datastream.h"
#include "expr.h"
#include "scope.h"

struct snooper;
struct snooppacket;
struct snoopfilter;
struct protostack;

typedef struct protocol {
/* mandatory statically initialized state */
	char			*pr_name;		/* string name */
	int			pr_namlen;		/* and length */
	char			*pr_title;		/* verbose name */
	int			pr_id;			/* fast identifier */
	enum dsbyteorder	pr_byteorder;		/* byte order */
	int			pr_maxhdrlen;		/* frame header size */
	struct protofield	*pr_discriminant;	/* nested type field */
/* protocol operation pointers */
	int			(*prop_init)();		/* (re)initialize */
	int			(*prop_setopt)();	/* set option */
	void			(*prop_embed)();	/* embed in protocol */
	Expr			*(*prop_resolve)();	/* resolve string */
	enum exprtype		(*prop_compile)();	/* compile raw filter */
	int			(*prop_match)();	/* match nested proto */
	int			(*prop_fetch)();	/* fetch field */
	void			(*prop_decode)();	/* decode frame */
/* optional statically initialized state */
	unsigned short		pr_flags;		/* optional flags */
	unsigned short		pr_numoptions;		/* number of options */
	struct protoptdesc	*pr_optdesc;		/* option descriptors */
	int			(*pr_xdrstate)();	/* xdr decode state */
/* automatically initialized state */
	struct scope		pr_scope;		/* symbol table */
	int			pr_level;		/* decoding level */
} Protocol;

/*
 * Initialized declaration macro for a protocol interface object.
 * Tag is the prefix of the interface structure and of its operations.
 * Name must be a string constant or a sized char array.
 */
#define	DefineProtocol(tag, name, title, id, order, maxhdrlen, discrim,	\
		       flags, optdesc, numopts, xdrstate)		\
int	 makeident2(tag,_init)();					\
int	 makeident2(tag,_setopt)(int, char *);				\
void	 makeident2(tag,_embed)(Protocol *, long, long);		\
Expr	 *makeident2(tag,_resolve)(char *, int, struct snooper *);	\
ExprType makeident2(tag,_compile)(struct protofield *, Expr *, Expr *,	\
				  struct protocompiler *);		\
int	 makeident2(tag,_match)(Expr *, DataStream *,			\
				struct protostack *, Expr *);		\
int	 makeident2(tag,_fetch)(struct protofield *, DataStream *,	\
				struct protostack *, Expr *);		\
void	 makeident2(tag,_decode)(DataStream *, struct protostack *,	\
				 struct packetview *);			\
Protocol makeident2(tag,_proto) = {					\
	name, constrlen(name), title, id, order, maxhdrlen, discrim,	\
	makeident2(tag,_init),						\
	makeident2(tag,_setopt),					\
	makeident2(tag,_embed),						\
	makeident2(tag,_resolve),					\
	makeident2(tag,_compile),					\
	makeident2(tag,_match),						\
	makeident2(tag,_fetch),						\
	makeident2(tag,_decode),					\
	flags, numopts, optdesc, xdrstate,				\
}

/*
 * Shorter macros to define protocols.
 */
#define	DefineBranchProtocol(tag,name,title,id,order,maxhdrlen,discrim) \
	DefineProtocol(tag,name,title,id,order,maxhdrlen,discrim,0,0,0,0)

#define	DefineLeafProtocol(tag,name,title,id,order,maxhdrlen) \
	DefineProtocol(tag,name,title,id,order,maxhdrlen,0,0,0,0,0)

/*
 * Macros to call protocol operations.
 */
#define	pr_init(pr)			((*(pr)->prop_init)())
#define	pr_setopt(pr, id, val)		((*(pr)->prop_setopt)(id, val))
#define	pr_embed(pr, npr, type, qual)	((*(pr)->prop_embed)(npr, type, qual))
#define	pr_resolve(pr, str, len, sn)	((*(pr)->prop_resolve)(str, len, sn))
#define	pr_compile(pr,pf,mex,tex,pc)	((*(pr)->prop_compile)(pf,mex,tex,pc))
#define	pr_match(pr, pex, ds, ps, rex)	((*(pr)->prop_match)(pex, ds, ps, rex))
#define	pr_fetch(pr, pf, ds, ps, rex)	((*(pr)->prop_fetch)(pf, ds, ps, rex))
#define	pr_decode(pr, ds, ps, pv)	((*(pr)->prop_decode)(ds, ps, pv))

/*
 * Protocol flags used by generic routines to control operation usage.
 */
#define	PR_COMPILETEST	0x0001		/* compile all test expressions */
#define	PR_EMBEDCACHED	0x0002		/* embed transiently-nested protocol */
#define	PR_MATCHNOTIFY	0x0004		/* notify protocol when matched */
#define	PR_DECODESTALE	0x0008		/* decode stale nested protocol */

/*
 * Protocol scope operations.
 */
#define	pr_lookupsym(pr, name, namlen) \
	sc_lookupsym(&(pr)->pr_scope, name, namlen)
#define	pr_addsym(pr, name, namlen, type) \
	sc_addsym(&(pr)->pr_scope, name, namlen, type)
#define	pr_deletesym(pr, name, namlen) \
	sc_deletesym(&(pr)->pr_scope, name, namlen)
#define	pr_addaddress(pr, name, namlen, addr, addrlen) \
	sc_addaddress(&(pr)->pr_scope, name, namlen, addr, addrlen)
#define	pr_addnumber(pr, name, namlen, val) \
	sc_addnumber(&(pr)->pr_scope, name, namlen, val)
#define	pr_addmacro(pr, name, namlen, def, deflen, src) \
	sc_addmacro(&(pr)->pr_scope, name, namlen, def, deflen, src)
#define	pr_addfunction(pr, name, namlen, func, nargs, desc) \
	sc_addfunction(&(pr)->pr_scope, name, namlen, func, nargs, desc)

void	sc_addaddress(Scope *, char *, int, char *, int);
void	sc_addnumber(Scope *, char *, int, long);
void	sc_addmacro(Scope *, char *, int, char *, int, ExprSource *);
void	sc_addfunction(Scope *, char *, int, int (*)(), int, char *);

/*
 * Generic protocol operations.  Initprotocols raises an exception if
 * any protocol module cannot be initialized.  Possible errors include
 * misordered or cyclic protocol dependencies, and invalid protocol
 * field descriptors.
 */
struct protomacro;

void		initprotocols(void);
Protocol	*findprotobyname(char *, int);
Protocol	*findprotobyid(unsigned int);
int		pr_register(Protocol *, struct protofield *, int, int);
int		pr_nest(Protocol *, int, long, struct protomacro *, int);
int		pr_nestqual(Protocol *, int, long, long, struct protomacro *,
			    int);
int		pr_unnest(Protocol *, int, long);
int		pr_unnestqual(Protocol *, int, long, long);
int		pr_test(Protocol *, Expr *, struct snooppacket *, int,
			struct snooper *);
int		pr_eval(Protocol *, Expr *, struct snooppacket *, int,
			struct snooper *, ExprError *, Expr *);

/*
 * This function is called by various protocols' match operations to skip
 * the current (matched) component in a protocol path expression and then
 * to evaluate the remaining components.
 */
int		ex_match(Expr *, DataStream *, struct protostack *, Expr *);

/*
 * Macros to call protocol and scope operations with constant string names.
 */
#define	findprotobyconstname(name) \
	findprotobyname(name, constrlen(name))
#define	pr_addconstaddress(pr, name, addr, addrlen) \
	pr_addaddress(pr, name, constrlen(name), addr, addrlen)
#define	pr_addconstnumber(pr, name, val) \
	pr_addnumber(pr, name, constrlen(name), val)
#define	pr_addconstmacro(pr, name, def) \
	pr_addmacro(pr, name, constrlen(name), def, constrlen(def), 0)
#define	pr_addconstfunction(pr, name, func, nargs, desc) \
	pr_addfunction(pr, name, constrlen(name), func, nargs, desc)

/*
 * Protocol field descriptor, passed in a vector to pr_register and added
 * to the registered protocol's scope.  If pf_size is PF_VARIABLE, then
 * pf_off contains a protocol-specific cookie.  If pf_size is negative,
 * its 2's-complement is the number of bits in the field, and pf_off is
 * the bit offset from protocol origin.
 *
 * If a fixed (bit or byte) protofield passed to pr_register has a negative
 * pf_off, pr_register will compute its offset by summing prior field sizes
 * with the last field with a non-negative offset, or with zero if all prior
 * fields in the vector had negative offsets.  Bit fields must sum to a
 * byte-congruent size and offset.
 */
#define	PF_VARIABLE	0

typedef struct protofield {
	char			*pf_name;	/* field name string */
	int			pf_namlen;	/* name length */
	char			*pf_title;	/* lengthy name string */
	int			pf_id;		/* field identifier code */
	void			*pf_data;	/* type specific data */
	int			pf_size;	/* bitsize if <0, byte if >0 */
	int			pf_off;		/* offset in bits or bytes */
	enum exop		pf_type;	/* expression type */
	int			pf_level;	/* viewing detail level */
} ProtoField;

#define	pf_cookie		pf_off
#define	pf_extendhow(pf)	((enum dsextendhow)(unsigned long)(pf)->pf_data)
#define	pf_element(pf)		((ProtoField *) (pf)->pf_data)
#define	pf_struct(pf)		((ProtoStruct *) (pf)->pf_data)

/*
 * Static initializer macros for variable, byte, bit, and int-typed fields.
 * Names must be string constants or char arrays of known size.
 *	PFI_VAR			variable field
 *	PFI_BYTE		fixed-length byte string field
 *	PFI_ADDR		1- to 8-byte address field
 *	PFI_SBIT, PFI_UBIT	signed/unsigned bit field
 *	PFI_SINT, PFI_UINT	signed/unsigned int field
 *	PFI_ARRAY, PFI_STRUCT	array or structure field
 */
#define	PFI(name, title, id, data, size, off, type, level) \
	{ name, constrlen(name), title, (int) id, (void *) data, size, off, \
	  type, level }

#define	PFI_VAR(name, title, id, cookie, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, PF_VARIABLE, cookie, EXOP_STRING, \
	    level)
#define	PFI_BYTE(name, title, id, size, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, size, -1, EXOP_STRING, level)
#define	PFI_ADDR(name, title, id, size, level) \
	PFI(name, title, id, DS_ZERO_EXTEND, size, -1, EXOP_ADDRESS, level)

#define	PFI_BIT(name, title, id, extendhow, bitsize, level) \
	PFI(name, title, id, extendhow, -(bitsize), -1, EXOP_NUMBER, level)
#define	PFI_SBIT(name, title, id, bitsize, level) \
	PFI_BIT(name, title, id, DS_SIGN_EXTEND, bitsize, level)
#define	PFI_UBIT(name, title, id, bitsize, level) \
	PFI_BIT(name, title, id, DS_ZERO_EXTEND, bitsize, level)

#define	PFI_INT(name, title, id, extendhow, type, level) \
	PFI(name, title, id, extendhow, sizeof(type), -1, EXOP_NUMBER, level)
#define	PFI_SINT(name, title, id, type, level) \
	PFI_INT(name, title, id, DS_SIGN_EXTEND, type, level)
#define	PFI_UINT(name, title, id, type, level) \
	PFI_INT(name, title, id, DS_ZERO_EXTEND, type, level)

#define	PFI_ARRAY(name, title, id, type, dim, level) \
	PFI(name, title, id, type, dim, -1, EXOP_ARRAY, level)
#define	PFI_STRUCT(name, title, id, struct, level) \
	PFI(name, title, id, struct, 0, -1, EXOP_STRUCT, level)

#define	pr_findfield(pr, pfid)	sc_findfield(&(pr)->pr_scope, pfid)

ProtoField	*sc_findfield(Scope *, int);	/* XXXbe */

/*
 * Protocol structure descriptor.
 */
typedef struct protostruct {
/* statically initialized members */
	char		*pst_name;	/* struct tag */
	ProtoField	*pst_fields;	/* array of member fields */
	int		pst_numfields;	/* number of members */
/* dynamically initialized members */
	ProtoField	*pst_parent;	/* this struct's field */
	Scope		pst_scope;	/* symbol table */
} ProtoStruct;

#define	PSI(name, fields)	{ name, fields, lengthof(fields), }

/*
 * Protocol option descriptor.  Those protocols that implement pr_setopt
 * should define an initialized vector of descriptors and pass it to their
 * DefineProtocol calls.
 */
typedef struct protoptdesc {
	char	*pod_name;	/* option name */
	int	pod_id;		/* identification code */
	char	*pod_desc;	/* brief description */
} ProtOptDesc;

#define	POD(name, id, desc)	{ name, (int) id, desc }

ProtOptDesc	*pr_findoptdesc(Protocol *, const char *);

/*
 * Protocol macro, passed in a vector to pr_nest and defined in the
 * encapsulating protocol's scope.
 */
typedef struct protomacro {
	char	*pm_name;	/* protocol macro name */
	int	pm_namlen;	/* name string length */
	char	*pm_def;	/* and defined value */
	int	pm_deflen;	/* def string length */
} ProtoMacro;

#define	PMI(name, def)	{ name, constrlen(name), def, constrlen(def) }

#define	pr_addmacros(pr, pm, npm) \
	sc_addmacros(&(pr)->pr_scope, pm, npm)

void	sc_addmacros(Scope *, ProtoMacro *, int);

/*
 * Protocol function descriptor, for multiple function definition driven by
 * a list of descriptors.  If the function takes arguments, pfd_desc should
 * end with an '@' followed by a comma-separated formal argument name list.
 */
typedef struct protofuncdesc {
	char	*pfd_name;	/* function name */
	int	pfd_namlen;	/* name string length */
	int	(*pfd_func)();	/* function pointer */
	int	pfd_nargs;	/* number of arguments */
	char	*pfd_desc;	/* brief description */
} ProtoFuncDesc;

#define	PFD(name, func, nargs, desc) \
	{ name, constrlen(name), func, nargs, desc }

#define	pr_addfunctions(pr, pfd, npfd) \
	sc_addfunctions(&(pr)->pr_scope, pfd, npfd)

void	sc_addfunctions(Scope *, ProtoFuncDesc *, int);

/*
 * Snoop filter compilation uses two datastreams, to encode mask bits
 * and field values to match.
 */
typedef struct protocompiler {
	unsigned short	*pc_errflags;	/* pointer to snoop error flags */
	DataStream	pc_mask;	/* encoder for snoopfilter mask */
	DataStream	pc_match;	/* encoder for fields to match */
	ExprError	*pc_error;	/* compiler error report */
} ProtoCompiler;

#define	PC_ALLINTBITS	(-1L)		/* mask to match all bits in any int */
#define	PC_ALLBYTES	((char *) 0)	/* exact mask for pc_bytes */

#define	pc_loc(pc)	ds_tell(&(pc)->pc_mask)

void			pc_init(ProtoCompiler *, struct snoopfilter *, int,
				enum dsbyteorder, unsigned short *,
				ExprError *);
void			pc_stop(ProtoCompiler *);
enum dsbyteorder	pc_setbyteorder(ProtoCompiler *, enum dsbyteorder);
int			pc_skipbytes(ProtoCompiler *, int);
int			pc_skipbits(ProtoCompiler *, int);
int			pc_byte(ProtoCompiler *, char, char);
int			pc_bytes(ProtoCompiler *, char *, char *, int);
int			pc_bits(ProtoCompiler *, long, long, int);
int			pc_short(ProtoCompiler *, short, short);
int			pc_long(ProtoCompiler *, long, long);
int			pc_int(ProtoCompiler *, long, long, int);
int			pc_intfield(ProtoCompiler *, ProtoField *, long,
				    long, int);
int			pc_bytefield(ProtoCompiler *, ProtoField *, char *,
				     char *, int);
int			pc_intmask(ProtoCompiler *, Expr *, long *);
void			pc_error(ProtoCompiler *, Expr *, char *);
void			pc_badop(ProtoCompiler *, Expr *);

/*
 * Default or no-op stubs for use by top-layer and simple protocols.
 *	setopt, resolve, and match return 0
 *	embed does nothing
 *	compile returns ET_COMPLEX, to halt snoop filter compilation
 *	fetch calls ds_field
 */
int	  pr_stub_setopt(int, char *);
void	  pr_stub_embed(Protocol *, long, long);
Expr	  *pr_stub_resolve(char *, int, struct snooper *);
ExprType  pr_stub_compile(ProtoField *, Expr *, Expr *, ProtoCompiler *);
int	  pr_stub_match(Expr *, DataStream *, struct protostack *, Expr *);
int	  pr_stub_fetch(ProtoField *, DataStream *, struct protostack *,
			Expr *);

#endif
