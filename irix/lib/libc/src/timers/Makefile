#!smake

#ident "$Revision: 1.3 $"

DEPTH=..

include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES=
ASFILES=
#else
CFILES=timers.c
ASFILES=timer_settime.s timer_gettime.s timer_create.s \
	timer_getoverrun.s timer_delete.s

#endif

include $(LIBLEAFRULES)
