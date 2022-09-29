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

#ident "$Id: logutil.c,v 2.61 1999/05/11 00:28:03 kenmcd Exp $"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"

extern int	errno;
int		__pmLogReads;

static char	*logfilename = NULL;
static int	logfilenamelen = 0;

/*
 * first two fields are made to look like a pmValueSet when no values are
 * present ... used to populate the pmValueSet in a pmResult when values
 * for particular metrics are not available from this log record.
 */
typedef struct {
    pmID	pc_pmid;
    int		pc_numval;	/* MUST be 0 */
    				/* value control for interpolation */
} pmid_ctl;

static __pmHashCtl	pc_hc;		/* hash control for requested metrics */

#ifdef PCP_DEBUG
static void
printstamp(struct timeval *tp)
{
    static struct tm	*tmp;
    time_t t = (time_t)tp->tv_sec;

    tmp = localtime(&t);
    fprintf(stderr, "%02d:%02d:%02d.%03d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)(tp->tv_usec/1000));
}

static void
printstamp32(__pmTimeval *tp)
{
    static struct tm	*tmp;

    tmp = localtime((time_t *)&tp->tv_sec);
    fprintf(stderr, "%02d:%02d:%02d.%03d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, tp->tv_usec/1000);
}
#endif

static int
chkLabel(__pmLogCtl *lcp, FILE *f, __pmLogLabel *lp, int vol)
{
    int		len;
    int		version = UNKNOWN_VERSION;
    int		xpectlen = sizeof(__pmLogLabel) + 2 * sizeof(len);
    int		n;

    if (vol >= 0 && vol < lcp->l_numseen && lcp->l_seen[vol]) {
	/* FastPath, cached result of previous check for this volume */
	fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
	return 0;
    }

    if (vol >= 0 && vol >= lcp->l_numseen) {
	lcp->l_seen = (int *)realloc(lcp->l_seen, (vol+1)*(int)sizeof(lcp->l_seen[0]));
	if (lcp->l_seen == NULL)
	    lcp->l_numseen = 0;
	else {
	    int 	i;
	    for (i = lcp->l_numseen; i < vol; i++)
		lcp->l_seen[i] = 0;
	    lcp->l_numseen = vol+1;
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, "chkLabel: fd=%d vol=%d", fileno(f), vol);
#endif

    fseek(f, (long)0, SEEK_SET);
    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len) || len != xpectlen) {
	if (feof(f)) {
	    clearerr(f);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, " file is empty\n");
#endif
	    return PM_ERR_NODATA;
	}
	else {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, " header read -> %d or bad header len=%d: expected %d\n",
		    n, len, xpectlen);
#endif
	    if (ferror(f)) {
		clearerr(f);
		return -errno;
	    }
	    else
		return PM_ERR_LABEL;
	}
    }

    if ((n = (int)fread(lp, 1, sizeof(__pmLogLabel), f)) != sizeof(__pmLogLabel)) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " bad label len=%d: expected %d\n",
		n, (int)sizeof(__pmLogLabel));
#endif
	if (ferror(f)) {
	    clearerr(f);
	    return -errno;
	}
	else
	    return PM_ERR_LABEL;
    }
    else {
	/* swab internal log label */
	lp->ill_magic = ntohl(lp->ill_magic);
	lp->ill_pid = ntohl(lp->ill_pid);
	lp->ill_start.tv_sec = ntohl(lp->ill_start.tv_sec);
	lp->ill_start.tv_usec = ntohl(lp->ill_start.tv_usec);
	lp->ill_vol = ntohl(lp->ill_vol);
    }

    n = (int)fread(&len, 1, sizeof(len), f);
    len = ntohl(len);
    if (n != sizeof(len) || len != xpectlen) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " trailer read -> %d or bad trailer len=%d: expected %d\n",
		n, len, xpectlen);
#endif
	if (ferror(f)) {
	    clearerr(f);
	    return -errno;
	}
	else
	    return PM_ERR_LABEL;
    }

    version = lp->ill_magic & 0xff;
    if ((lp->ill_magic & 0xffffff00) != PM_LOG_MAGIC ||
	(version != PM_LOG_VERS01 && version != PM_LOG_VERS02) ||
	lp->ill_vol != vol) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " version %d not supported\n", version);
#endif
	return PM_ERR_LABEL;
    }
    else {
	__pmIPC	ipc;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " [magic=%8x version=%d vol=%d pid=%d host=%s]\n",
		lp->ill_magic, version, lp->ill_vol, lp->ill_pid, lp->ill_hostname);
#endif

	ipc.version = version;
	ipc.ext = NULL;
	if ((__pmAddIPC(fileno(f), ipc)) < 0)
	    return -errno;
    }

    if (vol >= 0 && vol < lcp->l_numseen)
	lcp->l_seen[vol] = 1;

    return version;
}

static FILE *
_logpeek(__pmLogCtl *lcp, int vol)
{
    int		sts;
    FILE	*f;
    __pmLogLabel	label;

    sts = (int)strlen(lcp->l_name) + 6; /* name.XXXX\0 */
    if (sts > logfilenamelen) {
	if ((logfilename = (char *)realloc(logfilename, sts)) == NULL) {
	    logfilenamelen = 0;
	    return NULL;
	}
	logfilenamelen = sts;
    }

    sprintf(logfilename, "%s.%d", lcp->l_name, vol);
    if ((f = fopen(logfilename, "r")) == NULL)
	return f;

    if ((sts = chkLabel(lcp, f, &label, vol)) < 0) {
	fclose(f);
	errno = sts;
	return NULL;
    }
    
    return f;
}

int
__pmLogChangeVol(__pmLogCtl *lcp, int vol)
{
    char	name[MAXPATHLEN];
    int		sts;

    if (lcp->l_curvol == vol)
	return 0;

    if (lcp->l_mfp != NULL) {
	__pmResetIPC(fileno(lcp->l_mfp));
	fclose(lcp->l_mfp);
    }
    sprintf(name, "%s.%d", lcp->l_name, vol);
    if ((lcp->l_mfp = fopen(name, "r")) == NULL)
	return -errno;

    if ((sts = chkLabel(lcp, lcp->l_mfp, &lcp->l_label, vol)) < 0)
	return sts;

    lcp->l_curvol = vol;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, "__pmLogChangeVol: change to volume %d\n", vol);
#endif
    return sts;
}

