#
# Makefile for xfsm/lib
#
# "$Revision: 1.15 $"

DEPTH=..
include $(DEPTH)/Makedefs
include $(ROOT)/usr/include/make/librootdefs
OBJECT_STYLE=$(BUILD_OBJECT_STYLE)


LIBSERV_FILES=query/xfs_xlv_query.c query/xfs_disk_query.c \
		query/xfs_fs_query.c query/xfs_get_obj.c \
		utils/xfs_get_info.c fs/xfs_fs_clnt.c  \
		query/xfs_query.c utils/xfs_group.c utils/group.c \
		utils/xfs_get_hosts.c utils/gethosts.c \
		utils/xfs_debug.c disk/dvhpart.c rpc/xfs_rpc_defs_svc.c\
		rpc/xfs_rpc_defs_xdr.c fs/xfsfuncs.c utils/xfs_popen.c \
		fs/xfsutils.c xlv/xlv_cmd.c xlv/xlv_funcs.c xlv/xlv_int.c \
		xlv/table.c utils/xfs_addbuf.c \
		utils/xfs_gen.c

LIBSERV_OBJS = $(LIBSERV_FILES:.c=.o)

LIBCLNT_FILES= rpc/xfs_info_client.c rpc/xfs_rpc_defs_clnt.c \
		rpc/xfs_rpc_defs_xdr.c utils/xfs_gen.c utils/xfs_addbuf.c

LIBCLNT_OBJS	= $(LIBCLNT_FILES:.c=.o)
SUBDIRS		= disk fs query rpc utils xlv
TARGETS		= libxfsmserv.a libxfsmclnt.so

.ORDER:	libxfsmserv.a $(LIBCLNT_FILES)

default:	$(TARGETS)

install:	default
		$(INSTALL) -u root -g sys -m 0644 -F $(INSTXFSMLIB) libxfsmclnt.so
		$(INSTALL) -F /usr/lib -lns $(INSTXFSMLIB)/libxfsmclnt.so libxfsmclnt.so

clean:
		$(SUBDIRS_MAKERULE)

clobber:
		$(SUBDIRS_MAKERULE)
		rm -f $(TARGETS) libxfsmclnt.a

libxfsmserv.a:	$(SUBDIRS)
		$(AR) r libxfsmserv.a $(LIBSERV_OBJS)

libxfsmclnt.so:	libxfsmserv.a $(LIBCLNT_FILES)
		$(AR) r libxfsmclnt.a $(LIBCLNT_OBJS)
		$(LD) -elf -shared -no_unresolved -rdata_shared \
			-$(BUILD_OBJECT_STYLE) \
			-o libxfsmclnt.so -all libxfsmclnt.a -lc

$(SUBDIRS):	$(_FORCE)
		cd $@; $(MAKE)
# 1998.10.15 kjw@engr - due to the nature of the libxfsmserv.a:
# $(SUBDIRS) dependency, the utils:rpc dependency needs to be
# specified, otherwise there is a parallel race condition where utils
# needs a header file from the rpc subdirectory.
utils: rpc

$(_FORCE):
