#!smake

#ident "$Revision: 1.21 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

ABI_CFILES=doprnt.c dogrp.c fprintf.c inline_doprnt.c printf.c \
	sprintf.c vfprintf.c vprintf.c vsprintf.c

NONABI_CFILES=snprintf.c vsnprintf.c

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)
#endif

include $(LIBLEAFRULES)