static int
loadTI(__pmLogCtl *lcp)
{
    int		sts = 0;
    FILE	*f = lcp->l_tifp;
    int		n;
    __pmLogTI	*tip;

    lcp->l_numti = 0;
    lcp->l_ti = NULL;

    if (lcp->l_tifp != NULL) {
	fseek(f, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
	for ( ; ; ) {
	    lcp->l_ti = (__pmLogTI *)realloc(lcp->l_ti, (1 + lcp->l_numti) * sizeof(__pmLogTI));
	    if (lcp->l_ti == NULL) {
		sts = -errno;
		break;
	    }
	    tip = &lcp->l_ti[lcp->l_numti];
	    n = (int)fread(tip, 1, sizeof(__pmLogTI), f);

            if (n != sizeof(__pmLogTI)) {
		if (feof(f)) {
		    clearerr(f);
		    sts = 0; 
		    break;
		}
#ifdef PCP_DEBUG
	  	if (pmDebug & DBG_TRACE_LOG)
	    	    fprintf(stderr, "loadTI: bad TI entry len=%d: expected %d\n",
		            n, (int)sizeof(__pmLogTI));
#endif
		if (ferror(f)) {
		    clearerr(f);
		    sts = -errno;
		    break;
		}
		else {
		    sts = PM_ERR_LOGREC;
		    break;
		}
	    }
	    else {
		/* swab the temporal index record */
		tip->ti_stamp.tv_sec = ntohl(tip->ti_stamp.tv_sec);
		tip->ti_stamp.tv_usec = ntohl(tip->ti_stamp.tv_usec);
		tip->ti_vol = ntohl(tip->ti_vol);
		tip->ti_meta = ntohl(tip->ti_meta);
		tip->ti_log = ntohl(tip->ti_log);
	    }

	    lcp->l_numti++;
	}/*for*/
    }/*not null*/

    return sts;
}


const char *
__pmLogName(const char *base, int vol)
{
    static char		*tbuf;
    static int		tlen = 0;
    int			len;

    len = (int)strlen(base) + 8;
    if (len > tlen) {
	if (tlen)
	    free(tbuf);
	if ((tbuf = (char *)malloc(len)) == NULL) {
	    __pmNoMem("__pmLogName", len, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	tlen = len;
    }

    switch (vol) {
	case PM_LOG_VOL_TI:
	    sprintf(tbuf, "%s.index", base);
	    break;
	
	case PM_LOG_VOL_META:
	    sprintf(tbuf, "%s.meta", base);
	    break;

	default:
	    sprintf(tbuf, "%s.%d", base, vol);
	    break;
    }

    return tbuf;
}

FILE *
__pmLogNewFile(const char *base, int vol)
{
    const char	*fname;
    FILE	*f;
    int		save_errno;

    fname = __pmLogName(base, vol);

    if (access(fname, F_OK) != -1) {
	pmprintf("__pmLogNewFile: \"%s\" already exists, not over-written\n", fname);
	pmflush();
	setoserror(EEXIST);
	return NULL;
    }

    if ((f = fopen(fname, "w")) == NULL) {
	save_errno = oserror();
	pmprintf("__pmLogNewFile: failed to create \"%s\": %s\n", fname, strerror(save_errno));

	pmflush();
	setoserror(save_errno);
	return NULL;
    }
    else {
	__pmIPC	ipc = { PDU_VERSION, NULL };

	if ((__pmAddIPC(fileno(f), ipc)) < 0) {
	    save_errno = oserror();
	    pmprintf("__pmLogNewFile: failed to create \"%s\": %s\n", fname, strerror(save_errno));
	    pmflush();
	    setoserror(save_errno);
	    return NULL;
	}
    }

#if defined(__NUTC__)
    /* 
     * TODO: without this, NutCracker gets a segv in it's run-time DLL
     * when the stdio buffers get flushed. The problem might go away when
     * we get the released 4.1 version (currently running 4.0 beta).
     */
    setbuf(f, NULL);
#endif

    return f;
}

int
__pmLogWriteLabel(FILE *f, const __pmLogLabel *lp)
{
    int		len;
    int		sts = 0;
    __pmLogLabel outll = *lp;

    len = sizeof(*lp) + 2 * sizeof(len);
    len = htonl(len);

    /* swab */
    outll.ill_magic = htonl(outll.ill_magic);
    outll.ill_pid = htonl(outll.ill_pid);
    outll.ill_start.tv_sec = htonl(outll.ill_start.tv_sec);
    outll.ill_start.tv_usec = htonl(outll.ill_start.tv_usec);
    outll.ill_vol = htonl(outll.ill_vol);

    if ((int)fwrite(&len, 1, sizeof(len), f) != sizeof(len) ||
	(int)fwrite(&outll, 1, sizeof(outll), f) != sizeof(outll) ||
        (int)fwrite(&len, 1, sizeof(len), f) != sizeof(len)) {
	    sts = -errno;
	    pmprintf("__pmLogWriteLabel: %s\n", strerror(errno));
	    pmflush();
	    fclose(f);
    }

    return sts;
}

int
__pmLogCreate(const char *host, const char *base, int log_version,
	      __pmLogCtl *lcp)
{
    int		save_errno = 0;

    lcp->l_minvol = lcp->l_maxvol = lcp->l_curvol = 0;
    lcp->l_hashpmid.nodes = lcp->l_hashpmid.hsize = 0;
    lcp->l_hashindom.nodes = lcp->l_hashindom.hsize = 0;

    if ((lcp->l_tifp = __pmLogNewFile(base, PM_LOG_VOL_TI)) != NULL) {
	if ((lcp->l_mdfp = __pmLogNewFile(base, PM_LOG_VOL_META)) != NULL) {
	    if ((lcp->l_mfp = __pmLogNewFile(base, 0)) != NULL) {
		char *tz = __pmTimezone();
		lcp->l_label.ill_magic = PM_LOG_MAGIC | log_version;
		/*
		 * Warning	ill_hostname may be truncated, but we
		 *		guarantee it will be null-byte terminated
		 */
		strncpy(lcp->l_label.ill_hostname, host, PM_LOG_MAXHOSTLEN-1);
		lcp->l_label.ill_hostname[PM_LOG_MAXHOSTLEN-1] = '\0';
		lcp->l_label.ill_pid = getpid();
		/*
		 * hack - how do you get the TZ for a remote host?
		 */
		strcpy(lcp->l_label.ill_tz, tz ? tz : "");
		lcp->l_state = PM_LOG_STATE_NEW;
		return 0;
	    }
	    else {
		save_errno = oserror();
		unlink(__pmLogName(base, PM_LOG_VOL_TI));
		unlink(__pmLogName(base, PM_LOG_VOL_META));
		setoserror(save_errno);
	    }
	}
	else {
	    save_errno = oserror();
	    unlink(__pmLogName(base, PM_LOG_VOL_TI));
	    setoserror(save_errno);
	}
    }

    lcp->l_tifp = lcp->l_mdfp = lcp->l_mfp = NULL;
    return oserror() ? -oserror() : -EPERM;
}

/*
 * Close the log files.
 * Free up the space used by __pmLogCtl.
 */

void
__pmLogClose(__pmLogCtl *lcp)
{
    if (lcp->l_tifp != NULL) {
	fclose(lcp->l_tifp);
	__pmResetIPC(fileno(lcp->l_tifp));
	lcp->l_tifp = NULL;
    }
    if (lcp->l_mdfp != NULL) {
	fclose(lcp->l_mdfp);
	__pmResetIPC(fileno(lcp->l_mdfp));
	lcp->l_mdfp = NULL;
    }
    if (lcp->l_mfp != NULL) {
	__pmLogCacheClear(lcp->l_mfp);
	fclose(lcp->l_mfp);
	__pmResetIPC(fileno(lcp->l_mfp));
	lcp->l_mfp = NULL;
    }
    if (lcp->l_name != NULL) {
	free(lcp->l_name);
	lcp->l_name = NULL;
    }
    if (lcp->l_seen != NULL) {
	free(lcp->l_seen);
	lcp->l_seen = NULL;
	lcp->l_numseen = 0;
    }
    if (lcp->l_pmns != NULL) {
	__pmFreePMNS(lcp->l_pmns);
	lcp->l_pmns = NULL;
    }

    if (lcp->l_ti != NULL)
	free(lcp->l_ti);

    if (lcp->l_hashpmid.hsize != 0) {
	__pmHashCtl	*hcp = &lcp->l_hashpmid;
	__pmHashNode	*hp;
	__pmHashNode	*prior_hp;
	int		i;

	for (i = 0; i < hcp->hsize; i++) {
	    for (hp = hcp->hash[i], prior_hp = NULL; hp != NULL; hp = hp->next) {
		if (hp->data != NULL)
		    free(hp->data);
		if (prior_hp != NULL)
		    free(prior_hp);
		prior_hp = hp;
	    }
	    if (prior_hp != NULL)
		free(prior_hp);
	}
	free(hcp->hash);
    }

    if (lcp->l_hashindom.hsize != 0) {
	__pmHashCtl	*hcp = &lcp->l_hashindom;
	__pmHashNode	*hp;
	__pmHashNode	*prior_hp;
	__pmLogInDom	*idp;
	__pmLogInDom	*prior_idp;
	int		i;

	for (i = 0; i < hcp->hsize; i++) {
	    for (hp = hcp->hash[i], prior_hp = NULL; hp != NULL; hp = hp->next) {
		for (idp = (__pmLogInDom *)hp->data, prior_idp = NULL;
		     idp != NULL; idp = idp->next) {
		    if (idp->buf != NULL)
			free(idp->buf);
		    if (idp->allinbuf == 0 && idp->namelist != NULL)
			free(idp->namelist);
		    if (prior_idp != NULL)
			free(prior_idp);
		    prior_idp = idp;
		}
		if (prior_idp != NULL)
		    free(prior_idp);
		if (prior_hp != NULL)
		    free(prior_hp);
		prior_hp = hp;
	    }
	    if (prior_hp != NULL)
		free(prior_hp);
	}
	free(hcp->hash);
    }

}

int
__pmLogOpen(const char *name, __pmContext *ctxp)
{
    int		sts;
    int		blen;
    int		version;
    int		exists = 0;
    char	*q;
    char	*base;
    char	*tbuf;
    char	*tp;
    char	*dir;
    DIR		*dirp = NULL;
    char	filename[MAXPATHLEN];
    __pmLogCtl	*lcp = ctxp->c_archctl->ac_log;
    __pmLogLabel	label;
#if defined(HAVE_READDIR64)
    struct dirent64	*direntp;
#else
    struct dirent	*direntp;
#endif

    /*
     * find directory name component ... copy as dirname() may clobber
     * the string
     */
    if ((tbuf = strdup(name)) == NULL)
	return -oserror();
    dir = dirname(tbuf);

    /*
     * find file name component
     */
    strncpy(filename, name, MAXPATHLEN);
    if ((base = strdup(basename(filename))) == NULL) {
	sts = -oserror();
	free(tbuf);
	return sts;
    }

    if (access(name, F_OK) == 0) {
	/*
	 * file exists ... if name contains '.' and suffix is
	 * "index", "meta" or a string of digits, strip the
	 * suffix
	 */
	int	strip = 0;
	exists = 1;
	if ((q = strrchr(base, '.')) != NULL) {
	    if (strcmp(q, ".index") == 0) strip = 1;
	    else if (strcmp(q, ".meta") == 0) strip = 1;
	    else if (q[1] != '\0') {
		char	*end;
		(void)strtol(q+1, &end, 10);
		if (*end == '\0') strip = 1;
	    }
	}
	if (strip) {
	    *q = '\0';
	    exists = 0;
	}
    }

    sprintf(filename, "%s/%s", dir, base);
    if ((lcp->l_name = strdup(filename)) == NULL) {
	sts = -oserror();
	free(tbuf);
	free(base);
	return sts;
    }

    lcp->l_minvol = -1;
    lcp->l_tifp = lcp->l_mdfp = lcp->l_mfp = NULL;
    lcp->l_ti = NULL;
    lcp->l_hashpmid.nodes = lcp->l_hashpmid.hsize = 0;
    lcp->l_hashindom.nodes = lcp->l_hashindom.hsize = 0;
    lcp->l_numseen = 0; lcp->l_seen = NULL;
    lcp->l_pmns = NULL;

    blen = (int)strlen(base);
    if ((dirp = opendir(dir)) != NULL) {
#if defined(HAVE_READDIR64)
	while ((direntp = readdir64(dirp)) != NULL) {
#else
	while ((direntp = readdir(dirp)) != NULL) {
#endif
	    if (strncmp(base, direntp->d_name, blen) != 0)
		continue;
	    if (direntp->d_name[blen] != '.')
		continue;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		sprintf(filename, "%s/%s", dir, direntp->d_name);
		fprintf(stderr, "__pmLogOpen: inspect file \"%s\"\n", filename);
	    }
#endif
	    tp = &direntp->d_name[blen+1];
	    if (strcmp(tp, "index") == 0) {
		sprintf(filename, "%s/%s", dir, direntp->d_name);
		if ((lcp->l_tifp = fopen(filename, "r")) == NULL) {
		    sts = -errno;
		    goto cleanup;
		}
	    }
	    else if (strcmp(tp, "meta") == 0) {
		sprintf(filename, "%s/%s", dir, direntp->d_name);
		if ((lcp->l_mdfp = fopen(filename, "r")) == NULL) {
		    sts = -errno;
		    goto cleanup;
		}
	    }
	    else {
		char	*q;
		int	vol;
		vol = (int)strtol(tp, &q, 10);
		if (*q == '\0') {
		    if (lcp->l_minvol == -1) {
			lcp->l_minvol = vol;
			lcp->l_maxvol = vol;
		    }
		    else {
			if (vol < lcp->l_minvol)
			    lcp->l_minvol = vol;
			if (vol > lcp->l_maxvol)
			    lcp->l_maxvol = vol;
		    }
		}
	    }
	}
	closedir(dirp);
	dirp = NULL;
    }
    else {
#ifdef PCP_DEBUG
	sts = -oserror();
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "__pmLogOpen: cannot scan directory \"%s\": %s\n", dir, pmErrStr(sts));
	goto cleanup;
	
#endif
    }

    if (lcp->l_minvol == -1 || lcp->l_mdfp == NULL) {
	if (exists)
	    sts = PM_ERR_LABEL;
	else
	    sts = -ENOENT;
	goto cleanup;
    }
    
    lcp->l_curvol = -1;
    if ((sts = __pmLogChangeVol(lcp, lcp->l_minvol)) < 0)
	goto cleanup;
    else
	version = sts;

    ctxp->c_origin = lcp->l_label.ill_start;

    if (lcp->l_tifp) {
	if ((sts = chkLabel(lcp, lcp->l_tifp, &label, PM_LOG_VOL_TI)) < 0)
	    goto cleanup;
	else if (sts != version) {	/* mismatch between meta & actual data versions! */
	    sts = PM_ERR_LABEL;
	    goto cleanup;
	}

	if (lcp->l_label.ill_pid != label.ill_pid ||
		strcmp(lcp->l_label.ill_hostname, label.ill_hostname) != 0) {
	    sts = PM_ERR_LABEL;
	    goto cleanup;
	}
    }

    if ((sts = chkLabel(lcp, lcp->l_mdfp, &label, PM_LOG_VOL_META)) < 0)
	goto cleanup;
    else if (sts != version) {	/* version mismatch between meta & ti */
	sts = PM_ERR_LABEL;
	goto cleanup;
    }

    if ((sts = __pmLogLoadMeta(lcp)) < 0)
	goto cleanup;

    if ((sts = loadTI(lcp)) < 0)
	goto cleanup;
    if (lcp->l_label.ill_pid != label.ill_pid ||
	strcmp(lcp->l_label.ill_hostname, label.ill_hostname) != 0) {
	    sts = PM_ERR_LABEL;
	    goto cleanup;
    }
    
    lcp->l_refcnt = 0;
    lcp->l_physend = -1;
    free(tbuf);
    free(base);

    ctxp->c_mode = (ctxp->c_mode & 0xffff0000) | PM_MODE_FORW;

    return 0;

cleanup:
    __pmLogClose(lcp);
    free(tbuf);
    free(base);
    if (dirp != NULL) closedir(dirp);
    return sts;
}

void
__pmLogPutIndex(const __pmLogCtl *lcp, const __pmTimeval *tp)
{
    static __pmLogTI	ti;
    __pmLogTI		oti;

    if (tp == NULL) {
	struct timeval	tmp;
	gettimeofday(&tmp, NULL);
	ti.ti_stamp.tv_sec = (__int32_t)tmp.tv_sec;
	ti.ti_stamp.tv_usec = (__int32_t)tmp.tv_usec;
    }
    else
	ti.ti_stamp = *tp;		/* struct assignment */
    ti.ti_vol = lcp->l_curvol;
    fflush(lcp->l_mdfp);
    fflush(lcp->l_mfp);

    if (sizeof(off_t) > sizeof(__pm_off_t)) {
	/* check for overflow of the offset ... */
	off_t	tmp;

	tmp = ftell(lcp->l_mdfp);
	ti.ti_meta = (__pm_off_t)tmp;
	if (tmp != ti.ti_meta) {
	    __pmNotifyErr(LOG_ERR, "__pmLogPutIndex: PCP archive file (meta) too big\n");
	    exit(1);
	}
	tmp = ftell(lcp->l_mfp);
	ti.ti_log = (__pm_off_t)tmp;
	if (tmp != ti.ti_log) {
	    __pmNotifyErr(LOG_ERR, "__pmLogPutIndex: PCP archive file (data) too big\n");
	    exit(1);
	}
    }
    else {
	ti.ti_meta = (__pm_off_t)ftell(lcp->l_mdfp);
	ti.ti_log = (__pm_off_t)ftell(lcp->l_mfp);
    }

    oti.ti_stamp.tv_sec = htonl(ti.ti_stamp.tv_sec);
    oti.ti_stamp.tv_usec = htonl(ti.ti_stamp.tv_usec);
    oti.ti_vol = htonl(ti.ti_vol);
    oti.ti_meta = htonl(ti.ti_meta);
    oti.ti_log = htonl(ti.ti_log);
    fwrite(&oti, 1, sizeof(oti), lcp->l_tifp);
    fflush(lcp->l_tifp);
}

int
__pmLogPutResult(__pmLogCtl *lcp, __pmPDU *pb)
{
    /*
     * This is a bit tricky ...
     *
     *  Input
     *  :---------:----------:----------:----------------
     *  | int len | int from | int from | timestamp, .... pmResult
     *  :---------:----------:----------:----------------
     *  ^
     *  |
     *  pb
     *
     *  Output
     *  :---------:----------:----------:---------------- .........:---------:
     *  | int len | int from | int len  | timestamp, .... pmResult | int len |
     *  :---------:----------:----------:---------------- .........:---------:
     *                       ^
     *                       |
     *                       start
     */
    int			sz;
    int			sts = 0;
    __pmPDUHdr		*php = (__pmPDUHdr *)pb;

    if (lcp->l_state == PM_LOG_STATE_NEW) {
	int		i;
	__pmTimeval	*tvp;
	/*
	 * first result, do the label record
	 */
	i = sizeof(__pmPDUHdr) / sizeof(__pmPDU);
	tvp = (__pmTimeval *)&pb[i];
	lcp->l_label.ill_start.tv_sec = ntohl(tvp->tv_sec);
	lcp->l_label.ill_start.tv_usec = ntohl(tvp->tv_usec);
	lcp->l_label.ill_vol = PM_LOG_VOL_TI;
	__pmLogWriteLabel(lcp->l_tifp, &lcp->l_label);
	lcp->l_label.ill_vol = PM_LOG_VOL_META;
	__pmLogWriteLabel(lcp->l_mdfp, &lcp->l_label);
	lcp->l_label.ill_vol = 0;
	__pmLogWriteLabel(lcp->l_mfp, &lcp->l_label);
	lcp->l_state = PM_LOG_STATE_INIT;
    }

    php->from = php->len - (int)sizeof(__pmPDUHdr) + 2 * (int)sizeof(int);
    sz = php->from - (int)sizeof(int);

    /* swab */
    php->len = htonl(php->len);
    php->type = htonl(php->type);
    php->from = htonl(php->from);

    if ((int)fwrite(&php->from, 1, sz, lcp->l_mfp) != sz)
	sts = -errno;
    else
    if ((int)fwrite(&php->from, 1, sizeof(int), lcp->l_mfp) != sizeof(int))
	sts = -errno;

    /* unswab */
    php->len = ntohl(php->len);
    php->type = ntohl(php->type);
    php->from = ntohl(php->from);

    return sts;
}

/*
 * read next forward or backward from the log
 *
 * by default (peekf == NULL) use lcp->l_mfp and roll volume
 * at end of file if another volume is available
 *
 * if peekf != NULL, use this stream, and do not roll volume
 */
int
__pmLogRead(__pmLogCtl *lcp, int mode, FILE *peekf, pmResult **result)
{
    int		head;
    int		rlen;
    int		trail;
    int		sts;
    long	offset;
    __pmPDU	*pb;
    FILE	*f;
    int		n;

    /*
     * Strip any XTB data from mode, its not used here
     */
    mode &= __PM_MODE_MASK;

    if (peekf != NULL)
	f = peekf;
    else
	f = lcp->l_mfp;

    offset = ftell(f);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "__pmLogRead: fd=%d%s mode=%s vol=%d posn=%ld ",
	    fileno(f), peekf == NULL ? "" : " (peek)",
	    mode == PM_MODE_FORW ? "forw" : "back",
	    lcp->l_curvol, (long)offset);
    }
#endif

    if (mode == PM_MODE_BACK) {
       for ( ; ; ) {
	   if (offset <= sizeof(__pmLogLabel) + 2 * sizeof(int)) {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, "BEFORE start\n");
#endif
		if (peekf == NULL) {
		    int		vol = lcp->l_curvol-1;
		    while (vol >= lcp->l_minvol) {
			if (__pmLogChangeVol(lcp, vol) >= 0) {
			    f = lcp->l_mfp;
			    fseek(f, 0L, SEEK_END);
			    offset = ftell(f);
#ifdef PCP_DEBUG
			    if (pmDebug & DBG_TRACE_LOG) {
				fprintf(stderr, "vol=%d posn=%ld ",
				    lcp->l_curvol, (long)offset);
			    }
#endif
			    break;
			}
			vol--;
		    }
		    if (vol < lcp->l_minvol)
			return PM_ERR_EOL;
		}
		else
		    return PM_ERR_EOL;
	    }
	    else {
		fseek(f, -(long)sizeof(head), SEEK_CUR);
		break;
	    }
	}
    }

