#ifndef GENERATE_H
#define GENERATE_H
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Code generator declarations.
 */
#include "type.h"

struct treenode;
struct context;

#define	cgfFetch	0x01
#define	cgfDecode	0x02
#define	cgfSizeOf	0x04
#define	cgfOffset	0x08
#define	cgfLookahead	0x10

#define	CG_NAMESIZE	128

struct codegenerator;
typedef void (*cgfun_t)(struct codegenerator *, struct treenode *);

typedef struct codegenerator {
	enum datarep	cg_datarep;
	char		*cg_drepname;
	unsigned int	cg_flags;	/* XXX */
	void		(*cg_sizeof)(struct treenode *);
	void		(*cg_offset)(struct treenode *);
	cgfun_t		cg_fetcharray;
	cgfun_t		cg_fetch[btMax];
	cgfun_t		cg_decodearray;
	cgfun_t		cg_decode[btMax];
	char		cg_member[CG_NAMESIZE];
	char		cg_cookie[CG_NAMESIZE];
} CodeGenerator;

extern CodeGenerator	*cgsw[];

extern void	indent(int);
extern void	generate(struct treenode *, struct context *, char *);
extern void	generateCtype(typeword_t, unsigned short, FILE *);
extern void	generateCexpr(struct treenode *);

extern int	level;		/* current indentation level */
extern FILE	*cfile;		/* open .c output file */
extern FILE	*hfile;		/* null or .h exports file */

#endif
