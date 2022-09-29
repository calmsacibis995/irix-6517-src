#ident	"$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

CMDS=check_abi_interface check_for_syscalls check_abi_compliance
ENTRYPOINTS= libc.so.1.names libdl.so.names libmutex.so.names libnsl.so.names \
	libsocket.so.names libxnet.so.names libX11.so.2.names
RQRDNAMES=required.names
SYSCALLNAMES=syscall.names

TARGETS=

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(CMDS)
	$(INSTALL) -m 444 -F /usr/lib/abi/data $(ENTRYPOINTS)
	$(INSTALL) -m 444 -F /usr/lib/abi/data $(RQRDNAMES)
	$(INSTALL) -m 444 -F /usr/lib/abi/data $(SYSCALLNAMES)
