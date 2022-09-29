/*
* Copyright 1995, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#ident "$Revision: 1.9 $"

#ifdef __STDC__
	#pragma weak openproj	      = _openproj
	#pragma weak closeproj	      = _closeproj
	#pragma weak fprojid          = _fprojid
	#pragma weak fprojname        = _fprojname
	#pragma weak fgetprojuser     = _fgetprojuser
	#pragma weak fgetprojall      = _fgetprojall
	#pragma weak fgetdfltprojuser = _fgetdfltprojuser
	#pragma weak fvalidateproj    = _fvalidateproj
	#pragma weak projid           = _projid
	#pragma weak projname         = _projname
	#pragma weak getprojuser      = _getprojuser
	#pragma weak getprojall       = _getprojall
	#pragma weak getdfltprojuser  = _getdfltprojuser
	#pragma weak validateproj     = _validateproj
#endif



/*
 *
 * Project ID processing functions
 *
 */



#include "synonyms.h"
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <paths.h>
#include <proj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syssgi.h>
#include "gen_extern.h"



/* Projects descriptor. A pointer to this is known as a PROJ to users. */
typedef struct ProjDescriptor {
	char *Line;		/* Line buffer */
	int  LineLen;		/* Length of line buffer */
	FILE *ProjectFP;	/* FILE* for project file */
	FILE *ProjidFP;		/* FILE* for projid file */
} pd_t;

/* Internal functions */
static char *NextField(char *, char);
static int ParsePrid(pd_t *, char **, prid_t *);
static char *FindProjectByUser(pd_t *, const char *);

#define GetLine(F,L) ((L)->Line = __getline((F), (L)->Line, &((L)->LineLen)))

/*
 * Read line and extend buffer as needed
 */
char *
__getline(FILE *fp, char *l, int *len)
{
	register int cur = 0;
	int llen = *len;
	char *nl;

	if (llen == 0)
		*len = llen = 200;
	if ((l == NULL) && ((l = malloc(llen)) == NULL)) {
		*len = 0;
		return NULL;
	}
	l[llen - 2] = '\n';	/* if changed, line was too long */
	for (;;)		/* read entire line, grow if necessary */
	{
		if (fgets(&l[cur], llen - cur, fp) == NULL) {
			*len = 0;
			free(l);
			return NULL;
		}
		if (l[llen - 2] == '\n')
			break;
		cur = llen - 1;
		llen <<= 1;
		*len = llen;
		if ((nl = realloc(l, llen)) == NULL) {
			*len = 0;
			free(l);
			return NULL;
		}
		l = nl;
		l[llen - 2] = '\n';
	}

	return l;
}

/*
 * Skip ahead in string looking for the next delimited field. If
 * one is present, the delimiter will be replaced by a null character and
 * a pointer to the field will be returned. Otherwise, NULL is returned.
 * Encountering a "#" is the same as reaching the end of string.
 */
static char *
NextField(char *p, char delim)
{
	while (*p  &&  (*p != delim)  &&  (*p != '\n')  &&  (*p != '#'))
	    ++p;
	if (*p == delim) {
		*p = '\0';
		++p;
		return p;
	}

	/* Didn't field a new field */
	*p = '\0';
	return NULL;
}


/*
 * Parse the current record as a projid record and store the name
 * and prid in specified locations. Returns zero if the current
 * record is invalid, non-zero if successful.
 */
static int
ParsePrid(pd_t *PD, char **Name,  prid_t *Prid)
{
	char *p, *idfield;

	/* Skip empty lines & comment lines */
	p = PD->Line;
	while (*p  &&  isspace(*p))
	    p++;
	if (*p == '\n'  ||  *p == '#'  ||  *p == '\0')
	    return 0;

	/* Start parsing fields */
	*Name = PD->Line;
	idfield = NextField(PD->Line,':');
	if (idfield == NULL)
	    return 0;
	NextField(idfield,':');	/* Force delimiter at end of last field */

	/* Convert id to numeric value, making sure no garbage is present */
	*Prid = (prid_t) strtoll(idfield, &p, 10);
	if (*p != '\0')
	    return 0;	/* Garbage in ID field */

	return 1;
}


/*
 * Parse project records until one matching the specified user name is
 * found, then return a pointer to the first character in the record
 * after then name. Returns NULL if the user could not be found.
 */
static char *
FindProjectByUser(pd_t *PD, const char *Name)
{
	char *p;

	while (GetLine(PD->ProjectFP, PD) != NULL) {

		/* Skip empty lines & comment lines */
		p = PD->Line;
		while (*p  &&  isspace(*p))
		    p++;
		if (*p == '\n'  ||  *p == '#'  ||  *p == '\0')
		    continue;

		/* Parse the name field and return if it matches */
		p = NextField(PD->Line,':');
		if (p == NULL)
		    continue;
		if (strcmp(PD->Line, Name) == 0)
		    return p;
	}

	/* Couldn't find user */
	return NULL;
}



/*
 * Open a projects descriptor
 */
