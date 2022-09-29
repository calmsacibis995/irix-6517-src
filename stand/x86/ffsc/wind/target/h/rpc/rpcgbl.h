/* rpcGbl.h - header file for rpc globals */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01m,22sep92,rrr  added support for c++
01l,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01k,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -changed copyright notice
01j,01may91,elh	 added references to filename<Include>.
01i,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01h,02oct90,hjb  defined RPC_ERRBUF_SIZE, added clnt_rawExit, svc_rawExit
		   and errBuf to struct rpcModList.
01g,15may90,dnw  moved rpc static structures from various rpc modules to
		   the structure defined here.  This simplifies the
		   initializations and requires fewer mallocs.
		 renamed the structure type from MODULE_LIST to RPC_STATICS
		 renamed the macro for pointer in the tcb from taskModuleList to
		   taskRpcStatics
		 added rpc_createerr macro
01f,15apr90,jcf  made taskModuleList a macro thru taskIdCurrent.
01e,30oct89,hjb  added bindresvport in MODULE_LIST
01d,20may88,ak   made 01c version be rcs'd as part of yuba migration
01c,19apr88,llk  added nfsClientCache to MODULE_LIST.
01b,05nov87,dnw  moved definition of taskModuleList to rpcLib.c.
*/

#ifndef __INCrpcGblh
#define __INCrpcGblh

#ifdef __cplusplus
extern "C" {
#endif

#include "rpc/xdr.h"
#include "netinet/in.h"
#include "rpc/auth.h"
#include "rpc/clnt.h"
#include "rpc/svc.h"
#include "tasklib.h"

#define MAX_MARSHEL_SIZE 20
#define RPC_ERRBUF_SIZE	 256

/* statics from various rpc modules -
 * are defined in this structure which is dynamically allocated and
 * pointed to by a pointer in the tcb
 */
typedef struct rpcModList
    {
    /* statics from auth_none.c */

    struct auth_none
	{
	AUTH no_client;
	char marshalled_client[MAX_MARSHEL_SIZE];
	u_int mcnt;
	} auth_none;

    /* statics from clnt_simple.c */

    struct clnt_simple
	{
	CLIENT *client;
	int socket;
	int oldprognum;
	int oldversnum;
	int valid;
	char oldhost [50];
	} clnt_simple;

    /* statics from svc.c */

    struct svc
	{
	SVCXPRT *xports[FD_SETSIZE];
	fd_set svc_fdset;

	/*
	 * The services list
	 * Each entry represents a set of procedures (an rpc program).
	 * The dispatch routine takes request structs and runs the
	 * apropriate procedure.
	 */
	struct svc_callout
	    {
	    struct svc_callout *sc_next;
	    u_long		    sc_prog;
	    u_long		    sc_vers;
	    void		    (*sc_dispatch)();
	    } *svc_head;
	} svc;

    /* statics from svc_simple.c */

    struct svc_simple
	{
	struct proglst
	    {
	    char *(*p_progname)();
	    int  p_prognum;
	    int  p_procnum;
	    xdrproc_t p_inproc, p_outproc;
	    struct proglst *p_nxt;
	    } *proglst;
	SVCXPRT *transp;
	int madetransp;
	struct proglst *pl;
	} svc_simple;

    /* statics for nfs are defined in nfsLib and are allocated separately,
     * but referenced by this pointer
     */
    struct moduleStatics *nfsClientCache;

    /* statics for clnt_raw and svc_raw are in a structures in those modules,
     * since those modules are not used very often.
     * a pointer to the buffer that is used in common is defined here.
     */
    struct moduleStatics *clnt_raw;
    struct moduleStatics *svc_raw;
    void (*clnt_rawExit)();
    void (*svc_rawExit)();
    char *_raw_buf;			/* buffer for raw clnt/svc */
    char *errBuf;			/* buffer for err msg's */

    /* A handle on why an rpc creation routine failed (returned NULL.) */

    struct rpc_createerr task_rpc_createerr;

    } RPC_STATICS;

/* macro for the pointer to the current task's rpc statics */

#define taskRpcStatics	(taskIdCurrent->pRPCModList)

/* macro for the current task's rpc_createerr structure */

#define rpc_createerr	(taskRpcStatics->task_rpc_createerr)

#if defined(__STDC__) || defined(__cplusplus)

extern	void	clnt_genericInclude (void);
extern	void	clnt_rawInclude (void);
extern	void	clnt_simpleInclude (void);
extern	void	pmap_getmapsInclude (void);
extern	void	svc_rawInclude (void);
extern	void	svc_simpleInclude (void);
extern	void	xdr_floatInclude (void);

#else

extern	void	clnt_genericInclude ();
extern	void	clnt_rawInclude ();
extern	void	clnt_simpleInclude ();
extern	void	pmap_getmapsInclude ();
extern	void	svc_rawInclude ();
extern	void	svc_simpleInclude ();
extern	void	xdr_floatInclude ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrpcGblh */
