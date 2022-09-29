/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_EXEC_H
#define _SYS_EXEC_H

#ident "$Revision: 1.45 $"

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/uio.h>
#include <ksys/as.h>
#include <sys/sat.h>
#include <elf.h>

#define MAGIC_OFFSET	((off_t) 0)	/* offset of magic number first byte */
#define MAGIC_SIZE	((off_t) 2)	/* length of magic number (in bytes) */
#define MAX_PREFIX_ARGS	4		/* max number of prefix args plus NULL 
					   pointer */
#define	INTPSZ	256			/* max size of '#!' line allowed */

struct intpdata {			/* holds interpreter name & argument 						   data */

	char	*line1p;		/* points to dyn. alloc. buf of size 
					   INTPSZ */
	int	line1p_sz;		/* size of line1p */
	char	*intp_name;		/* points to name part */
	char	*intp_arg;		/* points to arg part */
};

#if ELF64
#define	Elf_Ehdr	Elf64_Ehdr
#else
#define	Elf_Ehdr	Elf32_Ehdr
#endif

/*
 * User argument structure for stack image management
 */

struct uarg {
	int		ua_auxsize;	/* size of aux arguments in bytes */
	caddr_t		ua_stackend;	/* kernel addr in user stack where
					 * aux args go */
	uvaddr_t	ua_stackaddr;	/* user stack address */
	char		**ua_argp;	/* user addr of exec arg pointers */
	char		**ua_envp;	/* user addr of exec env pointers */
	char		*ua_fname;	/* exec a.out file name */
	uio_seg_t	ua_fnameseg;	/* segment for fname (user or kernel) */
	int		ua_traceinval;	/* flag to cause /proc vnode
					 * invalidation */
	int		ua_abiversion;	/* used to select system call handler */
	int		ua_ncargs;	/* # args permitted */

	char		ua_exec_file[PSCOMSIZ];	/* iexec state */
	struct vnode	*ua_exec_vp;	/* iexec state */

	int		ua_level;	/* gexec recursion state */
	uid_t		ua_uid;		/* UID for SUID/SGID executables */
	gid_t		ua_gid;		/* GID for SUID/SGID executables */
	cap_set_t	ua_cap;		/* capabilities of executable */
	int		ua_setid;	/* true if SUID, SGID or SCAP */
	struct vnode	*ua_prev_script[2]; /* gexec state */

	char		*ua_prefix_argv[MAX_PREFIX_ARGS]; /* intp: prefix
							   * args if present */
	struct intpdata	ua_idata;	/* intp state */
	caddr_t		*ua_prefixp;	/* intp: prefix args if present */
	struct vnode	*ua_intpvp;	/* intp: vnode of script file */
	char		ua_intpfname[16];	/* intp state data */
	int		ua_prefixc;	/* intp: # of args to interpreter */
	struct vnode	*ua_intp_vp;	/* intp state */
	char		*ua_intpstkloc;	/* intp: stack element */
	char		*ua_intppsloc;	/* intp: u_pscomm element */

	caddr_t		ua_phdrbase;	/* elf2exec state */
	int		ua_phdrbase_sz;	/* elf2exec state */
	Elf_Ehdr	*ua_ehdrp;	/* elf2exec state */
	int		ua_ehdrp_sz;	/* elf2exec state */
	caddr_t		ua_vwinadr;	/* elf2exec state */
	int		ua_size;	/* elf2exec state */
	uvaddr_t	ua_newsp;	/* elf2exec state */
	int		ua_sourceabi;	/* elf2exec state */
	struct as_exec_state_s ua_as_state;	/* elf2exec state */
	int (*ua_elf2_proc) (		/* elf2exec state */
		struct vnode *vp,
		struct uarg *args,
		caddr_t phdrbase,
		Elf_Ehdr *ehdrp,
		caddr_t vwinadr,
		int size,
		uvaddr_t newsp,
		int sourceabi);

	char ua_exec_cleanup;		/* phase 1 cleanup required if set */

#ifdef TFP_PREFETCH_WAR
	int		ua_execflags;	/* Flags */
#endif
#ifdef CKPT
	int		ua_ckpt;	/* checkpoint info for exec vode */
	int		ua_intp_ckpt;	/* intpexec state data */
#endif
	cell_t		ua_cell;	/* cell to exec on */
};

struct cred;
struct proc;
struct vattr;
struct vnode;
struct uarg;
/* definitions for execflags above */
#define	ADD_PFETCHFD		0x2	/* TFP only - TFP_PREFETCH chip bug */

extern int  elfexec(struct vnode *, struct vattr *, struct uarg *, int);
extern int  intpexec(struct vnode *, struct vattr *, struct uarg *, int);
extern int  remove_proc(struct proc *, struct uarg *, struct vnode *, int);
extern int  execmap(struct vnode *, caddr_t, size_t, size_t, off_t,
		     int, int, vasid_t, int);
extern int  gexec(struct vnode **, struct uarg *, int);
extern int  execpermissions(struct vnode *, struct vattr *, struct uarg *);
extern int  exrdhead(struct vnode *, off_t, size_t, caddr_t *);
extern void exhead_free(caddr_t);
extern int  execopen(struct vnode **, int *);
extern int  execclose(int);
extern void setexecenv(struct vnode *);
extern int  check_dmapi_file(struct vnode *);

extern int fuexarg(struct uarg *, caddr_t, uvaddr_t *, int);

/*
 * Information regarding the use of struct uarg during remote exec:
 * stackend
 * vwinaddr
 * newsp
 * intpstkloc
 * intppsloc
 * 	pointers (kernel addresses) into the kernel the user stack;
 * 	partially built during phase 1;
 * 	kmem allocd space
 * 	vwinaddr is the base address (size is the size)
 * 		must be copied 
 * 		pointers must be relocated
 * 
 * fname
 * 	pointer to either 1) file name in user space, or 2) pointer
 * 	to intp_fname (in uarg)
 * 		#2 must be relocated 
 * 
 * exec_vp
 * intp_vp
 * intpvp
 * 	vnodes
 * 		must be exported/imported 
 * 
 * prev_script
 * 	vnodes
 * 		must not be exported - only apply to the old
 * 		process
 * 
 * execrec
 * execpn
 * 	security stuff
 * 		need to be migrated 
 * 
 * prefix_argv
 * 	pointers into uarg (idata)
 * 		need to be relocated 
 * 
 * idata
 * 	first line of intp file;
 * 	kmem allocd space + pointers to it
 * 		kmem space must be copied
 * 		pointers must be relocated
 * 
 * prefixp
 * 	pointer to prefix_argv or null
 * 		must be relocated
 * 
 * phdrbase
 * ehdrp
 * 	elf header info
 * 	kmem allocd space
 * 		kmem space must be copied
 * 
 */

#endif /* _SYS_EXEC_H */
