#!smake

#ident "$Revision: 1.12 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
ASFILES= rld_bridge.s
#elif $(LIBRARY:M?*_ns.a) != "" || $(LIBRARY:M?*_noso?*) != ""
CFILES=_rld_new_interface.c
#else
CFILES= _rld_new_interface.c elfmap.c get_rld.c libdl.c libdl_tp.c missing_function.c
ASFILES= rld_bridge.s missing_function_trap.s
#endif
SUBDIR_CDEFS=-DLIBC_ELFMAP


include $(LIBLEAFRULES)
