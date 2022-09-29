/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_PROCSET_H
#define _SYS_PROCSET_H
#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.11 $"

#include <sys/types.h>

/*	This file defines the data needed to specify a set of
**	processes.  These types are used by the sigsend, sigsendset,
**	priocntl, priocntlset, waitid, evexit, and evexitset system
**	calls.
*/

#define	P_INITPID	1
#define	P_INITUID	0
#define	P_INITPGID	0


/*	The following defines the values for an identifier type.  It
**	specifies the interpretation of an id value.  An idtype and
**	id together define a simple set of processes.
*/

#if _SGIAPI
typedef enum idtype {
#else
typedef enum {
#endif /* _SGIAPI */
	P_PID,		/* A process identifier.		*/
	P_PPID,		/* A parent process identifier.		*/
	P_PGID,		/* A process group (job control group)	*/
			/* identifier.				*/
	P_SID,		/* A session identifier.		*/
	P_CID,		/* A scheduling class identifier.	*/
	P_UID,		/* A user identifier.			*/
	P_GID,		/* A group identifier.			*/
	P_ALL		/* All processes.			*/
} idtype_t;


#if _SGIAPI || _ABIAPI
/*	The following defines the operations which can be performed to
 *	combine two simple sets of processes to form another set of
 *	processes.
 */
typedef enum idop {
	POP_DIFF,	/* Set difference.  The processes which	*/
			/* are in the left operand set and not	*/
			/* in the right operand set.		*/
	POP_AND,	/* Set disjunction.  The processes	*/
			/* which are in both the left and right	*/
			/* operand sets.			*/
	POP_OR,		/* Set conjunction.  The processes	*/
			/* which are in either the left or the	*/
			/* right operand sets (or both).	*/
	POP_XOR		/* Set exclusive or.  The processes 	*/
			/* which are in either the left or	*/
			/* right operand sets but not in both.	*/
} idop_t;

/*	The following structure is used to define a set of processes.
 *	The set is defined in terms of two simple sets of processes
 *	and an operator which operates on these two operand sets.
 */
typedef struct procset {
	idop_t		p_op;	/* The operator connection the	*/
				/* following two operands each	*/
				/* of which is a simple set of	*/
				/* processes.			*/

	idtype_t	p_lidtype;
				/* The type of the left operand	*/
				/* simple set.			*/
	id_t		p_lid;	/* The id of the left operand.	*/

	idtype_t	p_ridtype;
				/* The type of the right	*/
				/* operand simple set.		*/
	id_t		p_rid;	/* The id of the right operand.	*/
} procset_t;

/*	The following macro can be used to initialize a procset_t
 *	structure.
 */

#define	setprocset(psp, op, ltype, lid, rtype, rid) \
			(psp)->p_op		= (op); \
			(psp)->p_lidtype	= (ltype); \
			(psp)->p_lid		= (lid); \
			(psp)->p_ridtype	= (rtype); \
			(psp)->p_rid		= (rid);

#endif /* _SGIAPI || _ABIAPI */

#ifdef _KERNEL
struct vproc;
struct vp_get_attr_s;
extern int	checkprocset(procset_t *);
extern id_t	getmyid(idtype_t, struct vp_get_attr_s *);
extern int procinset(struct vproc *, procset_t *);
extern boolean_t cur_inset_only(procset_t *);
#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_PROCSET_H */
