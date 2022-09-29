/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: icl_control.c,v 65.7 1998/04/01 14:15:59 gwehrman Exp $";
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
 * HISTORY
 * $Log: icl_control.c,v $
 * Revision 65.7  1998/04/01 14:15:59  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added missing return value.
 *
 * Revision 65.6  1998/03/24 17:20:06  lmc
 * Changed an osi_mutex_t to a mutex_t and changed osi_mutex_enter() to
 * mutex_lock and osi_mutex_exit to mutex_unlock.
 * Added a name parameter to osi_mutex_init() for debugging purposes.
 *
 * Revision 65.5  1998/03/23  16:25:42  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/01/20 16:46:45  lmc
 * Initial merge of icl tracing for kudzu.  This picks up the support for
 * tracing 64 bit kernels.  There are still some compile errors because
 * it uses AFS_hgethi/AFS_hgetlo which isn't done yet.
 *
 * Revision 65.3  1997/11/06  19:58:08  jdoak
 * Source Identification added.
 *
 * Revision 65.2  1997/10/24  22:01:07  gwehrman
 * Changed include for pthread.h
 * to dce/pthread.h.
 * Revision 64.2  1997/06/24  19:43:00  lmc
 * Added support for tracing 64bit kernels.  The dfstrace command is assumed
 * to always be built as N32.  The kernel can be either 32 or 64bit.
 *
 * Revision 65.1  1997/10/20  19:17:39  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.43.1  1996/10/02  17:52:02  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:40:46  damon]
 *
 * Revision 1.1.38.3  1994/07/13  22:25:33  devsrc
 * 	merged with bl-10
 * 	[1994/06/28  21:04:50  devsrc]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  15:59:35  mbs]
 * 
 * Revision 1.1.38.2  1994/06/09  14:11:12  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:06  annie]
 * 
 * Revision 1.1.38.1  1994/02/04  20:20:55  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:09  devsrc]
 * 
 * Revision 1.1.36.1  1993/12/07  17:26:49  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/06  13:40:41  jaffe]
 * 
 * $EndLog$
 */

/* This file contains routines for logging events, given pointers to
 * sets.
 */

#include <icl.h>	/* includes standard stuff */

#if !defined(KERNEL) 
#include <dce/pthread.h>
#endif /* !KERNEL */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/icl/RCS/icl_control.c,v 65.7 1998/04/01 14:15:59 gwehrman Exp $")

int icl_inited = 0;

/* init function, called once, under icl_lock */
int
icl_Init(void)
{
    if (icl_inited)
#ifdef SGIMIPS
	return 0;
#else
	return;
#endif
    icl_inited = 1;
#if defined(SGIMIPS) && defined(_KERNEL)
    osi_mutex_init (&icl_lock, "icl_lock");
#else
    osi_mutex_init (&icl_lock);
#endif

#ifndef KERNEL
    /* setup signal handler, in user space */
#endif /* KERNEL */

    return 0;
}

