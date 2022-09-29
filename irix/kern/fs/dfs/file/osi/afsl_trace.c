/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: afsl_trace.c,v 65.4 1998/03/23 16:26:38 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1995, 1991 Transarc Corporation - All rights reserved. */

/* afsl_trace.c -- procedures supporting the trace facility. */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/debug.h>
#ifndef _KERNEL
#include <string.h>
#endif /* !_KERNEL */

#include <dcedfs/osi.h>
#ifndef KERNEL
#include <dce/pthread.h>
#endif /* !KERNEL */


unsigned long afsl_tr_mbz_error = 0;

struct afsl_tracePackage afsl_tr_global =
    {0, 0, "global package", 0};
struct afsl_tracePackage afsl_tr_osi =
    {0, 0, "file/osi", &afsl_tr_global};
struct afsl_tracePackage afsl_tr_cm =
    {0, 0, "file/cm", &afsl_tr_osi};
struct afsl_tracePackage afsl_tr_px =
    {0, 0, "file/px", &afsl_tr_cm};
struct afsl_tracePackage afsl_tr_fshost =
    {0, 0, "file/fshost", &afsl_tr_px};
struct afsl_tracePackage afsl_tr_xvolume =
    {0, 0, "file/xvolume", &afsl_tr_fshost};
struct afsl_tracePackage afsl_tr_xaggr =
    {0, 0, "file/xaggr", &afsl_tr_xvolume};
struct afsl_tracePackage afsl_tr_volreg =
    {0, 0, "file/volreg", &afsl_tr_xaggr};
struct afsl_tracePackage afsl_tr_nfstr =
    {0, 0, "file/nfstr", &afsl_tr_volreg};
struct afsl_tracePackage afsl_tr_xcred =
    {0, 0, "file/xcred", &afsl_tr_nfstr};
struct afsl_tracePackage afsl_tr_xvnode =
    {0, 0, "file/xvnode", &afsl_tr_xcred};
struct afsl_tracePackage afsl_tr_episode_vnops =
    {0, 0, "file/episode/eacl", &afsl_tr_xvnode};
struct afsl_tracePackage afsl_tr_episode_eacl =
    {0, 0, "file/episode/vnops", &afsl_tr_episode_vnops};
struct afsl_tracePackage afsl_tr_episode_anode =
    {0, 0, "file/episode/anode", &afsl_tr_episode_eacl};
struct afsl_tracePackage afsl_tr_episode_logbuf =
    {0, 0, "file/episode/logbuf", &afsl_tr_episode_anode};
struct afsl_tracePackage afsl_tr_episode_async =
    {0, 0, "file/episode/async", &afsl_tr_episode_logbuf};

struct afsl_tracePackage *afsl_tracePackageList =
    &afsl_tr_episode_async;

static long ParseRcsidString(
  char *rcsid,
  char **package,			/* package's name */
  u_int *packageLen,
  char **fileName,			/* file's name */
  u_int *fileLen,
  char **revision,			/* file's revision number */
  u_int *revisionLen)
{
    char *p;
    char *start;

#define literal_strlen(s)	(sizeof(s) - 1)
#define is_prefix(s, p)	\
	(strncmp(s, p, literal_strlen(p)) == 0)

    start = rcsid +
	(is_prefix(rcsid, "$" "Header:") ? literal_strlen("$" "Header:") :
	is_prefix(rcsid, "$" "Id:") ? literal_strlen("$" "Id:") :
	0);	/* something fishy */

    while (*start == ' ')
	start++;	/* skip blanks */

    /* now find beginning of the package name */
    p = start;
    while (*p != '\0') {
	if (is_prefix(p, "/rcs/") || is_prefix(p, "/src/")) {
	    p += literal_strlen("/rcs/");
	    *fileName = *package = p;
	    break;
	}
	p++;
    }
    if (*p == '\0') {
	/*
	 * Didn't find these things so pathname has unexpected form.  Reset
         * to beginning of string.
	 */
	p = start;
	*fileName = *package = p;
    }

    *packageLen = *fileLen = 0;

    /* now look for beginning and end of file name */
    while (*p != '\0') {
	if (is_prefix(p, "/RCS/")) {
	    *packageLen = p - *package;
	    p += literal_strlen("/RCS/");
	    *fileName = p;
	} else if (*p == ' ' || is_prefix(p, ",v ")) {
	    if (*packageLen == 0)
		*packageLen = *fileName - *package;
	    *fileLen = p - *fileName;
	    if (*p == ',')
		p += literal_strlen(",v");
	    p++;			/* skip the space */
	    break;
	} else if (*p == '/')
	    *fileName = p + 1;
	p++;
    }

    if (*p == '\0') {
	/*
	 * Still more trouble.  Punt package and file names, and reset in
         * hopes of finding the revision number.
	 */
	*packageLen = *fileLen = 0;
	 p = start;
	 while (*p < '0' || *p > '9')
	    p++;
    }

    /* Now pluck out the revision number */
    if (*p < '0' || *p > '9')
	return (-4);
    *revision = p;
    while ((*p <= '9' && *p >= '0') || *p == '.')
	p++;
    *revisionLen = p - *revision;
    return 0;
}

static struct prolog {
    /* struct lock_data lock; */
    int type;
    struct afsl_trace *trace;
    int line;
    char *file;
} prolog;
static int inited = 0;