PROJ
openproj(const char *ProjectName, const char *ProjidName)
{
	pd_t *PD;

	PD = malloc(sizeof(pd_t));
	if (PD == NULL) {
		setoserror(ENOMEM);
		return NULL;
	}

	PD->Line = NULL;
	PD->LineLen = 0;

	PD->ProjectFP = fopen((ProjectName ? ProjectName : _PATH_PROJECT),
			      "r");
	if (PD->ProjectFP == NULL) {
		free(PD);
		setoserror(ENOENT);
		return NULL;
	}

	PD->ProjidFP = fopen((ProjidName ? ProjidName : _PATH_PROJID), "r");
	if (PD->ProjidFP == NULL) {
		fclose(PD->ProjectFP);
		free(PD);
		setoserror(ENOENT);
		return NULL;
	}

	return (PROJ) PD;
}


/*
 * Close a projects descriptor
 */
void
closeproj(PROJ OPD)
{
	pd_t *PD = (pd_t *) OPD;

	fclose(PD->ProjectFP);
	fclose(PD->ProjidFP);
	if (PD->Line != NULL)
	    free(PD->Line);
	free(PD);

	return;
}


/*
 * Convert a project name to a project ID given an open projects descriptor.
 * Returns -1 if the project name was not found.
 */
prid_t
fprojid(PROJ OPD, const char *ProjName)
{
	char *CurrName;
	pd_t *PD = (pd_t *) OPD;
	prid_t Prid;

	rewind(PD->ProjidFP);
	while (GetLine(PD->ProjidFP, PD) != NULL) {
		if (!ParsePrid(PD, &CurrName, &Prid))
		    continue;

		if (strcmp(CurrName, ProjName) == 0)
		    return Prid;
	}

	setoserror(EINVAL);
	return (prid_t) -1;
}


/*
 * Convert a project name to a project ID
 */
prid_t
projid(const char *Name)
{
	PROJ   OPD;
	prid_t Result;

	OPD = openproj(NULL, NULL);
	if (OPD == NULL)
	    return (prid_t) -1L;
	Result = fprojid(OPD, Name);
	closeproj(OPD);

	return Result;
}


/*
 * Convert a project ID to a project name given an open projects descriptor.
 * Returns 1 if successful, 0 if not.
 */
int
fprojname(PROJ OPD, prid_t ProjPrid, char *ProjName, size_t NameLen)
{
	char *CurrName;
	pd_t *PD = (pd_t *) OPD;
	prid_t CurrPrid;

	rewind(PD->ProjidFP);
	while (GetLine(PD->ProjidFP, PD) != NULL) {
		if (!ParsePrid(PD, &CurrName, &CurrPrid))
		    continue;

		if (CurrPrid == ProjPrid) {
			strncpy(ProjName, CurrName, NameLen);
			ProjName[NameLen-1] = '\0';
			return 1;
		}
	}

	/* If you made it here, the search failed */
	setoserror(EINVAL);
	return 0;
}


/*
 * Same as fprojname, but without a projects descriptor.
 */
int
projname(prid_t Prid, char *Name, size_t NameLen)
{
	int  Result;
	PROJ OPD;

	OPD = openproj(NULL, NULL);
	if (OPD == NULL)
	    return 0;
	Result = fprojname(OPD, Prid, Name, NameLen);
	closeproj(OPD);

	return Result;
}


/*
 * Enumerate all project IDs. Returns the number of project IDs stored into
 * the specified array, up to the specified maximum. If a maximum of 0 is
 * specified, no project IDs are stored, but the number of valid project
 * IDs known to the system is still returned. An open projects descriptor
 * must be provided.
 */
int
fgetprojall(PROJ OPD, projid_t *ProjIDs, int MaxProjIDs)
{
	char *CurrName;
	int  i = 0;
	pd_t *PD = (pd_t *) OPD;
	prid_t CurrPrid;

	rewind(PD->ProjidFP);
	while (GetLine(PD->ProjidFP, PD) != NULL) {
		if (ParsePrid(PD, &CurrName, &CurrPrid)) {
			if (i < MaxProjIDs) {
				ProjIDs[i].proj_id = CurrPrid;
				strncpy(ProjIDs[i].proj_name,
					CurrName,
					MAXPROJNAMELEN);
				ProjIDs[i].proj_name[MAXPROJNAMELEN-1] = '\0';
			}
			++i;
			if (i == MaxProjIDs)
			    break;
		}
	}

	return i;
}


/*
 * Same as fgetprojall, but without a projects descriptor. Returns -1 if
 * unable to create a projects descriptor, number of projids otherwise.
 */
int
getprojall(projid_t *ProjIDs, int MaxProjIDs)
{
	int  Result;
	PROJ OPD;

	OPD = openproj(NULL, NULL);
	if (OPD == NULL)
	    return -1;
	Result = fgetprojall(OPD, ProjIDs, MaxProjIDs);
	closeproj(OPD);

	return Result;
}