again:
    n = (int)fread(&head, 1, sizeof(head), f);
    head = ntohl(head); /* swab head */
    if (n != sizeof(head)) {
	if (feof(f)) {
	    /* no more data ... looks like End of Archive volume */
	    clearerr(f);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG)
		fprintf(stderr, "AFTER end\n");
#endif
	    fseek(f, offset, SEEK_SET);
	    if (peekf == NULL) {
		int	vol = lcp->l_curvol+1;
		while (vol <= lcp->l_maxvol) {
		    if (__pmLogChangeVol(lcp, vol) >= 0) {
			f = lcp->l_mfp;
			goto again;
		    }
		    vol++;
		}
	    }
	    return PM_ERR_EOL;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: hdr fread got %d expected %d\n", n, (int)sizeof(head));
#endif
	if (ferror(f)) {
	    /* I/O error */
	    clearerr(f);
	    return -errno;
	}
	else
	    /* corrupted archive */
	    return PM_ERR_LOGREC;
    }

    /*
     * This is pretty ugly (forward case shown backwards is similar) ...
     *
     *  Input
     *                         head    <--- rlen bytes -- ...--->   tail
     *  :---------:---------:---------:---------------- .........:---------:
     *  |   ???   |   ???   | int len | timestamp, .... pmResult | int len |
     *  :---------:---------:---------:---------------- .........:---------:
     *  ^                             ^
     *  |                             |
     *  pb                            read into here
     *
     *  Decode
     *  <----  __pmPDUHdr  ----------->
     *  :---------:---------:---------:---------------- .........:
     *  |   ???   |   ???   |   ???   | timestamp, .... pmResult |
     *  :---------:---------:---------:---------------- .........:
     *  ^
     *  |
     *  pb
     *
     * Note: cannot volume switch in the middle of a log record
     */

    rlen = head - 2 * (int)sizeof(head);
    if (rlen < 0 || (mode == PM_MODE_BACK && rlen > offset)) {
	/*
	 * corrupted! usually means a truncated log ...
	 */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: truncated log? rlen=%d (offset %d)\n",
		rlen, (int)offset);
#endif
	    return PM_ERR_LOGREC;
    }
    if ((pb = __pmFindPDUBuf(rlen + (int)sizeof(__pmPDUHdr))) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: __pmFindPDUBuf(%d) %s\n",
		(int)(rlen + sizeof(__pmPDUHdr)),
		strerror(errno));
#endif
	fseek(f, offset, SEEK_SET);
	return -errno;
    }

    if (mode == PM_MODE_BACK)
	fseek(f, -(long)(sizeof(head) + rlen), SEEK_CUR);

    if ((n = (int)fread(&pb[3], 1, rlen, f)) != rlen) {
	/* data read failed */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: data fread got %d expected %d\n", n, rlen);
#endif
	fseek(f, offset, SEEK_SET);
	if (ferror(f)) {
	    /* I/O error */
	    clearerr(f);
	    return -errno;
	}
	clearerr(f);

	/* corrupted archive */
	return PM_ERR_LOGREC;
    }
    else {
	/* swab pdu buffer - done later in __pmDecodeResult */
    }

    if (mode == PM_MODE_BACK)
	fseek(f, -(long)(rlen + sizeof(head)), SEEK_CUR);

    if ((n = (int)fread(&trail, 1, sizeof(trail), f)) != sizeof(trail)) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: hdr fread got %d expected %d\n", n, (int)sizeof(trail));
