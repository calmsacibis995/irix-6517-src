
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <ksys/xpc.h>

#define	DOPANIC(_s)	panic(_s)

void	xpc_init()	{return;}

const char *xpc_decode[1] = {"xpcstubs"};
const int   xpc_errno[1]  = {EIO};
    

/* ARGSUSED */
xpc_t	xpc_allocate(xpchan_t cid, xpm_t **xpm)
{ 
    DOPANIC("xpc_allocate"); 
    return(xpcError);
}

/* ARGSUSED */
xpc_t	xpc_send(xpchan_t cid, xpm_t *m)	
{ 
    DOPANIC("xpc_send"); 
    return(xpcError);
}

/* ARGSUSED */
xpc_t	xpc_send_notify(xpchan_t cid, xpm_t *m, xpc_async_rtn_t r, void *k)
{ 
    DOPANIC("xpc_send_notify"); 
    return(xpcError);
}

/* ARGSUSED */
xpc_t	xpc_connect(partid_t p, int c, __uint32_t ms, __uint32_t mc, 
		    xpc_async_rtn_t ar, __uint32_t flags, __uint32_t t,
		    xpchan_t *cid)
{ 
    return(xpcError); 
}

/* ARGSUSED */
void	xpc_disconnect(xpchan_t cid)		
{ 
    DOPANIC("xpc_disconnect"); 
}

/* ARGSUSED */
xpc_t	xpc_receive_uio(xpchan_t cid, uio_t *uio)	
{ 
    DOPANIC("xpc_receive_uio");
    return(xpcError);
}

/* ARGSUSED */
xpc_t	xpc_receive(xpchan_t cid, xpm_t **xpm)	
{ 
    DOPANIC("xpc_receive");
    return(xpcError);
}

/* ARGSUSED */
xpc_t	xpc_receive_xpmb(xpmb_t *x, __uint32_t *c, xpmb_t **xo,
			 alenlist_t al, ssize_t *size)
{
    DOPANIC("xpc_receive_xpmb");
    return(xpcError);
}    

/* ARGSUSED */
void	xpc_done(xpchan_t cid, xpm_t *m)
{
    DOPANIC("xpc_done");
}

/* ARGSUSED */
xpc_t	xpc_poll(xpchan_t cid, short events, int anyyet, 
		 short *reventsp, struct pollhead **phpp,
		 unsigned int *genp)
{
    DOPANIC("xpc_poll");
    return(xpcError);
}

/* ARGSUSED */
void	xpc_check(xpchan_t cid, xpm_t *m)	
{ 	
    return;
}

/* ARGSUSED */
xpc_t xpc_avail(partid_t p)
{
    return(xpcError);
}

/* idbg routines */

void	xpc_dump_xpr()	{ return; }
void	xpc_dump_xpe()	{ return; }
void 	xpc_dump_xpm()  { return; }   