/*
 * Stores a list of project IDs that a specified user is authorized
 * for (up to a specified maximum), given a projects descriptor.
 * Returns the actual number of project IDs stored, or -1 if the user
 * was not found.
 */
int
fgetprojuser(PROJ OPD, const char *User, projid_t *ProjIDs, int MaxProjIDs)
{
	char *CurrName, *NextName;
	int  CurrProj, i;
	pd_t *PD = (pd_t *) OPD;

	/* Look for the specified user */
	rewind(PD->ProjectFP);
	CurrName = FindProjectByUser(PD, User);
	if (CurrName == NULL) {
		return 0;
	}
	
	/* Name matches - parse individual project names. Can't do */
	/* project IDs yet or PD->Line will get overwritten.       */
	NextField(CurrName,':');	/* Null terminate entire field */
	for (CurrProj = 0;
	     CurrName  &&  (*CurrName != '\0')  &&  (CurrProj < MaxProjIDs);
	     ++CurrProj)
	{
		NextName = NextField(CurrName, ',');
		strncpy(ProjIDs[CurrProj].proj_name, CurrName, MAXPROJNAMELEN);
		ProjIDs[CurrProj].proj_name[MAXPROJNAMELEN-1] = '\0';
		CurrName = NextName;
	}

	/* Now go back and fill in the project IDs */
	for (i = 0;  i < CurrProj;  ++i)
	    ProjIDs[i].proj_id = fprojid(OPD, ProjIDs[i].proj_name);
	
	return CurrProj;
}


/*
 * Same as fgetprojuser, but without a projects descriptor.
 */
int
getprojuser(const char *User, projid_t *ProjIDs, int MaxProjIDs)
{
	int  Result;
	PROJ OPD;

	OPD = openproj(NULL, NULL);
	if (OPD == NULL)
	    return -1;
	Result = fgetprojuser(OPD, User, ProjIDs, MaxProjIDs);
	closeproj(OPD);

	return Result;
}


/*
 * Returns the default project ID for the specified user given an open
 * projects descriptor. If the user was not found or no user was specified
 * return the system default project ID.
 */
prid_t 
fgetdfltprojuser(PROJ OPD, const char *User)
{
	char *ProjName;
	pd_t *PD = (pd_t *) OPD;
	prid_t Prid = -1;

	/* Prepare for the worst */
	syssgi(SGI_GETDFLTPRID, &Prid);

	/* Finish immediately if no user was specified */
	if (User == NULL)
	    return Prid;

	/* Look for the specified user */
	rewind(PD->ProjectFP);
	ProjName = FindProjectByUser(PD, User);
	if (ProjName == NULL)
	    return Prid;
	
	/* Name matches - parse only the first project name, which */
	/* is the actual default project.			   */
	NextField(ProjName, ':');	/* Null terminate entire field */
	if (*ProjName == '\0')
	    return Prid;

	/* Make a copy of the project name so fprojid can use PD->Line */
	NextField(ProjName, ',');	/* Null terminate 1st project name */
	ProjName = strdup(ProjName);
	if (ProjName == NULL)
	    return Prid;

	Prid = fprojid(OPD, ProjName);

	free(ProjName);
	return Prid;
}


/*
 * Same as fgetdfltprojuser, but without a projects descriptor
 */
prid_t
getdfltprojuser(const char *User)
{
	prid_t Result;
	PROJ OPD;

	OPD = openproj(NULL, NULL);
	if (OPD == NULL)
	    return (prid_t) -1;
	Result = fgetdfltprojuser(OPD, User);
	closeproj(OPD);

	return Result;
}


/*
 * Searches the project database to see if the specified user is authorized
 * for the specified project. If so, the numeric project ID is returned;
 * otherwise, -1 is returned. An open projects descriptor must be provided.
 */
prid_t 
fvalidateproj(PROJ OPD, const char *User, const char *ProjName)
{
	char *CurrName, *NextName;
	pd_t *PD = (pd_t *) OPD;

	/* Look for the specified user */
	rewind(PD->ProjectFP);
	CurrName = FindProjectByUser(PD, User);
	if (CurrName == NULL) {
		setoserror(EINVAL);
		return (prid_t) -1;
	}
	
	/* Name matches - parse individual project names. */
	NextField(CurrName,':');	/* Null terminate entire field */
	while (CurrName && (*CurrName != '\0')) {
		NextName = NextField(CurrName, ',');
		if (strcmp(CurrName, ProjName) == 0) {
			return fprojid(OPD, ProjName);
		}
		CurrName = NextName;
	}

	/* If you made it to here, the search failed */
	setoserror(EACCES);
	return (prid_t) -1;
}


/*
 * Same as fvalidateproj, but without specifying a projects descriptor
 */
prid_t 
validateproj(const char *User, const char *ProjName)
{
	prid_t Result;
	PROJ OPD;

	OPD = openproj(NULL, NULL);
	if (OPD == NULL)
	    return (prid_t) -1;
	Result = fvalidateproj(OPD, User, ProjName);
	closeproj(OPD);

	return Result;
}
