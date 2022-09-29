#!smake

#ident "$Revision: 1.6 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

CFILES=\
t_accept.c	t_alloc.c	t_bind.c	t_close.c	\
t_connect.c	t_error.c	t_free.c	t_getinfo.c	\
t_getname.c	t_getstate.c	t_listen.c	t_look.c	\
t_open.c	t_optmgmt.c	t_rcv.c 	t_rcvconnect.c	\
t_rcvdis.c	t_rcvrel.c	t_rcvudata.c	t_rcvuderr.c	\
t_snd.c 	t_snddis.c	t_sndrel.c	t_sndudata.c	\
t_sync.c	t_unbind.c	_conn_util.c	_data.c 	\
_data2.c	_dummy.c	_errlst.c	_utility.c	\
_initialize.c	t_getprotaddr.c	t_strerror.c


SUBDIR_CDEFS=-DNO_IMPORT -D_BUILDING_LIBXNET

OTHERCFILES=\
__calloc_def.c	__close_def.c	__errno_def.c	__fcntl_def.c	\
__free_def.c	__getmsg_def.c	__getpmsg_df.c	__ioctl_def.c	\
__memcpy_def.c	__open_def.c	__patch.c	__perror_def.c	\
__putmsg_def.c	__putpmsg_df.c	__sigset_def.c	__strlen_def.c	\
__ulimit_def.c	__write_def.c	


include $(LIBLEAFRULES)