#endif
	fseek(f, offset, SEEK_SET);
	if (ferror(f)) {
	    /* I/O error */
	    clearerr(f);
	    return -errno;
	}
	clearerr(f);

	/* corrupted archive */
	return PM_ERR_LOGREC;
    }
    else {
	/* swab trail */
	trail = ntohl(trail);
    }

    if (trail != head) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, "\nError: hdr (%d) - trail (%d) mismatch\n", head, trail);
#endif
	return PM_ERR_LOGREC;
    }

    if (mode == PM_MODE_BACK)
	fseek(f, -(long)sizeof(trail), SEEK_CUR);

    __pmOverrideLastFd(fileno(f));
    sts = __pmDecodeResult(pb, PDU_BINARY, result); /* also swabs the result */
    /* note, PDU buffer (pb) is now pinned */

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	head -= sizeof(head) + sizeof(trail);
	fprintf(stderr, "@");
	if (sts >= 0)
	    printstamp(&(*result)->timestamp);
	else
	    fprintf(stderr, "unknown time");
	fprintf(stderr, " len=head+%d+trail\n", head);
    }
#endif

    /* exported to indicate how efficient we are ... */
    __pmLogReads++;

    if (sts < 0) {
	__pmUnpinPDUBuf(pb);
	return PM_ERR_LOGREC;
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDU) {
	int		i, j;
	fprintf(stderr, "__pmLogRead timestamp=");
	printstamp(&(*result)->timestamp);
	fprintf(stderr, " 0x%p ... 0x%p", &pb[3], &pb[head/sizeof(__pmPDU)+3]);
	fputc('\n', stderr);
	fprintf(stderr, "%03d: ", 0);
	for (j = 0, i = 0; j < head/sizeof(__pmPDU); j++) {
	    if (i == 8) {
		fprintf(stderr, "\n%03d: ", j);
		i = 0;
	    }
	    fprintf(stderr, "%8x ", pb[j+3]);	/* see above to explain "3" */
	    i++;
	}
	fputc('\n', stderr);
    }
#endif

    return 0;
}

