#!smake

#ident "$Revision: 1.44 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

#if $(LIBRARY:M?*_ns.a) != "" && \
    $(LIBRARY) != "libc_nomp_32_M2_ns.a" && \
    $(LIBRARY) != "libc_nomp_n32_M3_ns.a"
# a performance hack for specmarks
# by compiling data.c -G6000, the __iob struct goes into .sdata
#SUBDIR_CDEFS=-G6000
#endif

ABI_CFILES = \
	cuserid.c ctermid.c data.c datar.c sobuf.c sibuf.c clearerr.c \
	_filbuf.c fileno.c _flsbuf.c fdopen.c fgetc.c fgets.c fgetws.c \
	feof.c ferror.c \
	flush.c fopen.c fputc.c fputs.c fputws.c fread.c fseek.c ftell.c \
	fwrite.c fpos.c \
	getc.c getchar.c getpass.c gets.c getw.c \
	mktemp.c mkstemp.c popen.c putc.c putchar.c puts.c putw.c \
	rewind.c scanf.c setbuf.c setvbuf.c system.c system_tp.c \
	tempnam.c tmpfile.c tmpnam.c ungetc.c \
	fgetwc.c fputwc.c getwchar.c putwchar.c ungetwc.c

NONABI_CFILES=\
	_findbuf.c _wrtchk.c \
	doscan.c getws.c \
	inline_doscan.c \
	putws.c \
	setbuffer.c

ABI_32CFILES= fpos64.c fseek64.c ftell64.c

#if $(LIBRARY) == "libc_32_abi.a"
CFILES= $(ABI_CFILES) $(ABI_32CFILES)
#elif $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)

#if $(OBJECT_STYLE:M32*) != ""
CFILES+=fpos64.c
#endif

#if $(OBJECT_STYLE:M32*) != "" || $(OBJECT_STYLE:MN32*) != ""
CFILES+=fseek64.c ftell64.c
#endif
#endif

include $(LIBLEAFRULES)
