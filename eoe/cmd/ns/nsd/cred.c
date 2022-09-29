/*
** This file contains to create and manipulate nsd creditial structures.
*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "nfs.h"

/*
** This routine creates a new credential using the data provided.
*/
/*VARARGS2*/
nsd_cred_t *
nsd_cred_new(uid_t uid, int gids, ...)
{
	nsd_cred_t *cp;
	va_list ap;
	int i;

	cp = (nsd_cred_t *)nsd_calloc(1, sizeof(*cp));
	if (! cp) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nsd_cred_new: failed malloc\n");
		return (nsd_cred_t *)0;
	}

	cp->c_uid = uid;
	cp->c_gids = (gids > NSD_MAX_GIDS) ? NSD_MAX_GIDS : gids;
	va_start(ap, gids);
	for (i = 0; i < cp->c_gids; i++) {
		cp->c_gid[i] = (uid_t)va_arg(ap, gid_t);
	}
	for (; i < NSD_MAX_GIDS; i++) {
		cp->c_gid[i] = NFS_GID_NOBODY;
	}

	cp->c_nlink = 1;

	return cp;
}

/*
** This routine removes a reference to a credential, and if it goes to
** zero, frees the structure.
*/
void
nsd_cred_clear(nsd_cred_t **cpp)
{
	nsd_cred_t *cp;

	if (! cpp && ! *cpp) {
		return;
	}
	cp = *cpp;

	if ((--cp->c_nlink) <= 0) {
		if (cp->c_data && cp->c_vops.cv_free) {
			(*cp->c_vops.cv_free)(cp->c_data);
		}

		free(cp);
	}
	*cpp = (nsd_cred_t *)0;
}

/*
** This is just a wrapper around calling the library verify routine for
** this credential.
*/
int
nsd_cred_verify(nsd_file_t *fp, nsd_cred_t *cp)
{
	if (fp && fp->f_cred && fp->f_cred->c_vops.cv_verify) {
		return (*fp->f_cred->c_vops.cv_verify)(fp, cp);
	}

	return NSD_OK;
}

/*
** This is a wrapper to set the data and operations pointers, it frees
** old data if it was already set.
*/
int
nsd_cred_data_set(nsd_file_t *fp, void *cdata, void (*cfree)(void *),
    int (*cverf)(nsd_file_t *, nsd_cred_t *))
{
	if (fp && fp->f_cred) {
		if (fp->f_cred->c_data && fp->f_cred->c_vops.cv_free) {
			(*fp->f_cred->c_vops.cv_free)(fp->f_cred->c_data);
		}

		fp->f_cred->c_data = cdata;
		fp->f_cred->c_vops.cv_free = cfree;
		fp->f_cred->c_vops.cv_verify =
		    (int (*)(void *, struct nsd_cred *))cverf;

		return NSD_OK;
	}

	return NSD_ERROR;
}
