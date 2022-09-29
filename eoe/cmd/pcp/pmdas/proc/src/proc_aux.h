/*
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

#ident "$Id: proc_aux.h,v 1.2 1998/09/09 19:04:35 kenmcd Exp $"

#ifndef PROCAUX_H
#define PROCAUX_H

#include "pmapi.h"
#include "cluster.h"

extern int	_pagesize;
extern pmInDom	proc_indom;

typedef enum {PROC_PATH, PINFO_PATH} path_kind;

int getdesc(int ndesc, pmDesc desctab[], pmID pmid, pmDesc *desc);

void proc_pid_to_path(pid_t pid, char **fname, char **path, path_kind kind);
int proc_id_to_mapname(int id, char **name);
int proc_id_to_name(int id, char **name);
int proc_name_to_id(char *name, int *id);

void init_table(int ndesc, pmDesc desctab[], int dom);
void init_tables(int dom);

#endif
