/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_cell.c,v 65.5 1998/03/23 16:23:56 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#include <dcedfs/osi_cred.h>
#include <dcedfs/afsvl_proc.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_cell.h>
#include <cm_conn.h>
#include <cm_site.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_cell.c,v 65.5 1998/03/23 16:23:56 gwehrman Exp $")

/*
 * Cell module related globals
 */
struct cm_cell *cm_cells = 0;		/* Root of cells global linked list */
struct lock_data cm_celllock;		/* allocation lock for cells */
#ifdef SGIMIPS
static short cm_cellindex = 0;		/* Relative index cell id */
#else  /* SGIMIPS */
static long cm_cellindex = 0;		/* Relative index cell id */
#endif /* SGIMIPS */


/*
 * Find a cell by its name
 */
#ifdef SGIMIPS
struct cm_cell *cm_GetCellByName(register char *cellNamep)
#else  /* SGIMIPS */
struct cm_cell *cm_GetCellByName(cellNamep)
    register char *cellNamep;
#endif /* SGIMIPS */
{
    register struct cm_cell *cellp;

    lock_ObtainRead(&cm_celllock);
    for (cellp = cm_cells; cellp; cellp = cellp->next) {
	if (!strcmp(cellp->cellNamep, cellNamep)
	    || (cellp->oldCellNamep && !strcmp(cellp->oldCellNamep, cellNamep))) {
	    lock_ReleaseRead(&cm_celllock);
	    return cellp;
	}
    }
    lock_ReleaseRead(&cm_celllock);
    icl_Trace1(cm_iclSetp, CM_TRACE_GETCELLBYNAMEFAIL,
	       ICL_TYPE_STRING, cellNamep);
    return (struct cm_cell *) 0;
}


/*
 * Find a cell by its id
 */
#ifdef SGIMIPS
struct cm_cell *cm_GetCell(register afs_hyper_t *cellIdp)
#else  /* SGIMIPS */
struct cm_cell *cm_GetCell(cellIdp)
    register afs_hyper_t *cellIdp;
#endif /* SGIMIPS */
{
    register struct cm_cell *cellp;

    lock_ObtainRead(&cm_celllock);
    for (cellp = cm_cells; cellp; cellp = cellp->next) {
	if (AFS_hsame(cellp->cellId, *cellIdp)) {
	    lock_ReleaseRead(&cm_celllock);
	    return cellp;
	}
    }
    lock_ReleaseRead(&cm_celllock);
    icl_Trace1(cm_iclSetp, CM_TRACE_GETCELLFAIL, ICL_TYPE_HYPER, cellIdp);
    return (struct cm_cell *) 0;
}


/*
 * Find a cell by its index; unlike its cell id, cellIndex is
 * simply a relative counter incremented each time a new cell is added
 */
#ifdef SGIMIPS
struct cm_cell *cm_GetCellByIndex(register long cellIndex)
#else  /* SGIMIPS */
struct cm_cell *cm_GetCellByIndex(cellIndex)
     register long cellIndex;
#endif /* SGIMIPS */
{
    register struct cm_cell *cellp;

    lock_ObtainRead(&cm_celllock);
    for (cellp = cm_cells; cellp; cellp = cellp->next) {
	if (cellp->cellIndex == cellIndex) {
	    lock_ReleaseRead(&cm_celllock);
	    return cellp;
	}
    }
    lock_ReleaseRead(&cm_celllock);
    return (struct cm_cell *) 0;
}



/*
 * cm_NewCell() -- Locate/allocate a new cell entry for the named cell.
 *     If the cell entry already exists, its state is updated.
 * 
 * LOCKS USED: locks cm_celllock and individual cell lock; released on exit
 *
 * RETURN CODES: reference to cell object
 */
#ifdef SGIMIPS
struct cm_cell*
cm_NewCell(
  char *cellNamep,
  long cellflags,
  afsUUID *flObjIdp,
  int flCount,
  char **flNamevp,
  struct sockaddr_in *flAddrvp)
#else  /* SGIMIPS */
struct cm_cell*
cm_NewCell(cellNamep,      /* cell's name */
	   cellflags,      /* cell's config flags (unused) */
	   flObjIdp,       /* flservers' UUID */
	   flCount,        /* flserver count */
	   flNamevp,       /* flservers' principal names */
	   flAddrvp)       /* flservers' addresses */

  char *cellNamep;
  long cellflags;
  afsUUID *flObjIdp;
  int flCount;
  char **flNamevp;
  struct sockaddr_in *flAddrvp;