int
__pmLogFetch(__pmContext *ctxp, int numpmid, pmID pmidlist[], pmResult **result)
{
    int		i;
    int		j;
    int		u;
    int		sts;
    int		found;
    double	tdiff;
    FILE	*f;
    pmResult	*newres;
    pmDesc	desc;
    int		kval;
    __pmHashNode	*hp;
    pmid_ctl	*pcp;
    int		nskip;
    __pmTimeval	tmp;
    int		ctxp_mode = ctxp->c_mode & __PM_MODE_MASK;

    if (ctxp_mode == PM_MODE_INTERP) {
	return __pmLogFetchInterp(ctxp, numpmid, pmidlist, result);
    }

    /* re-establish position */
    __pmLogChangeVol(ctxp->c_archctl->ac_log, ctxp->c_archctl->ac_vol);
    f = ctxp->c_archctl->ac_log->l_mfp;
    fseek(f, (long)ctxp->c_archctl->ac_offset, SEEK_SET);

more:

    found = 0;
    nskip = 0;
    *result = NULL;
    while (!found) {
	if (ctxp->c_archctl->ac_serial == 0) {
	    /*
	     * no serial access, so need to make sure we are
	     * starting in the correct place
	     */
	    int		tmp_mode;
	    nskip = 0;
	    if (ctxp_mode == PM_MODE_FORW)
		tmp_mode = PM_MODE_BACK;
	    else
		tmp_mode = PM_MODE_FORW;
	    while (__pmLogRead(ctxp->c_archctl->ac_log, tmp_mode, NULL, result) >= 0) {
		nskip++;
		tmp.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
		tmp.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
		tdiff = __pmTimevalSub(&tmp, &ctxp->c_origin);
		if (tdiff < 0 && ctxp_mode == PM_MODE_FORW ||
		    tdiff > 0 && ctxp_mode == PM_MODE_BACK) {
		    pmFreeResult(*result);
		    *result = NULL;
		    break;
		}
		else if (tdiff == 0) {
		    /* exactly the one we wanted */
		    found = 1;
		    break;
		}
		pmFreeResult(*result);
		*result = NULL;
	    }
	    ctxp->c_archctl->ac_serial = 1;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		if (nskip) {
		    fprintf(stderr, "__pmLogFetch: ctx=%d skip reverse %d to ",
			pmWhichContext(), nskip);
		    if (*result  != NULL)
			printstamp(&(*result)->timestamp);
		    else
			fprintf(stderr, "unknown time");
		    fprintf(stderr, ", found=%d\n", found);
		}
#ifdef DESPERATE
		else
		    fprintf(stderr, "__pmLogFetch: ctx=%d no skip reverse\n",
			pmWhichContext());
#endif
	    }
#endif
	    nskip = 0;
	}
	if (found)
	    break;
	if ((sts = __pmLogRead(ctxp->c_archctl->ac_log, ctxp->c_mode, NULL, result)) < 0)
	    break;
	tmp.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
	tmp.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
	tdiff = __pmTimevalSub(&tmp, &ctxp->c_origin);
	if (tdiff < 0 && ctxp_mode == PM_MODE_FORW ||
	    tdiff > 0 && ctxp_mode == PM_MODE_BACK) {
		nskip++;
		pmFreeResult(*result);
		continue;
	}
	found = 1;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    if (nskip) {
		fprintf(stderr, "__pmLogFetch: ctx=%d skip %d to ",
		    pmWhichContext(), nskip);
		    printstamp(&(*result)->timestamp);
		    fputc('\n', stderr);
		}
#ifdef DESPERATE
	    else
		fprintf(stderr, "__pmLogFetch: ctx=%d no skip\n",
		    pmWhichContext());
#endif
	}
#endif
    }
    if (found) {
	ctxp->c_origin.tv_sec = (__int32_t)(*result)->timestamp.tv_sec;
	ctxp->c_origin.tv_usec = (__int32_t)(*result)->timestamp.tv_usec;
    }

    if (*result != NULL && (*result)->numpmid == 0) {
	/*
	 * mark record, and not interpolating ...
	 * if pmFetchArchive(), return it
	 * otherwise keep searching
	 */
	if (numpmid == 0)
	    newres = *result;
	else {
	    pmFreeResult(*result);
	    goto more;
	}
    }
    else if (found) {
	if (numpmid > 0) {
	    /*
	     * not necesssarily after them all, so cherry-pick the metrics
	     * we wanted ..
	     * there are two tricks here ...
	     * (1) pmValueSets for metrics requested, but not in the pmResult
	     *     from the log are assigned using the first two fields in the
	     *     pmid_ctl struct -- since these are allocated once as
	     *	   needed, and never free'd, we have to make sure pmFreeResult
	     *     finds a pmValueSet in a pinned pdu buffer ... this means
	     *     we must find at least one real value from the log to go
	     *     with any "unavailable" results
	     * (2) real pmValueSets can be ignored, they are in a pdubuf
	     *     and will be reclaimed when the buffer is unpinned in
	     *     pmFreeResult
	     */

	    i = (int)sizeof(pmResult) + numpmid * (int)sizeof(pmValueSet *);
	    if ((newres = (pmResult *)malloc(i)) == NULL) {
		__pmNoMem("__pmLogFetch.newres", i, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    newres->numpmid = numpmid;
	    newres->timestamp = (*result)->timestamp;
	    u = 0;
	    for (j = 0; j < numpmid; j++) {
		hp = __pmHashSearch((int)pmidlist[j], &pc_hc);
		if (hp == NULL) {
		    /* first time we've been asked for this one */
		    if ((pcp = (pmid_ctl *)malloc(sizeof(pmid_ctl))) == NULL) {
			__pmNoMem("__pmLogFetch.pmid_ctl", sizeof(pmid_ctl), PM_FATAL_ERR);
			/*NOTREACHED*/
		    }
		    pcp->pc_pmid = pmidlist[j];
		    pcp->pc_numval = 0;
		    sts = __pmHashAdd((int)pmidlist[j], (void *)pcp, &pc_hc);
		    if (sts < 0)
			return sts;
		}
		else
		    pcp = (pmid_ctl *)hp->data;
		for (i = 0; i < (*result)->numpmid; i++) {
		    if (pmidlist[j] == (*result)->vset[i]->pmid) {
			/* match */
			newres->vset[j] = (*result)->vset[i];
			u++;
			break;
		    }
		}
		if (i == (*result)->numpmid) {
		    /*
		     * requested metric not returned from the log, construct
		     * a "no values available" pmValueSet from the pmid_ctl
		     */
		    newres->vset[j] = (pmValueSet *)pcp;
		}
	    }
	    if (u == 0) {
		/*
		 * not one of our pmids was in the log record, try
		 * another log record ...
		 */
		pmFreeResult(*result);
		free(newres);
		goto more;
	    }
	    /*
	     * *result malloc'd in __pmLogRead, but vset[]'s are either in
	     * pdubuf or the pmid_ctl struct
	     */
	    free(*result);
	    *result = newres;
	}
	else
	    /* numpmid == 0, pmFetchArchive() call */
	    newres = *result;
	/*
	 * Apply instance profile filtering ...
	 * Note. This is a little strange, as in the numpmid == 0,
	 *       pmFetchArchive() case, this for-loop is not executed ...
	 *       this is correct, the instance profile is ignored for
	 *       pmFetchArchive()
	 */
	for (i = 0; i < numpmid; i++) {
	    if (newres->vset[i]->numval <= 0) {
		/*
		 * no need to xlate numval for an error ... already done
		 * below __pmLogRead() in __pmDecodeResult() ... also xlate
		 * here would have been skipped in the pmFetchArchive() case
		 */
		continue;
	    }
	    sts = __pmLogLookupDesc(ctxp->c_archctl->ac_log, newres->vset[i]->pmid, &desc);
	    if (sts < 0) {
		__pmNotifyErr(LOG_WARNING, "__pmLogFetch: missing pmDesc for pmID %s: %s",
			    pmIDStr(desc.pmid), pmErrStr(sts));
		pmFreeResult(newres);
		break;
	    }
	    if (desc.indom == PM_INDOM_NULL)
		/* no instance filtering to be done for these ones */
		continue;

	    /*
	     * scan instances, keeping those "in" the instance profile
	     *
	     * WARNING
	     *		This compresses the pmValueSet INSITU, and since
	     *		these are in a pdu buffer, it trashes the the
	     *		pdu buffer and means there is no clever way of
	     *		re-using the pdu buffer to satisfy multiple
	     *		pmFetch requests
	     *		Fortunately, stdio buffering means copying to
	     *		make additional pdu buffers is not too expensive.
	     */
	    kval = 0;
	    for (j = 0; j < newres->vset[i]->numval; j++) {
		if (__pmInProfile(desc.indom, ctxp->c_instprof, newres->vset[i]->vlist[j].inst)) {
		    if (kval != j)
			 /* struct assignment */
			 newres->vset[i]->vlist[kval] = newres->vset[i]->vlist[j];
		    kval++;
		}
	    }
	    newres->vset[i]->numval = kval;
	}
    }

    /* remember your position in this context */
    ctxp->c_archctl->ac_offset = ftell(f);
    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;

    return sts;
}

/*
 * error handling wrapper around __pmLogChangeVol() to deal with
 * missing volumes ... return lcp->l_ti[] index for entry matching
 * success
 */
static int
VolSkip(__pmLogCtl *lcp, int mode,  int j)
{
    int		vol = lcp->l_ti[j].ti_vol;

    while (lcp->l_minvol <= vol && vol <= lcp->l_maxvol) {
	if (__pmLogChangeVol(lcp, vol) >= 0)
	    return j;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG) {
	    fprintf(stderr, "VolSkip: Skip missing vol %d\n", vol);
	}
#endif
	if (mode == PM_MODE_FORW) {
	    for (j++; j < lcp->l_numti; j++)
		if (lcp->l_ti[j].ti_vol != vol)
		    break;
	    if (j == lcp->l_numti)
		return PM_ERR_EOL;
	    vol = lcp->l_ti[j].ti_vol;
	}
	else {
	    for (j--; j >= 0; j--)
		if (lcp->l_ti[j].ti_vol != vol)
		    break;
	    if (j < 0)
		return PM_ERR_EOL;
	    vol = lcp->l_ti[j].ti_vol;
	}
    }
    return PM_ERR_EOL;
}

