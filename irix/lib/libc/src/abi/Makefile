#!smake

#ident "$Revision: 1.9 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

ABI_CFILES= abi_getpwnam.c abi_catchall.c abi_grp.c

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
#else
CFILES=
#endif

include $(LIBLEAFRULES)
