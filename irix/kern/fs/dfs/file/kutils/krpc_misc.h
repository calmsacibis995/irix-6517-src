
#ifndef _KRPC_MISC_H_
#define _KRPC_MISC_H_

/*
 * Exported by krpc_misc.c
 */
extern char protSeq[];
extern int krpc_BindingFromInBinding(char *, handle_t, handle_t *,
                struct sockaddr_in *, struct sockaddr_in *);
extern int krpc_BindingFromSockaddr(char *, char *, afsNetAddr *,
                handle_t *);
extern int krpc_CopyBindingWithNewPort(char *, handle_t, handle_t *, int);
extern long krpc_InvokeServer(char *, char *, rpc_if_handle_t, rpc_mgr_epv_t,
			      uuid_t *, long, int, char *, int  *,
			      struct sockaddr_in *);
extern rpc_thread_pool_handle_t krpc_SetupThreadsPool(char *, rpc_if_handle_t,
						      char *, int, int);
extern void krpc_UnInvokeServer(char *, char *, rpc_if_handle_t,
				rpc_mgr_epv_t, uuid_t *, long);

/*
 * Exported by rpc/kruntime
 */
extern void rpc__binding_inq_sockaddr(rpc_binding_handle_t, sockaddr_p_t *,
				      unsigned32 *);
#endif /* _KRPC_MISC_H_ */