void
__pmLogSetTime(__pmContext *ctxp)
{
    __pmLogCtl	*lcp = ctxp->c_archctl->ac_log;
    int		mode;

    mode = ctxp->c_mode & __PM_MODE_MASK; /* strip XTB data */

    if (mode == PM_MODE_INTERP)
	mode = ctxp->c_delta > 0 ? PM_MODE_FORW : PM_MODE_BACK;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "__pmLogSetTime(%d) ", pmWhichContext());
	printstamp32(&ctxp->c_origin);
	fprintf(stderr, " delta=%d", ctxp->c_delta);
    }
#endif

    if (lcp->l_numti) {
	/* we have a temporal index, use it! */
	int		i;
	int		j = -1;
	int		toobig = 0;
	int		match = 0;
	int		numti = lcp->l_numti;
	__pmLogTI	*tip = lcp->l_ti;
	double		t_hi;
	double		t_lo;
	struct stat	sbuf;

	sbuf.st_size = -1;

	for (i = 0; i < numti; i++, tip++) {
	    if (tip->ti_vol < lcp->l_minvol)
		/* skip missing preliminary volumes */
		continue;
	    if (tip->ti_vol == lcp->l_maxvol) {
		/* truncated check for last volume */
		if (sbuf.st_size < 0) {
		    FILE	*f = _logpeek(lcp, lcp->l_maxvol);

		    sbuf.st_size = 0;
		    if (f != NULL) {
			fstat(fileno(f), &sbuf);
			fclose(f);
		    }
		}
		if (tip->ti_log > sbuf.st_size) {
		    j = i;
		    toobig++;
		    break;
		}
	    }
	    t_hi = __pmTimevalSub(&tip->ti_stamp, &ctxp->c_origin);
	    if (t_hi > 0) {
		j = i;
		break;
	    }
	    else if (t_hi == 0) {
		j = i;
		match = 1;
		break;
	    }
	}
	if (i == numti)
	    j = numti;

	ctxp->c_archctl->ac_serial = 1;

	if (match) {
	    j = VolSkip(lcp, mode, j);
	    if (j < 0)
		return;
	    fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
	    if (mode == PM_MODE_BACK)
		ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, " at ti[%d]@", j);
		printstamp32(&lcp->l_ti[j].ti_stamp);
	    }
#endif
	}
	else if (j < 1) {
	    j = VolSkip(lcp, PM_MODE_FORW, 0);
	    if (j < 0)
		return;
	    fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, " before start ti@");
		printstamp32(&lcp->l_ti[j].ti_stamp);
	    }
#endif
	}
	else if (j == numti) {
	    j = VolSkip(lcp, PM_MODE_BACK, numti-1);
	    if (j < 0)
		return;
	    fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
	    if (mode == PM_MODE_BACK)
		ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LOG) {
		fprintf(stderr, " after end ti@");
		printstamp32(&lcp->l_ti[j].ti_stamp);
	    }
#endif
	}
	else {
	    /*
	     *    [j-1]             [origin]           [j]
	     *      <----- t_lo -------><----- t_hi ---->
	     *
	     * choose closest index point.  if toobig, [j] is not
	     * really valid (log truncated or incomplete)
	     */
	    t_hi = __pmTimevalSub(&lcp->l_ti[j].ti_stamp, &ctxp->c_origin);
	    t_lo = __pmTimevalSub(&ctxp->c_origin, &lcp->l_ti[j-1].ti_stamp);
	    if (t_hi <= t_lo && !toobig) {
		j = VolSkip(lcp, mode, j);
		if (j < 0)
		    return;
		fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
		if (mode == PM_MODE_FORW)
		    ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, " before ti[%d]@", j);
		    printstamp32(&lcp->l_ti[j].ti_stamp);
		}
#endif
	    }
	    else {
		j = VolSkip(lcp, mode, j-1);
		if (j < 0)
		    return;
		fseek(lcp->l_mfp, (long)lcp->l_ti[j].ti_log, SEEK_SET);
		if (mode == PM_MODE_BACK)
		    ctxp->c_archctl->ac_serial = 0;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, " after ti[%d]@", j);
		    printstamp32(&lcp->l_ti[j].ti_stamp);
		}
#endif
	    }
	    if (ctxp->c_archctl->ac_serial && mode == PM_MODE_FORW) {
		/*
		 * back up one record ...
		 * index points to the END of the record!
		 */
		pmResult	*result;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, " back up ...");
#endif
		if (__pmLogRead(lcp, PM_MODE_BACK, NULL, &result) >= 0)
		    pmFreeResult(result);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG)
		    fprintf(stderr, "...");
#endif
	    }
	}
    }
    else {
	/* index either not available, or not useful */
	if (mode == PM_MODE_FORW) {
	    __pmLogChangeVol(lcp, lcp->l_minvol);
	    fseek(lcp->l_mfp, (long)(sizeof(__pmLogLabel) + 2*sizeof(int)), SEEK_SET);
	}
	else if (mode == PM_MODE_BACK) {
	    __pmLogChangeVol(lcp, lcp->l_maxvol);
	    fseek(lcp->l_mfp, (long)0, SEEK_END);
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOG)
	    fprintf(stderr, " index not useful\n");
#endif
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG)
	fprintf(stderr, " vol=%d posn=%ld serial=%d\n",
	    lcp->l_curvol, (long)ftell(lcp->l_mfp), ctxp->c_archctl->ac_serial);