#ifdef KERNEL
int
afscall_icl(
  long opcode,
  long p1, long p2, long p3, long p4,
  long *retval
)
{
#ifdef SGIMIPS
    signed32 *lp, elts, flags;
#else    
    register int i;
    long *lp, elts, flags;
#endif /* SGIMIPS */    
    register long code;
    struct icl_log *logp;
    struct icl_set *setp;
#ifdef SGIMIPS
    size_t temp;
#else	 
    unsigned int temp;
#endif /* SGIMIPS */    
    char tname[65];
#ifdef SGIMIPS
    unsigned32 startCookie;
    signed32 allocated;
#else    
    long startCookie;
    long allocated;
#endif /* SGIMIPS */    
    struct icl_log *tlp;

    /* make sure we're running as root */
    if (!osi_suser(osi_getucred()))
        return EACCES;

    switch (opcode) {
    case ICL_OP_COPYOUTCLR:	/* copy out data then clear */
    case ICL_OP_COPYOUT:	/* copy ouy data */
	/* copyout: p1=logname, p2=&buffer, p3=size(words), p4=&cookie
	 * return flags<<24 + nwords.
	 * updates cookie to updated start (not end) if we had to
	 * skip some records.
	 */
	code = (long) osi_copyinstr((char *)p1, tname, sizeof(tname), &temp);
	if (code) return (int)code;
#ifdef SGIMIPS
	code = (long)osi_copyin((char *)p4, (char *)&startCookie, sizeof(unsigned32));
#else
	code = (long)osi_copyin((char *)p4, (char *)&startCookie, sizeof(long));
#endif /* SGIMIPS */	
	if (code) return (int)code;
	logp = icl_FindLog(tname);
	if (!logp) return ENOENT;
#ifdef SGIMIPS
	lp = (signed32 *) osi_AllocBufferSpace();
	elts = osi_BUFFERSIZE / sizeof(unsigned32);
	if (p3 < elts) elts = p3;
	flags = (opcode == ICL_OP_COPYOUT) ? 0 : ICL_COPYOUTF_CLRAFTERREAD;
	code = icl_CopyOut(logp, lp, &elts, &startCookie, &flags);
#else	
	lp = (long *) osi_AllocBufferSpace();
	elts = osi_BUFFERSIZE / sizeof(long);
	if (p3 < elts) elts = p3;
	flags = (opcode == ICL_OP_COPYOUT) ? 0 : ICL_COPYOUTF_CLRAFTERREAD;
	code = icl_CopyOut(logp, lp, &elts, (unsigned long *) &startCookie,
			   &flags);
#endif /* SGIMIPS */	
	if (code) {
	    osi_FreeBufferSpace((struct osi_buffer *) lp);
	    return (int)code;
	}
#ifdef SGIMIPS	
	code = osi_copyout((char *)lp, (char *)p2, 
		(int)(elts * sizeof(signed32)));
	if (code) goto done;
	code = osi_copyout((char *) &startCookie, (char *)p4,
			   sizeof(unsigned32));
#else
	code = osi_copyout((char *)lp, (char *)p2, elts * sizeof(long));
	if (code) goto done;
	code = osi_copyout((char *) &startCookie, (char *)p4, sizeof(long));
#endif /* SGIMIPS */	
	if (code) goto done;
	*retval = (flags<<24) | (elts & 0xffffff);
      done:
	icl_LogRele(logp);
	osi_FreeBufferSpace((struct osi_buffer *) lp);
	break;

    case ICL_OP_ENUMLOGS:	/* enumerate logs */
	/* enumerate logs: p1=index, p2=&name, p3=sizeof(name), p4=&size.
	 * return 0 for success, otherwise error.
	 */
	for(tlp = icl_allLogs; tlp; tlp=tlp->nextp) {
	    if (p1-- == 0) break;
	}
	if (!tlp) return ENOENT;	/* past the end of file */
	temp = strlen(tlp->name)+1;
	if (temp > p3) return EINVAL;
	code = osi_copyout(tlp->name, (char *) p2, (int)temp);
	if (!code)	/* copy out size of log */
#ifdef SGIMIPS
	    code = osi_copyout((char *)&tlp->logSize,
			       (char *)p4, sizeof (signed32));
#else	     
	    code = osi_copyout((char *)&tlp->logSize,
			       (char *)p4, sizeof (long));
#endif /* SGIMIPS */	
	break;

    case ICL_OP_ENUMLOGSBYSET:	/* enumerate logs by set name */
	/* enumerate logs: p1=setname, p2=index, p3=&name, p4=sizeof(name).
	 * return 0 for success, otherwise error.
	 */
	code = osi_copyinstr((char *)p1, tname, sizeof (tname), &temp);
	if (code) return (int)code;
	setp = icl_FindSet(tname);
	if (!setp) return ENOENT;
	if (p2 > ICL_LOGSPERSET)
	    return EINVAL;
	if (!(tlp = setp->logs[p2]))
	    return EBADF;
	temp = strlen(tlp->name)+1;
	if (temp > p4) return EINVAL;
	code = osi_copyout(tlp->name, (char *)p3, (int)temp);
	break;

    case ICL_OP_CLRLOG:		/* clear specified log */
	/* zero out the specified log: p1=logname */
	code = osi_copyinstr((char *)p1, tname, sizeof (tname), &temp);
	if (code) return (int)code;
	logp = icl_FindLog(tname);
	if (!logp) return ENOENT;
	code = icl_ZeroLog(logp);
	icl_LogRele(logp);
	break;

    case ICL_OP_CLRSET:		/* clear specified set */
	/* zero out the specified set: p1=setname */
	code = osi_copyinstr((char *)p1, tname, sizeof (tname), &temp);
	if (code) return (int)code;
	setp = icl_FindSet(tname);
	if (!setp) return ENOENT;
	code = icl_ZeroSet(setp);
	icl_SetRele(setp);
	break;

    case ICL_OP_CLRALL:		/* clear all logs */
	/* zero out all logs -- no args */
	code = 0;
	icl_MakePreemptionRight();
	osi_mutex_enter(&icl_lock);
	for(tlp = icl_allLogs; tlp; tlp=tlp->nextp) {
	    tlp->refCount++;	/* hold this guy */
	    osi_mutex_exit(&icl_lock);
	    /* don't clear persistent logs */
	    if ((tlp->states & ICL_LOGF_PERSISTENT) == 0)
		code = icl_ZeroLog(tlp);
	    osi_mutex_enter(&icl_lock);
	    if (--tlp->refCount == 0)
		icl_ZapLog_r(tlp);
	    if (code) break;
	}
	osi_mutex_exit(&icl_lock);
	icl_UnmakePreemptionRight();
	break;

    case ICL_OP_ENUMSETS:	/* enumerate all sets */
	/* enumerate sets: p1=index, p2=&name, p3=sizeof(name), p4=&states.
	 * return 0 for success, otherwise error.
	 */
	for(setp = icl_allSets; setp; setp = setp->nextp) {
	    if (p1-- == 0) break;
	}
	if (!setp) return ENOENT;	/* past the end of file */
	temp = strlen(setp->name)+1;
	if (temp > p3) return EINVAL;
	code = osi_copyout(setp->name, (char *)p2, temp);
	if (!code)	/* copy out size of log */
#ifdef SGIMIPS
	    code = osi_copyout((char *)&setp->states,
				(char *)p4, sizeof (signed32));
#else	     
	    code = osi_copyout((char *)&setp->states,
				(char *)p4, sizeof (long));
#endif /* SGIMIPS */	
	break;

    case ICL_OP_SETSTAT:	/* set status on a set */
	/* activate the specified set: p1=setname, p2=op */
	code = osi_copyinstr((char *)p1, tname, sizeof(tname), &temp);
	if (code) return (int)code;
	setp = icl_FindSet(tname);
	if (!setp) return ENOENT;
	code = icl_SetSetStat(setp, (int)p2);
	icl_SetRele(setp);
	break;

    case ICL_OP_SETSTATALL:	/* set status on all sets */
	/* activate the specified set: p1=op */
	code = 0;
	icl_MakePreemptionRight();
	osi_mutex_enter(&icl_lock);
	for(setp = icl_allSets; setp; setp=setp->nextp) {
	    setp->refCount++;	/* hold this guy */
	    osi_mutex_exit(&icl_lock);
	    /* don't set states on persistent sets */
	    if ((setp->states & ICL_SETF_PERSISTENT) == 0)
		code = icl_SetSetStat_r(setp, p1);
	    osi_mutex_enter(&icl_lock);
	    if (--setp->refCount == 0)
		icl_ZapSet_r(setp);
	    if (code) break;
	}
	osi_mutex_exit(&icl_lock);
	icl_UnmakePreemptionRight();
	break;

    case ICL_OP_SETLOGSIZE:		/* set size of log */
	/* set the size of the specified log: p1=logname, p2=size (in words) */
	code = osi_copyinstr((char *)p1, tname, sizeof(tname), &temp);
	if (code) return (int)code;
	logp = icl_FindLog(tname);
	if (!logp) return ENOENT;
	code = icl_LogSetSize(logp, (int)p2);
	icl_LogRele(logp);
	break;

    case ICL_OP_GETLOGINFO:		/* get size of log */
	/* zero out the specified log: p1=logname, p2=&logSize, p3=&allocated */
	code = osi_copyinstr((char *)p1, tname, sizeof(tname), &temp);
	if (code) return (int)code;
	logp = icl_FindLog(tname);
	if (!logp) return ENOENT;
	allocated = !!logp->datap;
#ifdef SGIMIPS
	code = osi_copyout((char *)&logp->logSize, (char *) p2, sizeof(signed32));
#else	
	code = osi_copyout((char *)&logp->logSize, (char *) p2, sizeof(long));
#endif /* SGIMIPS */	
	if (!code)
#ifdef SGIMIPS
	    code = osi_copyout((char *)&allocated, (char *) p3, sizeof(signed32));
#else	     
	    code = osi_copyout((char *)&allocated, (char *) p3, sizeof(long));
#endif /* SGIMIPS */	
	icl_LogRele(logp);
	break;

    case ICL_OP_GETSETINFO:		/* get state of set */
	/* zero out the specified set: p1=setname, p2=&state */
	code = osi_copyinstr((char *)p1, tname, sizeof(tname), &temp);
	if (code) return (int)code;
	setp = icl_FindSet(tname);
	if (!setp) return ENOENT;
#ifdef SGIMIPS
	code = osi_copyout((char *)&setp->states, (char *) p2, sizeof(signed32));
#else	
	code = osi_copyout((char *)&setp->states, (char *) p2, sizeof(long));
#endif /* SGIMIPS */	
	icl_SetRele(setp);
	break;

    default:
	code = EINVAL;
    }

    return (int)code;
}
#endif /* KERNEL */
