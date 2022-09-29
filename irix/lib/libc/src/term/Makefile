#!smake

#ident "$Revision: 1.6 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

ABI_CFILES= cfgetispeed.c cfgetospeed.c cfsetispeed.c cfsetospeed.c \
	isatty.c tcdrain.c tcflow.c tcflush.c tcgetattr.c \
	tcgetpgrp.c tcgetsid.c tcsendbreak.c tcsetattr.c tcsetpgrp.c \
	ttyname.c ttyslot.c

NONABI_CFILES=new_cfgetispeed.c new_cfgetospeed.c \
	new_cfsetispeed.c new_cfsetospeed.c \
	new_tcgetattr.c new_tcsetattr.c

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)
#endif

#
# files here use _OLD_TERMIOS to maintain binary compatibility with
# applications, except for new_* which undefine _OLD_TERMIOS 
# manually
#
LCDEFS += -D_OLD_TERMIOS

include $(LIBLEAFRULES)
