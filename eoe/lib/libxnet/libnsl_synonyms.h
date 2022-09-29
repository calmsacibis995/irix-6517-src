/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __libnsl_synonyms_h__
#define __libnsl_synonyms_h__ 1

#define openfiles		__openfiles
#define t_errlist		__t_errlist
#define tiusr_statetbl		__tiusr_statetbl
#define t_errno			__t_errno
#define t_nerr			__t_nerr


#define authdes_create		_authdes_create
#define authdes_getucred	_authdes_getucred
#define authdes_seccreate	_authdes_seccreate
#define authnone_create		_authnone_create
#define authsys_create		_authsys_create
#define authsys_create_default	_authsys_create_default
#define callrpc			_callrpc
#define cbc_crypt		_cbc_crypt
#define check_version		_check_version
#define clnt_broadcast		_clnt_broadcast
#define clnt_create		_clnt_create
#define clnt_create_vers	_clnt_create_vers
#define clnt_dg_create		_clnt_dg_create
#define clnt_pcreateerror	_clnt_pcreateerror
#define clnt_perrno		_clnt_perrno
#define clnt_perror		_clnt_perror
#define clnt_raw_create		_clnt_raw_create
#define clnt_spcreateerror	_clnt_spcreateerror
#define clnt_sperrno		_clnt_sperrno
#define clnt_sperror		_clnt_sperror
#define clnt_tli_create		_clnt_tli_create
#define clnt_tp_create		_clnt_tp_create
#define clnt_vc_create		_clnt_vc_create
#define clntraw_create		_clntraw_create
#define clnttcp_create		_clnttcp_create
#define clntudp_bufcreate	_clntudp_bufcreate
#define clntudp_create		_clntudp_create
#define cs_connect		_cs_connect
#define cs_perror		_cs_perror
#define des_setparity		_des_setparity
#define doconfig		_doconfig
#define ecb_crypt		_ecb_crypt
#define endhostent		_endhostent
#define endnetconfig		_endnetconfig
#define endnetpath		_endnetpath
#define endrpcent		_endrpcent
#define ffs			_ffs
#define freenetconfigent	_freenetconfigent
#define get_myaddress		_get_myaddress
#define getbroadcastnets	_getbroadcastnets
#define getdomainname		_getdomainname
#define gethostent		_gethostent
#define gethostname		_gethostname
#define getkey			_getkey
#define getnetconfig		_getnetconfig
#define getnetconfigent		_getnetconfigent
#define getnetid		_getnetid
#define getnetname		_getnetname
#define getnetpath		_getnetpath
#define getpublicandprivatekey	_getpublicandprivatekey
#define getpublickey		_getpublickey
#define getrpcbyname		_getrpcbyname
#define getrpcbynumber		_getrpcbynumber
#define getrpcent		_getrpcent
#define getsecretkey		_getsecretkey
#define host2netname		_host2netname
#define inet_addr		_inet_addr
#define inet_netof		_inet_netof
#define inet_ntoa		_inet_ntoa
#define key_decryptsession	_key_decryptsession
#define key_encryptsession	_key_encryptsession
#define key_gendes		_key_gendes
#define key_setsecret		_key_setsecret
#define nc_perror		_nc_perror
#define nc_sperror		_nc_sperror
#define negotiate_broadcast	_negotiate_broadcast
#define netdir_free		_netdir_free
#define netdir_perror		_netdir_perror
#define netdir_sperror		_netdir_sperror
#define netname2host		_netname2host
#define netname2user		_netname2user
#define passwd2des		_passwd2des
#define pmap_getmaps		_pmap_getmaps
#define pmap_getport		_pmap_getport
#define pmap_rmtcall		_pmap_rmtcall
#define pmap_set		_pmap_set
#define pmap_unset		_pmap_unset
#define read_status		_read_status
#define registerrpc		_registerrpc
#define rpc_broadcast		_rpc_broadcast
#define rpc_call		_rpc_call
#define rpc_createerr		_rpc_createerr
#define rpc_nullproc		_rpc_nullproc
#define rpc_reg			_rpc_reg
#define rpcb_getaddr		_rpcb_getaddr
#define rpcb_getmaps		_rpcb_getmaps
#define rpcb_gettime		_rpcb_gettime
#define rpcb_rmtcall		_rpcb_rmtcall
#define rpcb_set		_rpcb_set
#define rpcb_taddr2uaddr	_rpcb_taddr2uaddr
#define rpcb_uaddr2taddr	_rpcb_uaddr2taddr
#define rpcb_unset		_rpcb_unset
#define rtime_tli		_rtime_tli
#define setdomainname		_setdomainname
#define sethostent		_sethostent
#define sethostfile		_sethostfile
#define setnetconfig		_setnetconfig
#define setnetpath		_setnetpath
#define setrpcent		_setrpcent
#define svc_create		_svc_create
#define svc_dg_create		_svc_dg_create
#define svc_dg_enablecache	_svc_dg_enablecache
#define svc_fd_create		_svc_fd_create
#define svc_fdset		_svc_fdset
#define svc_getreq		_svc_getreq
#define svc_getreqset		_svc_getreqset
#define svc_raw_create		_svc_raw_create
#define svc_reg			_svc_reg
#define svc_run			_svc_run
#define svc_sendreply		_svc_sendreply
#define svc_tli_create		_svc_tli_create
#define svc_tp_create		_svc_tp_create
#define svc_unreg		_svc_unreg
#define svc_vc_create		_svc_vc_create
#define svc_versquiet		_svc_versquiet
#define svcauthdes_stats	_svcauthdes_stats
#define svcerr_auth		_svcerr_auth
#define svcerr_decode		_svcerr_decode
#define svcerr_noproc		_svcerr_noproc
#define svcerr_noprog		_svcerr_noprog
#define svcerr_progvers		_svcerr_progvers
#define svcerr_systemerr	_svcerr_systemerr
#define svcerr_weakauth		_svcerr_weakauth
#define svcfd_create		_svcfd_create
#define svcraw_create		_svcraw_create
#define svctcp_create		_svctcp_create
#define svcudp_bufcreate	_svcudp_bufcreate
#define svcudp_create		_svcudp_create
#define t_accept		_t_accept
#define t_alloc			_t_alloc
#define t_bind			_t_bind
#define t_close			_t_close
#define t_connect		_t_connect
#define t_error			_t_error
#define t_free			_t_free
#define t_getinfo		_t_getinfo
#define t_getname		_t_getname
#ifdef _BUILDING_LIBXNET
#define t_getprotaddr		_t_getprotaddr
#endif
#define t_getstate		_t_getstate
#define t_listen		_t_listen
#define t_look			_t_look
#define t_open			_t_open
#define t_optmgmt		_t_optmgmt
#define t_rcv			_t_rcv
#define t_rcvconnect		_t_rcvconnect
#define t_rcvdis		_t_rcvdis
#define t_rcvrel		_t_rcvrel
#define t_rcvudata		_t_rcvudata
#define t_rcvuderr		_t_rcvuderr
#define t_snd			_t_snd
#define t_snddis		_t_snddis
#define t_sndrel		_t_sndrel
#define t_sndudata		_t_sndudata
#ifdef _BUILDING_LIBXNET
#define t_strerror		_t_strerror
#endif
#define t_sync			_t_sync
#define t_unbind		_t_unbind
#define user2netname		_user2netname
#define write_dialrequest	_write_dialrequest
#define xdecrypt		_xdecrypt
#define xdr_accepted_reply	_xdr_accepted_reply
#define xdr_array		_xdr_array
#define xdr_authdes_cred	_xdr_authdes_cred
#define xdr_authdes_verf	_xdr_authdes_verf
#define xdr_authsys_parms	_xdr_authsys_parms
#define xdr_bool		_xdr_bool
#define xdr_bytes		_xdr_bytes
#define xdr_callhdr		_xdr_callhdr
#define xdr_callmsg		_xdr_callmsg
#define xdr_char		_xdr_char
#define xdr_cryptkeyarg		_xdr_cryptkeyarg
#define xdr_cryptkeyres		_xdr_cryptkeyres
#define xdr_des_block		_xdr_des_block
#define xdr_double		_xdr_double
#define xdr_enum		_xdr_enum
#define xdr_float		_xdr_float
#define xdr_free		_xdr_free
#define xdr_getcredres		_xdr_getcredres
#define xdr_gid_t		_xdr_gid_t
#define xdr_int			_xdr_int
#define xdr_keybuf		_xdr_keybuf
#define xdr_keystatus		_xdr_keystatus
#define xdr_long		_xdr_long
#define xdr_netbuf		_xdr_netbuf
#define xdr_netnamestr		_xdr_netnamestr
#define xdr_netobj		_xdr_netobj
#define xdr_opaque		_xdr_opaque
#define xdr_opaque_auth		_xdr_opaque_auth
#define xdr_pmap		_xdr_pmap
#define xdr_pmaplist		_xdr_pmaplist
#define xdr_pointer		_xdr_pointer
#define xdr_reference		_xdr_reference
#define xdr_rejected_reply	_xdr_rejected_reply
#define xdr_replymsg		_xdr_replymsg
#define xdr_rmtcall_args	_xdr_rmtcall_args
#define xdr_rmtcallres		_xdr_rmtcallres
#define xdr_rpcb		_xdr_rpcb
#define xdr_rpcb_rmtcallargs	_xdr_rpcb_rmtcallargs
#define xdr_rpcb_rmtcallres	_xdr_rpcb_rmtcallres
#define xdr_rpcblist		_xdr_rpcblist
#define xdr_short		_xdr_short
#define xdr_sizeof		_xdr_sizeof
#define xdr_string		_xdr_string
#define xdr_u_char		_xdr_u_char
#define xdr_u_int		_xdr_u_int
#define xdr_u_long		_xdr_u_long
#define xdr_u_short		_xdr_u_short
#define xdr_uid_t		_xdr_uid_t
#define xdr_union		_xdr_union
#define xdr_unixcred		_xdr_unixcred
#define xdr_vector		_xdr_vector
#define xdr_void		_xdr_void
#define xdr_wrapstring		_xdr_wrapstring
#define xdrmbuf_destroy		_xdrmbuf_destroy
#define xdrmbuf_getbytes	_xdrmbuf_getbytes
#define xdrmbuf_getlong		_xdrmbuf_getlong
#define xdrmbuf_getpos		_xdrmbuf_getpos
#define xdrmbuf_init		_xdrmbuf_init
#define xdrmbuf_inline		_xdrmbuf_inline
#define xdrmbuf_putbytes	_xdrmbuf_putbytes
#define xdrmbuf_putlong		_xdrmbuf_putlong
#define xdrmbuf_setpos		_xdrmbuf_setpos
#define xdrmem_create		_xdrmem_create
#define xdrrec_create		_xdrrec_create
#define xdrrec_endofrecord	_xdrrec_endofrecord
#define xdrrec_eof		_xdrrec_eof
#define xdrrec_skiprecord	_xdrrec_skiprecord
#define xdrstdio_create		_xdrstdio_create
#define xencrypt		_xencrypt
#define xprt_register		_xprt_register
#define xprt_unregister		_xprt_unregister


#ifdef not_so_sure
#define netdir_getbyaddr	_netdir_getbyaddr
#define netdir_getbyname	_netdir_getbyname
#define netdir_options		_netdir_options
#define taddr2uaddr		_taddr2uaddr
#define uaddr2taddr		_uaddr2taddr
#endif

#endif /* __libnsl_synonyms_h__ */
