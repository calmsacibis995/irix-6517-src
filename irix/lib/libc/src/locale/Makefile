#!smake
#ident "$Revision: 1.10 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#
# the old_* files are only for shared C backward compat support
#
NOTNEEDED=old_tolower.c old_toupper.c

ABI_CFILES=\
	_ctype.c _loc_data.c catclose.c catgets.c \
	catopen.c colldata.c ctypefcns.c \
	fmtmsg.c gtxt.c gettxt.c localeconv.c nl_langinfo.c \
	pfmt_data.c setlabel.c setlocale.c strfmon.c strftime.c strxfrm.c \
	tolower.c toupper.c wcsftime.c

NONABI_CFILES= \
	_set_tab.c _locale.c \
	addsev.c cat_init.c \
	catgetmsg.c catmsgfmt.c \
	ctype_.c \
	lfmt.c lfmt_log.c \
	nl_error.c nl_query.c nl_usage.c \
	pfmt.c pfmt_print.c \
	_shrd_data.c setchrclass.c \
	sgi_dofmt.c sgi_sfmtmsg.c sgi_ffmtmsg.c \
	vpfmt.c vlfmt.c 

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)
#endif

include $(LIBLEAFRULES)
