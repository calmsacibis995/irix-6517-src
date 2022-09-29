#!smake
#
# top level Makefile for:
#	rfind client (rfind), server (rfindd) and dumper (fsdump).

include ${ROOT}/usr/include/make/commondefs

VARIANTS=lib.32bit lib.64bit fsdump.32bit fsdump.64bit rfindd.32bit rfindd.64bit
SUBDIRS=$(VARIANTS) lib fsdump rfindd rfind config init.d rotatelogs

LDIRT=$(VARIANTS)

default install clean rmtargets fluff tags: $(VARIANTS)
	$(SUBDIRS_MAKERULE)

clobber:
	$(SUBDIRS_MAKERULE)
	rm -fr $(LDIRT)

$(VARIANTS)::
	test -d $@ || mkdir $@; cd ./$(@:R); p_list -a | cpio -pul ../$@
	rm -f $@/Makefile; ln $@/Makefile.$(@:E) $@/Makefile
