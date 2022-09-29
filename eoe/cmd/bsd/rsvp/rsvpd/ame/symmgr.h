/*sccs************************************************************************
 *sccs Audit trail generated for Peer sccs tracking
 *sccs module symmgr.h, release 1.4 dated 96/02/12
 *sccs retrieved from /home/peer+bmc/ame/common/include/src/SCCS/s.symmgr.h
 *sccs
 *sccs    1.4	96/02/12 13:21:19 randy
 *sccs	update company name (PR#613)
 *sccs
 *sccs    1.3	95/01/24 13:01:56 randy
 *sccs	update company addres (PR#183)
 *sccs
 *sccs    1.2	92/11/12 16:34:53 randy
 *sccs	Copyright PEER Networks, Inc.
 *sccs
 *sccs    1.1	92/10/23 15:28:45 randy
 *sccs	date and time created 92/10/23 15:28:45 by randy
 *sccs
 *sccs
 *sccs************************************************************************/

#ifndef SYMMGRH				/* avoid recursive re-inclusion */
#define SYMMGRH

/************************************************************************
 *                                                                      *
 *          PEER Networks, a division of BMC Software, Inc.             *
 *                   CONFIDENTIAL AND PROPRIETARY                       *
 *                                                                      *
 *	This product is the sole property of PEER Networks, a		*
 *	division of BMC Software, Inc., and is protected by U.S.	*
 *	and other copyright laws and the laws protecting trade		*
 *	secret and confidential information.				*
 *									*
 *	This product contains trade secret and confidential		*
 *	information, and its unauthorized disclosure is			*
 *	prohibited.  Authorized disclosure is subject to Section	*
 *	14 "Confidential Information" of the PEER Networks, a		*
 *	division of BMC Software, Inc., Standard Development		*
 *	License.							*
 *									*
 *	Reproduction, utilization and transfer of rights to this	*
 *	product, whether in source or binary form, is permitted		*
 *	only pursuant to a written agreement signed by an		*
 *	authorized officer of PEER Networks, a division of BMC		*
 *	Software, Inc.							*
 *									*
 *	This product is supplied to the Federal Government as		*
 *	RESTRICTED COMPUTER SOFTWARE as defined in clause		*
 *	55.227-19 of the Federal Acquisitions Regulations and as	*
 *	COMMERCIAL COMPUTER SOFTWARE as defined in Subpart		*
 *	227.401 of the Defense Federal Acquisition Regulations.		*
 *									*
 * Unpublished, Copr. PEER Networks, a division of BMC	Software, Inc.  *
 *                     All Rights Reserved				*
 *									*
 *	PEER Networks, a division of BMC Software, Inc.			*
 *	1190 Saratoga Avenue, Suite 130					*
 *	San Jose, CA 95129-3433 USA					*
 *									*
 ************************************************************************/


/*
 *	This header file has the function prototypes for the public
 *	functions defined in symmgr.c
 */
#include "ame/machtype.h"
#include "ame/smitypes.h"

#ifndef USE_PROTOTYPES

/*
 *	bsym_free  -  get rid of a symbol table
 */
void	bsym_free();

/*
 *	bsym_lookup - search a symbol table for a symbol
 */
int	bsym_lookup();

/*
 *	bsym_delete - remove a table entry
 */
int	bsym_delete();

/*
 *	bsym_apply  -  apply a function to all entries of table
 */
int	bsym_apply();

/*
 *	bsym_define  -	define a new symbol in a symbol table
 *		       expand table if necessary
 */
int	bsym_define();

/*
 *	bsym_alloc  -  create a global symbol table
 */
Void	*bsym_alloc();

#else

/*
 *	bsym_free  -  get rid of a symbol table
 */
void	bsym_free(Void *id);

/*
 *	bsym_lookup - search a symbol table for a symbol
 */
int	bsym_lookup(Void		*id,
		    ubyte		*symbol,
		    Len_t		sym_sz,
		    unsigned long	*vptr,
		    Void		**aptr);

/*
 *	bsym_delete - remove a table entry
 */
int	bsym_delete(Void		*id,
		    ubyte		*symbol,
		    Len_t		sym_sz);

/*
 *	bsym_apply  -  apply a function to all entries of table
 */
int	bsym_apply(Void		*id,
		   int		(*func) (Void		*table,
					 ubyte		*name,
					 Len_t		name_len,
					 unsigned long	*vptr,
					 Void		**aptr,
					 Void		*parm),
		   Void		*parm);

/*
 *	bsym_define  -	define a new symbol in a symbol table
 *		       expand table if necessary
 */
int	bsym_define(Void			*id,
		    ubyte			*symbol,
		    Len_t			sym_sz,
		    unsigned long		value,
		    Void			*application);

/*
 *	bsym_alloc  -  create a global symbol table
 */
Void	*bsym_alloc(void);

#endif

#endif
