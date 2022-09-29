#!smake
#
# Makefile for libgen.
#
#ident	"$Revision: 1.2 $"

DEPTH=.
CUR_CFILES=basename.c bgets.c bufsplit.c copylist.c dirname.c eaccess.c \
	  gmatch.c isencrypt.c mkdirp.c p2open.c pathfind.c \
	  reg_compile.c reg_step.c regcmp.c regex.c rmdirp.c strccpy.c \
	  strecpy.c strfind.c strrspn.c strtrns.c

CUR_CFILES_VFORK=basename.c bgets.c bufsplit.c copylist.c dirname.c eaccess.c \
	  gmatch.c isencrypt.c mkdirp.c p2open.c pathfind.c \
	  reg_compile.c reg_step.c regcmp.c regex.c rmdirp.c strccpy.c \
	  strecpy.c strfind.c strrspn.c strtrns.c vfork.c

include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY) == "libgen_32_abi.a" || \
    $(LIBRARY) == "libgen_64_abi.a"
CFILES= $(CUR_CFILES)
#else
CFILES= $(CUR_CFILES_VFORK)
#endif

include $(LIBLEAFRULES)

