/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/fw_lib.h,v 1.2 1996/07/06 17:43:12 nn Exp $"

#ifndef	_FW_LIB_H
#define	_FW_LIB_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __STDC__

extern short net_invoke(char _TKFAR *, char _TKFAR *, char _TKFAR *,
	char _TKFAR *, u_long, u_long, char _TKFAR *,
    invk_context, Op_arg _TKFAR * _TKFAR *, Op_err _TKFAR * _TKFAR *, ...);
extern short net_more(u_long, u_long, invk_context, Op_arg _TKFAR * _TKFAR *,
	Op_err _TKFAR * _TKFAR *);
extern short net_end(u_long, invk_context, Op_err _TKFAR * _TKFAR *);
extern Op_arg _TKFAR *net_arg_init(void);
extern short net_arg_set(char _TKFAR *, ...);
extern short net_arg_markrow(void);
extern short net_arg_get(Op_arg _TKFAR *, char _TKFAR *,
    char _TKFAR * _TKFAR *);
extern short net_arg_getnext(Op_arg _TKFAR *, char _TKFAR * _TKFAR *,
	char _TKFAR * _TKFAR *);
extern short net_arg_nextrow(Op_arg _TKFAR *);
extern short net_arg_rowstart(Op_arg _TKFAR *);
extern short net_arg_reset(Op_arg _TKFAR *);
extern void net_arg_free(Op_arg _TKFAR *);
extern void net_err_free(Op_err _TKFAR *);
extern Op_arg _TKFAR *new_Op_arg(void);
extern void free_Op_arg(Op_arg _TKFAR *);
extern void free_Op_err(Op_err _TKFAR *);
extern short append_Op_arg(Op_arg _TKFAR *, char _TKFAR *, char _TKFAR *);
extern void fw_err_set(Op_err _TKFAR * _TKFAR *, Fw_err, u_long, ...);

#ifdef _WINDOWS
extern void net_cleanup(void);
#endif

#else

extern short net_invoke();
extern short net_more();
extern short net_end();
extern Op_arg _TKFAR *net_arg_init();
extern short net_arg_set();
extern short net_arg_markrow();
extern short net_arg_get();
extern short net_arg_getnext();
extern short net_arg_nextrow();
extern short net_arg_rowstart();
extern short net_arg_reset();
extern void net_arg_free();
extern void net_err_free();
extern Op_arg _TKFAR *new_Op_arg();
extern void free_Op_arg();
extern void free_Op_err();
extern short append_Op_arg();
extern void fw_err_set();

#ifdef _WINDOWS
extern void net_cleanup();
#endif

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* !_FW_LIB_H */
