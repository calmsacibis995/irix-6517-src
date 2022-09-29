#!make
#

include $(ROOT)/usr/include/make/commondefs

TARGETS	  = iconv_comp
OBJECTS	  = in_proc.o iconv_compy.o iconv_compl.o iconv_comp.o
CFILES    = iconv_comp.c in_proc.c iconv_compl.c iconv_compy.c
YFILES= iconv_compy.y
LFILES= iconv_compl.l

COMMONPREF = iconv_

SUBDIRS   = genxlt ucs_map

targets: $(TARGETS) 

all default install $(COMMONTARGS):$(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(COMMONRULES)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(COMMONPREF)default $(COMMONPREF)all: $(TARGETS)

$(COMMONPREF)install: $(COMMONPREF)default $(COMMONPREF)all
	$(INSTALL) -F /usr/sbin -m 755 $(IDB_TAG32) iconv_comp
	$(INSTALL) -m 444 -F /usr/lib/iconv/spec -idb \
		'exitop("cd $$rbase/usr/lib/iconv/spec; $$rbase/usr/sbin/iconv_comp -f ../iconvtab iconv.spec*")' iconv.spec.core iconv.speczz_mbwc


iconv_comp: $(OBJECTS)
	cc -o iconv_comp $(OBJECTS) $(LDFLAGS)

iconv_compl.c:	iconv_compl.l y.tab.h

# parallelism problem.  Only one can run at a time.
iconv_compy.c: y.tab.h
iconv_compy.c y.tab.h: iconv_compy.y
	$(YACCF) -d iconv_compy.y
	mv y.tab.c iconv_compy.c