#endif

    /* remember your position in this context */
    ctxp->c_archctl->ac_offset = ftell(lcp->l_mfp);
    ctxp->c_archctl->ac_vol = ctxp->c_archctl->ac_log->l_curvol;
}

int
pmGetArchiveLabel(pmLogLabel *lp)
{
    __pmContext		*ctxp;
    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL || ctxp->c_type != PM_CONTEXT_ARCHIVE)
	return PM_ERR_NOCONTEXT;
    else {
	__pmLogLabel	*rlp;
	/*
	 * we have to copy the structure to hide the differences
	 * between the internal __pmTimeval and the external struct timeval
	 */
	rlp = &ctxp->c_archctl->ac_log->l_label;
	lp->ll_magic = rlp->ill_magic;
	lp->ll_pid = rlp->ill_pid;
	lp->ll_start.tv_sec = rlp->ill_start.tv_sec;
	lp->ll_start.tv_usec = rlp->ill_start.tv_usec;
	memcpy(lp->ll_hostname, rlp->ill_hostname, PM_LOG_MAXHOSTLEN);
	memcpy(lp->ll_tz, rlp->ill_tz, sizeof(lp->ll_tz));
	return 0;
    }
}

int
pmGetArchiveEnd(struct timeval *tp)
{
    /*
     * set l_physend and l_endtime
     * at the end of ... ctxp->c_archctl->ac_log
     */
    struct stat	sbuf;
    __pmLogCtl	*lcp;
    FILE	*f;
    long	save;
    pmResult	*rp = NULL;
    pmResult	*nrp;
    int		i;
    int		sts;
    __pmContext	*ctxp;
    int		found;
    int		head;
    long	offset;
    int		vol;
    __pm_off_t	logend;
    __pm_off_t	physend;

    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL || ctxp->c_type != PM_CONTEXT_ARCHIVE)
	return PM_ERR_NOCONTEXT;
    lcp = ctxp->c_archctl->ac_log;

    /*
     * expect things to be stable, so l_maxvol is not empty, and
     * l_physend does not change for l_maxvol ... the ugliness is
     * to handle situations where these expectations are not met
     */
    found = 0;
    sts = PM_ERR_LOGREC;	/* default error condition */
    f = NULL;
    for (vol = lcp->l_maxvol; vol >= lcp->l_minvol; vol--) {
	if (lcp->l_curvol == vol) {
	    f = lcp->l_mfp;
	    save = ftell(f);
	}
	else if ((f = _logpeek(lcp, vol)) == NULL) {
	    sts = -errno;
	    break;
	}

	if (fstat(fileno(f), &sbuf) < 0) {
	    sts = -errno;
	    break;
	}

	if (vol == lcp->l_maxvol && sbuf.st_size == lcp->l_physend) {
	    /* nothing changed, return cached stuff */
	    tp->tv_sec = lcp->l_endtime.tv_sec;
	    tp->tv_usec = lcp->l_endtime.tv_usec;
	    sts = 0;
	    break;
	}

	/* if this volume is empty, try previous volume */
	if (sbuf.st_size <= (int)sizeof(__pmLogLabel) + 2*(int)sizeof(int)) {
	    if (f != lcp->l_mfp) {
		fclose(f);
		f = NULL;
	    }
	    continue;
	}

	physend = (__pm_off_t)sbuf.st_size;
	if (sizeof(off_t) > sizeof(__pm_off_t)) {
	    if (physend != sbuf.st_size) {
		__pmNotifyErr(LOG_ERR, "pmGetArchiveEnd: PCP archive file (meta) too big (%lld bytes)\n", (__int64_t)sbuf.st_size);
		exit(1);
	    }
	}

	/* try to read backwards for the last physical record ... */
	fseek(f, (long)physend, SEEK_SET);
	if (__pmLogRead(lcp, PM_MODE_BACK, f, &rp) >= 0) {
	    /* success, we are done! */
	    found = 1;
	    break;
	}

	/*
	 * failure at the physical end of file may be related to a truncted
	 * block flush for a growing archive.  Scan temporal index, and use
	 * last entry at or before end of physical file for this volume
	 */
	logend = (int)sizeof(__pmLogLabel) + 2*(int)sizeof(int);
	for (i = lcp->l_numti - 1; i >= 0; i--) {
	    if (lcp->l_ti[i].ti_vol != vol)
		continue;
	    if (lcp->l_ti[i].ti_log <= physend) {
		logend = lcp->l_ti[i].ti_log;
		break;
	    }
	}

	/*
	 * Now chase it forwards from the last index entry ...
	 *
	 * BUG 357003 - pmchart can't read archive file
	 *	turns out the index may point to the _end_ of the last
	 *	valid record, so if not at start of volume, back up one
	 *	record, then scan forwards.
	 */
	fseek(f, (long)logend, SEEK_SET);
	if (logend > (int)sizeof(__pmLogLabel) + 2*(int)sizeof(int)) {
	    if (__pmLogRead(lcp, PM_MODE_BACK, f, &rp) < 0) {
		/* this is badly damaged! */
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_LOG) {
		    fprintf(stderr, "pmGetArchiveEnd: "
                            "Error reading record ending at posn=%d ti[%d]@",
			    logend, i);
		    printstamp32(&lcp->l_ti[i].ti_stamp);
		    fputc('\n', stderr);
		}
#endif
		break;
	    }
	}

        /* Keep reading records from "logend" until can do so no more... */
	for ( ; ; ) {
	    offset = ftell(f);
	    if ((int)fread(&head, 1, sizeof(head), f) != sizeof(head))
		/* cannot read header for log record !!?? */
		break;
	    head = ntohl(head);
	    if (offset + head > physend)
		/* last record is incomplete */
		break;
	    fseek(f, offset, SEEK_SET);
	    if (__pmLogRead(lcp, PM_MODE_FORW, f, &nrp) < 0)
		/* this record is truncated, or bad, we lose! */
		break;
	    /* this one is ok, remember it as it may be the last one */
	    found = 1;
	    if (rp != NULL)
		pmFreeResult(rp);
	    rp = nrp;
	}
	if (found)
	    break;

	/*
	 * this probably means this volume contains no useful records,
	 * try the previous volume
	 */
    }/*for*/

    if (f == lcp->l_mfp)
	fseek(f, save, SEEK_SET); /* restore file pointer in current vol */ 
    else
	/* temporary FILE * from _logpeek() */
	fclose(f);

    if (found) {
	tp->tv_sec = (time_t)rp->timestamp.tv_sec;
	tp->tv_usec = (int)rp->timestamp.tv_usec;
	if (vol == lcp->l_maxvol) {
	    lcp->l_endtime.tv_sec = (__int32_t)rp->timestamp.tv_sec;
	    lcp->l_endtime.tv_usec = (__int32_t)rp->timestamp.tv_usec;
	    lcp->l_physend = physend;
	}
	pmFreeResult(rp);
	sts = 0;
    }

    return sts;
}
