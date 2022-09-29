#!smake

#ident "$Revision: 1.4 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
ASFILES=
CFILES=
#else
ASFILES=sgi_sesmgr.s
CFILES= sesmgr_t6api.c sesmgr_t6net.c
#endif

include $(LIBLEAFRULES)