#endif /* SGIMIPS */
{
    struct cm_cell *cellp;
    struct sockaddr_in *addrvp;
    int i, addrvcnt;

    icl_Trace1(cm_iclSetp, CM_TRACE_NEWCELL,
	       ICL_TYPE_STRING, cellNamep);

    osi_assert(cellNamep != NULL && flNamevp != NULL && flAddrvp != NULL);

    if (flCount <= 0) {
	/* must have at least one flserver to generate cell... */
	return (struct cm_cell *) 0;
    } else if (flCount > MAXVLHOSTSPERCELL) {
	/* ... but not too many */
	flCount = MAXVLHOSTSPERCELL;
    }

    /* scan cell list for cellNamep */
    lock_ObtainWrite(&cm_celllock);

    for (cellp = cm_cells; cellp; cellp = cellp->next) {
	if (!strcmp(cellp->cellNamep, cellNamep) ||
	    (cellp->oldCellNamep && !strcmp(cellp->oldCellNamep, cellNamep))) {
	    break;
	}
    }

    if (cellp) {
	/* cell located; lock structure */
	lock_ObtainWrite(&cellp->lock);
    } else {
	/* cell not located, allocate new cell structure and initialize */
	cellp = (struct cm_cell *) osi_Alloc(sizeof(*cellp));
	bzero((char *)cellp, sizeof(*cellp));

	lock_Init(&cellp->lock);
	cellp->cellNamep = (char *) osi_Alloc(strlen(cellNamep)+1);
	strcpy(cellp->cellNamep, cellNamep);
	cellp->cellIndex = cm_cellindex++;
	cellp->lastUsed = osi_Time();

	icl_Trace1(cm_iclSetp, CM_TRACE_CREATECELL,
		   ICL_TYPE_STRING, cellp->cellNamep);

	/* lock cell structure and insert into list */
	lock_ObtainWrite(&cellp->lock);

	cellp->next = cm_cells;
	cm_cells = cellp;
    }

#ifdef KERNEL
    /* determine if this is the local cell */
    if (strcmp(cellp->cellNamep, cm_localCellName) == 0) {
	cellp->lclStates |= CLL_LCLCELL;
    } else {
	cellp->lclStates &= ~CLL_LCLCELL;
    }
#endif /* KERNEL */

    /* clear cell's existing FL-server references and set up new ones */
    bzero((char *)cellp->cellHosts, sizeof(cellp->cellHosts));

    addrvp = flAddrvp;

    for (i = 0; i < flCount; i++) {
	/* count addrs for flserver in its null-terminated addr-set */
	for (addrvcnt = 0; SR_ADDR_VAL(&addrvp[addrvcnt]) != 0; addrvcnt++);
	osi_assert(addrvcnt > 0);

	cellp->cellHosts[i] = cm_GetServer(cellp,
					   flNamevp[i],
					   SRT_FL,
					   flObjIdp,
					   addrvp,
					   addrvcnt);

	/* update server addrs in case server entry was extant */
	cm_SiteAddrReplace(cellp->cellHosts[i]->sitep, addrvp, addrvcnt);

	/* adjust address-set pointer for next flserver */
	addrvp += (addrvcnt + 1);
    }

    /* Sort servers and increment host generation counter */
    cm_SortServers(cellp->cellHosts, MAXVLHOSTSPERCELL);
    cellp->hostGen++;

    lock_ReleaseWrite(&cellp->lock);
    lock_ReleaseWrite(&cm_celllock);
    return cellp;
}



/*
 * Find out the correct cell ID for the given cell, if possible.
 */
#ifdef SGIMIPS
long cm_FindCellID(register struct cm_cell *cellp)
#else  /* SGIMIPS */
long cm_FindCellID(cellp)
    register struct cm_cell *cellp;
#endif /* SGIMIPS */
{
    struct cm_rrequest treq;
    struct cm_conn *connp;
    struct vlconf_cell *onecellp;
    u_long startTime;
    long code;

    if ((cellp->lclStates & CLL_IDSET) != 0)
	return 0;
    icl_Trace1(cm_iclSetp, CM_TRACE_FINDCELLID, ICL_TYPE_STRING,
	       (cellp->cellNamep? cellp->cellNamep : "none"));
    cm_InitUnauthReq(&treq);	/* *must* be unauth for vldb */
    onecellp = (struct vlconf_cell *) osi_Alloc(sizeof(struct vlconf_cell));
    do {
	if (connp = cm_ConnByMHosts(cellp->cellHosts, MAIN_SERVICE(SRT_FL),
				    &cellp->cellId, &treq, NULL,
				    cellp, NULL)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_GETCELLSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    code = VL_GetCellInfo(connp->connp, onecellp);
	    osi_PreemptionOff();
	    icl_Trace1(cm_iclSetp, CM_TRACE_GETCELLEND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, (afsFid *) 0, &treq,
			(struct cm_volume *)NULL, -1, startTime));
    if (code) {
	code = cm_CheckError(ENODEV, &treq);
	goto done;
    }
    /*
     * Use flserver as the authoritative source of the remote cell name.
     */
    if (strcmp((char *)onecellp->name, cellp->cellNamep) != 0) {
	if (cellp->cellNamep) {
	    /* Keep the original name to circumvent duplication from CDS cell/fs format. */
	    if (cellp->oldCellNamep)
		osi_Free((opaque)cellp->cellNamep, (strlen(cellp->cellNamep) + 1));
	    else
		cellp->oldCellNamep = cellp->cellNamep;
	}
	cellp->cellNamep = (char *) osi_Alloc(strlen((char *)onecellp->name) + 1);
	strcpy(cellp->cellNamep, (char *)onecellp->name);
    }
    if (AFS_hiszero(onecellp->CellID)) {/* manufacture an ID if necessary */
	AFS_hset64(cellp->cellId, 0, cellp->cellIndex);
    } else {
	cellp->cellId = onecellp->CellID;
    }

    cellp->lclStates |= CLL_IDSET;

done:
    osi_Free((char *) onecellp, sizeof(struct vlconf_cell));
    return code;
}
