#!smake

#ident "$Revision: 1.24 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

ABI_CFILES = \
	blk_init.c \
	_mf_wcs.c _mfutil.c confstr.c \
	iconv_lib.c \
	mbstowcs.c mbtowc.c memccpy.c memchr.c \
	strchr.c \
	strcspn.c strcasecmp.c strdup.c strerror.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strrchr.c \
	strspn.c strstr.c strtok.c \
	wcscoll.c wcstod.c wcstok.c wcstol.c wcstoll.c \
	wcstombs.c wcstoul.c wcstoull.c wcsstr.c \
	wctomb.c wctype.c wcwidth.c  \
	wsspn.c wstostr.c \
	wscat.c wschr.c wscmp.c wscpy.c wscspn.c \
	wslen.c wsncat.c wsncmp.c wsncpy.c wspbrk.c wsrchr.c

#
# Note - memset.s memcmp.s memcpy.s don't exist - they are used to generate
#	 $(OBJECTS)
#
ABI_ASFILES = \
	bcmp.s bzero.s bzero_fp.s index.s rindex.s memcmp.s \
	strcat.s strcmp.s strcpy.s strlen.s

NONABI_CFILES=

NONABI_ASFILES=

#
# Same as below - look for the -mips4 flag rather than OBJECT_STYLE
# This then works for either 64 bit or n32 mips4
#
#if $(CSTYLE:M*-mips4*) != ""
ABI_ASFILES+=bcopy_mips4.s bcopy.s
#else
ABI_ASFILES+=bcopy_fp.s bcopy.s
#endif

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
CFILES= $(ABI_CFILES)
ASFILES=$(ABI_ASFILES)
#else
CFILES= $(ABI_CFILES) $(NONABI_CFILES)
ASFILES=$(ABI_ASFILES) $(NONABI_ASFILES)
#endif

FORCE_MIPS3=-
#if $(LIBRARY) == "libc.a"
# No longer set FORCE_MIPS3 to "+" for libc.a (o32 once again uses -mips2)
#endif
SM=:

#
# A bit of a hack - rather than look at the lib name or OBJECT_STYLE
# we look at the actual flags being set. This way, if someone changes
# the generic "32" style to be mips2, all the right stuff happens
#
#if $(CSTYLE:M*-mips1*) != ""
SM=$(SETMAGIC)
#endif
#if $(CSTYLE:M*-mips2*) != ""
SM2=$(SETMAGIC)
#endif

include $(LIBLEAFRULES)

bcopy.o: bcopy.s
	@if [ "$(FORCE_MIPS3)" = "+" ]  ; then \
	    $(ASF:S/-mips2/-mips3 -o32 -KPIC/) $(NONABI_OK) -c bcopy.s -o $@ ; \
	    $(SETMAGIC) -mips2 $@ ; \
	else  \
	    $(ASF) $(NONABI_OK) -c bcopy.s -o $@ ; \
	fi

bcopy_fp.o: bcopy_fp.s
	$(AS) $(ASFLAGS:S/-mips1/-mips2/:S/-abi//) -c bcopy_fp.s -o $@
	$(SM) -mips1 $@

bcopy_mips4.o: bcopy_mips4.s
	$(ASF) -c bcopy_mips4.s -o $@

memcmp.o: bcmp.s
	$(ASF) -c bcmp.s -o $@

bcmp.o: bcmp.s
	$(ASF) -DISBCMP -c bcmp.s -o $@

bzero.o: bzero.s
	@if [ "$(FORCE_MIPS3)" = "+" ]  ; then \
	    $(ASF:S/-mips2/-mips3 -o32 -KPIC/) $(NONABI_OK) -c bzero.s -o $@ ; \
	    $(SETMAGIC) -mips2 $@ ; \
	else  \
	    $(ASF) $(NONABI_OK) -c bzero.s -o $@ ; \
	fi

bzero_fp.o: bzero_fp.s
	$(AS) $(ASFLAGS:S/-mips1/-mips2/:S/-abi//) -c bzero_fp.s -o $@
	$(SM) -mips1 $@

mbwc_wrap.o: mbwc_wrap.s ../computed_include/mbwc_wrap.h
	$(ASF) -c mbwc_wrap.s -o $@

../computed_include/mbwc_wrap.h: ../computed_include/mbwcoffs.c
	cd ../computed_include; $(MAKE) mbwc_wrap.h

iconv_lib.o: iconv_lib.c
	$(CCF) -o $@ -woff 1209 -c iconv_lib.c

