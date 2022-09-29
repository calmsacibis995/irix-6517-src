/*
 * -a option for chkhelp
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: all.c,v 2.6 1999/02/07 20:20:57 kenmcd Exp $"

#include <stdio.h>
#include <ndbm.h>
#include "pmapi.h"
#include "impl.h"

static int	_maxid;
static int	_numid;
typedef struct {
    int		ident;
    int		type;		/* less the ONELINE/HELP bits */
} ident_t;
static ident_t	*_idlist;

static int
idcomp(const void *a, const void *b)
{
    ident_t	*ia, *ib;

    ia = (ident_t *)a;
    ib = (ident_t *)b;
    if (ia->type & PM_TEXT_INDOM) {
	if (ib->type & PM_TEXT_INDOM)
	    return ia->ident > ib->ident;
	else
	    return -1;
    }
    else if (ib->type & PM_TEXT_INDOM)
	return 1;
    else
	return ia->ident > ib->ident;
}

void
pass0(DBM *dbf)
{
    datum	key;
    _pmHelpKey	hk;
    int		i;
    int		sts;

    _maxid = 100;
    if ((_idlist = (ident_t *)malloc(_maxid * sizeof(_idlist[0]))) == NULL) {
	_pmNoMem("pass0", _maxid * sizeof(_idlist[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
#if MALLOC_AUDIT
    _persistent_(_idlist);
#endif
    _numid = 0;

    for (key = dbm_firstkey(dbf); key.dptr != NULL; key = dbm_nextkey(dbf)) {
	/*
	 * tricky ... key.dptr may not be word-aligned, so copy rather than
	 * using pointer assignment
	 */
	if (key.dsize != sizeof(hk)) {
	    fprintf(stderr, "pass0: corrupt helpfile, illegal key size (%d)\n", key.dsize);
	    exit(1);
	}
	memcpy((void *)&hk, (void *)key.dptr, key.dsize);
	hk.type &= ~PM_TEXT_ONELINE;
	hk.type &= ~PM_TEXT_HELP;
	for (i = 0; i < _numid; i++) {
	    if (hk.ident == _idlist[i].ident && hk.type == _idlist[i].type)
		break;
	}
	if (i == _numid) {
	    _numid++;
	    if (_numid == _maxid) {
		_maxid *= 2;
		if ((_idlist = (ident_t *)realloc(_idlist, _maxid * sizeof(_idlist[0]))) == NULL) {
		    _pmNoMem("pass0", _maxid * sizeof(_idlist[0]), PM_FATAL_ERR);
		    /*NOTREACHED*/
		}
	    }
	    _idlist[i].ident = hk.ident;
	    _idlist[i].type = hk.type;
	}

    }
    sts = dbm_error(dbf);
    if (sts != 0) {
	fprintf(stderr, "pass0: dbm error %d\n", sts);
	exit(1);
    }
    qsort((void *)_idlist, _numid, sizeof(_idlist[0]), idcomp);
    _maxid = _numid;
    _numid = 0;
}

int
pass1(int *identp, int *typep)
{
    if (_numid < _maxid) {
	*identp = _idlist[_numid].ident;
	*typep = _idlist[_numid++].type;
	return 0;
    }
    else
	return -1;
}
