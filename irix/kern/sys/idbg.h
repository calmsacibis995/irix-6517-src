#ident "$Revision: 3.27 $"

#ifndef _SYS_IDBG_H
#define _SYS_IDBG_H

/*
 * The kernel writes out an array of these structures to dbgmon memory
 * at startup, so that dbgmon knows where to find various structures.
 * Remember to change stand/symmon/idbg.h if this file is changed
 */
typedef struct {
	int	s_type;
	union {
		struct dbgstruct	*s_h;
		void			(*s_f1)(void *, void *);
		__psint_t		s_gpa;
	} un;
# define s_func		un.s_f1
# define s_gp		un.s_gpa
# define s_head		un.s_h
	union {
		char	*k_n;
		char	u_n[12];
	} name_un;
#define ks_name		name_un.k_n
#define us_name		name_un.u_n
} dbgoff_t;


typedef struct dbgstruct {
	dbgoff_t		dp;
	struct dbgstruct	*next;
} dbglist_t;

# define	DO_ENV		0x45454545	/* first record */
# define	DO_END		0x54545454	/* final record */

struct dbstbl {
	__psunsigned_t addr;
	__uint32_t noffst;
};

#if defined(_KERNEL) && !defined(_STANDALONE)
/*
 * WARNING: Following definition must match that in stand/arcs/symmon/dbgmon.h
 *
 * Also, we don't want this defined for commands (cmd/idbg for example)
 */
#define NBREAKPOINTS	32
struct brkpt_table {
	__psunsigned_t bt_addr;		/* address where brkpt is installed */
	cpumask_t bt_cpu;		/* for which cpu(s) - bit mask */
	unsigned bt_inst;		/* previous instruction */
	int bt_type;			/* breakpoint type */
	int bt_oldtype;			/* type before being suspended */
};

/* stole from symmon/brkpt.c */
#define	BTTYPE_EMPTY	0		/* unused entry */
#define	BTTYPE_SUSPEND	1		/* suspended breakpoint */
#define	BTTYPE_CONT	2		/* continue breakpoint */
#define	BTTYPE_TEMP	3		/* temporary breakpoint */
#endif

/*
 * structure for commmunication between user idbg and internal
 */
struct idbgcomm {
	__psint_t i_func;	/* function index into dbgoff_t table */
	__psint_t i_arg;	/* 1st arg */
	caddr_t	i_argp;		/* pointer to i_argcnt pairs of addr,len */
	int	i_argcnt;	/* addr/len pairs */
	caddr_t i_uaddr;	/* where to put output */
	unsigned i_ulen;	/* length of user's buffer */
	int	i_cause;	/* set to cause reg if error else 0 */
	__psunsigned_t i_badvaddr; /* where address error was if got one */
};

#if defined(_KERNEL) && !defined(_STANDALONE)
/* 
 * We really don't want to include <stdarg.h> here
 * since stdarg.h is part of compiler_dev.
 * Thus define what we need ...
 */

/* Define the va_list type: */
#ifndef _VA_LIST_
#define _VA_LIST_
typedef char *va_list;
#endif /* !_VA_LIST_ */

union rval;
struct mrlock_s;
struct vfs;
struct vfsops;
struct vnode;
struct vnodeops;
#include <sys/vnode.h>

typedef struct idbgfssw {
	struct idbgfssw	*next;
	char		*name;
	struct vnodeops	*vnops;
	struct vfsops	*vfsops;
	void		(*vfs_data_print)(void *);
	void		(*vfs_vnodes_print)(struct vfs *, int);
	void		(*vnode_data_print)(void *);
	struct vnode *	(*vnode_find)(struct vfs *, vnumber_t);
} idbgfssw_t;

extern idbgfssw_t	*idbgfssws;

typedef struct idbgfunc {
	int  *stubs;
	void (*setup)(void);
	int  (*copytab)(caddr_t *, int, union rval *);
	void (*tablesize)(union rval *);
	int  (*dofunc)(struct idbgcomm *, union rval *);
	int  (*error)(void);
	void (*iswitch)(int, int);
	int  (*addfunc)(char *, void (*)());
	void (*delfunc)(void (*)());
	void (*qprintf)(char *, va_list);
	void (*prvn)(struct vnode *, int);
	void (*wd93)(__psint_t);
	void (*late_setup)(void);
	int  (*unload)(void);
	void (*addfssw)(idbgfssw_t *);
	void (*delfssw)(idbgfssw_t *);
	void (*printflags)(uint64_t, char **, char *);
	char*(*padstr)(char *, int);
	void (*pmrlock)(struct mrlock_s *);
	void (*prsymoff)(void *, char *, char *);
} idbgfunc_t;

extern idbgfunc_t *idbgfuncs;
extern idbgfunc_t idbgdefault;

#endif	/* _KERNEL && !_STANDALONE */

#endif	/* _SYS_IDBG_H */
