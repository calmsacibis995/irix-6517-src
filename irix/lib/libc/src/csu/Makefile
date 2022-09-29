#!smake
#
# Makefile for crt? - this doesn't use main rules, etc
#
DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

.NOTPARALLEL:
LDIRT=*.x

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
TARGETS=crt1.o mcrt1.o crtn.o
ABIDEFS=-u _lib_version -u _gp -u _DYNAMIC_LINKING
#elif $(LIBRARY:M?*_nomp_?*.a) != "" || $(LIBRARY:M?*_G0?*) != ""
TARGETS=
#else
TARGETS=crt1.o mcrt1.o crtn.o
#endif

#
# Request for large-GOT for both 32/64 crt1.o & crtn.o
#
XLG_GOT = -xgot

LD_DASHI_OPT=

#if $(VLD) == "UCODE"
LD_DASHI_OPT=-i
#endif

default! $(TARGETS)

include $(COMMONRULES)

crt1.o: crt1text.s crt1tinit.x
	$(ASF) $(XLG_GOT) -c -DCRT0 -o crt1text.x crt1text.s
	$(LD)  $(LDOPTS) -r $(ABIDEFS) -u __xpg4 -u _DYNAMIC_LINK -o $@ crt1text.x ${LD_DASHI_OPT} crt1tinit.x

mcrt1.o: mcrt1text.x mcrt1tinit.x
	$(LD)  $(LDOPTS) -r -u __xpg4 -u _DYNAMIC_LINK -o $@ mcrt1text.x ${LD_DASHI_OPT} mcrt1tinit.x

mcrt1text.x: crt1text.s
	$(ASF) ${XLG_GOT} -c -DMCRT0 -o mcrt1text.x crt1text.s

mcrt1tinit.x: crt1tinit.s
	$(ASF) ${XLG_GOT} -c -DMCRT0 -o mcrt1tinit.x crt1tinit.s

crtn.o: crtninit.x
	$(LD)  $(LDOPTS) -r -o $@ ${LD_DASHI_OPT} crtninit.x

.SUFFIXES: .x
.s.x:
	$(ASF) $(XLG_GOT) -o $@ $<
