#!smake
#ident "$Revision: 1.47 $"

DEPTH=.
SM=:
include $(ROOT)/usr/include/make/libleafdefs

#if defined(LIBCPR_INTF_ONLY)
ASFILES=
#else
ASFILES= \
	psema_cntl.s	\
	sproc.s
#endif

#if defined(LIBCPR_INTF_ONLY)
CFILES=	ckpt_intf.c
#else
CFILES= ckpt_array.c ckpt_attr.c ckpt_create.c ckpt_dump.c ckpt_file.c \
	ckpt_misc.c ckpt_numa.c ckpt_obj.c ckpt_pathname.c ckpt_remove.c \
	ckpt_revision.c ckpt_restart.c ckpt_savemem.c ckpt_signal.c \
	ckpt_special.c ckpt_stat.c ckpt_stop.c ckpt_shmsys.c
#endif

LCOPTS+=-no_unresolved -g3
#
# 1209 to turn off constant expression warnings, 1233 due o lm_client.h
# incomplete function protoypes
# 
LWOFF=,1209,1233
LCDEFS+=-D_KMEMUSER -DCKPT -DNDEBUG -DLIBPW_WAR -DNS_WAR

include $(LIBLEAFRULES)