static void InitTrace(void)
{
    /* lock_Init (&prolog.lock); */
    inited = 1;
}

/* EXPORT */
void afsl_AddPackage(char *packageName, struct afsl_tracePackage *package)
{
    u_int packageLen = strlen(packageName);

    if (!inited)
	InitTrace();

    package->next = afsl_tracePackageList;
    afsl_tracePackageList = package;

    if (packageName != NULL && package->name == NULL) {
	package->name = (char *)osi_Alloc(packageLen + 1);
	strcpy(package->name, packageName);
    }
}

static void GetPackage(
  char *package,			/* package name */
  struct afsl_trace *trace)		/* caller's local trace info */
{
    struct afsl_tracePackage *p;
    for (p = afsl_tracePackageList; p != NULL; p = p->next)
	if (strcmp(p->name, package) == 0) {
	    trace->control = p;
	    return;
	}
    /* no existing package found */
    trace->control = &afsl_tr_global;
}

typedef long (*afsl_parseFunc_t)
	(char *, char **, u_int *, char **, u_int *, char **, u_int *);

static afsl_parseFunc_t parseTable[] = {
    0,					/* Leave slot for AFSL_UNKNOWN_ID */
    ParseRcsidString
};
static parseTableSize = sizeof parseTable / sizeof (afsl_parseFunc_t);

static struct afsl_tracePackage losePackage =
    { 0, 0, "can't parse file id "};

/* EXPORT */
int afsl_InitTraceControl(
  char *rcsid,				/* caller's rcsid string */
  struct afsl_trace *trace)		/* caller's local trace info */
{
    char *package;			/* package's name */
    u_int packageLen;
    char *fileName;			/* file's name */
    u_int fileLen;
    char *revision;			/* file's revision number */
    u_int revisionLen;
    char *p;
    long code;
    u_int fileIdType;

    if (!inited) InitTrace();

    if (trace == NULL || rcsid == NULL)
	return 0;

    fileIdType = trace->fileIdType;
    if (fileIdType < parseTableSize && parseTable[fileIdType] != 0) {
	code = (*parseTable[fileIdType]) (rcsid, &package, &packageLen,
					  &fileName, &fileLen,
					  &revision, &revisionLen);
    } else
	code = 1;

    if (code != 0) {
	/* We couldn't parse file id, make something up */
	trace->control = &losePackage;
	if (!trace->fileName) trace->fileName = rcsid;
	trace->fileMask = 0;
	return 0;
    }

    /* Now copy all these goodies into caller's buffer. */
    if (packageLen + 1 + fileLen + 1 + revisionLen + 1 > sizeof (trace->buffer))
	return 0;
    p = trace->buffer;
    bcopy(package, p, packageLen);
    p += packageLen;
    *p++ = '\0';
    GetPackage(trace->buffer, trace);

    bcopy(fileName, p, fileLen);
    trace->fileName = p;
    p += fileLen;
    *p++ = '\0';

    bcopy(revision, p, revisionLen);
    trace->revision = p;
    p += revisionLen;
    *p = '\0';

    return 0;
}


static char *TypeString(int type)
{
    switch (type) {
      case AFSL_TR_ASSERT:  return " assertion failed";
      case AFSL_TR_ENTRY:   return " entry";
      case AFSL_TR_EXIT:    return " exit";
      case AFSL_TR_SLEEP:   return " sleep";
      case AFSL_TR_ERRMAP:  return " error code map";
      case AFSL_TR_UNUSUAL: /* fall through */
      case AFSL_TR_ALWAYS:  /* fall through */
      default: return "";
    }
}

/* afsl_TracePrintProlog -- initializes state used by an immediately following
 *     call to afsl_TracePrint.  It saves its parameters in a static buffer.
 *
 * LOCKS USED -- Since the state is static it must be protected by a lock.
 *     That lock is obtained by this call and released by the following call to
 *     afsl_TracePrint. */

/* EXPORT */
void afsl_TracePrintProlog(
  int type,
  struct afsl_trace *trace,
  int line)
{
    /* not MP safe */
    /* lock_ObtainWrite (&prolog.lock); */
    prolog.type = type;
    prolog.trace = trace;
    prolog.line = line;
}

/* afsl_TracePrint -- produces tracing output given printf like arguments.  It
 *     uses static state initialized by afsl_TracePrintProlog.
 *
 * LOCKS USED -- To properly handle this state the prolog's lock must be held.
 *     It is released early in this procedure. */

/* EXPORT */
int afsl_TracePrint(char *fmt, ...)
{
    va_list ap;
    int type;
    struct afsl_trace *trace;
    int line;

    type = prolog.type;
    trace = prolog.trace;
    line = prolog.line;
    /* not MP safe */
    /* lock_ReleaseWrite (&prolog.lock); */

    osi_printf ("(%s/%s rev. %s #%d%s) ",
	    trace->control->name, trace->fileName,
	    trace->revision, line, TypeString(type));

    /* We couldn't parse the file ID, so we'll print it raw */
    if (trace->control == &losePackage && trace->fileid)
	osi_printf("\n(Raw file ID: %s) ", trace->fileid);

    va_start(ap, fmt);
    osi_vprintf(fmt, ap);
    va_end (ap);

    osi_printf ("\n");
#ifndef KERNEL
    if (type == AFSL_TR_ASSERT) fflush (stdout);
#endif
    return 0;
}
