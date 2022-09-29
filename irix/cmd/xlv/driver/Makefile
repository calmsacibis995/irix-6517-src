#!smake
#ident "$Revision: 1.17 $"
#
# User-level test for xlv driver.
#
include $(ROOT)/usr/include/make/commondefs
WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I../include -I$(KERN) -I$(KERN)/io/xlv

DRV_CFILES = drv_test.c xlv_mk_1.c xlv_driver_stub.c ../mkxlv/lex.yy.c
CPY_CFILES = cpy_test.c xlv_mk_2.c ../mkxlv/lex.yy.c
CFILES = DRV_CFILES CPY_CFILES $(KERN)/io/xlv/xlv_ktab.c $(KERN)/io/xlv/xlv.c $(KERN)/io/xlv/xlv_merge.c $(KERN)/io/xlv/xlv_block_map.c
LEXFILES = xlv.lex
DRV_OBJECTS = $(DRV_CFILES:.c=.o) xlv.o xlv_block_map.o
CPY_OBJECTS = $(CPY_CFILES:.c=.o) xlv_ktab.o xlv_merge.o xlv_block_map.o
OBJECTS = DRV_OBJECTS CPY_OBJECTS

REGOPTS = -fullwarn -D_SGI_SOURCE -DUSER_MODE
LCOPTS = $(REGOPTS)
LLDLIBS = -lxlv -ll -ldisk
LLDOPTS = -L$(LIB)
DEBUG = -g -DDEBUG

TARGETS = cpin drv

default install: $(TARGETS)

include $(COMMONRULES)

lex.yy.c: ../mkxlv/xlv.lex
	$(LEX) ../mkxlv/xlv.lex

xlv.o: $(KERN)/io/xlv/xlv.c
	$(CCF) -c -D_KERNEL -I. $(KERN)/io/xlv/xlv.c -o $@

xlv_ktab.o: $(KERN)/io/xlv/xlv_ktab.c
	$(CCF) -c -D_KERNEL -D_XLV_TAB_SUBVOL_SIZE $(KERN)/io/xlv/xlv_ktab.c -o $@

xlv_merge.o: $(KERN)/io/xlv/xlv_merge.c
	$(CCF) -c -D_KERNEL $(KERN)/io/xlv/xlv_merge.c -o $@

xlv_block_map.o: $(KERN)/io/xlv/xlv_block_map.c
	$(CCF) -c $(KERN)/io/xlv/xlv_block_map.c -o $@

drv: $(DRV_OBJECTS) xlv.o $(LIB)/libxlv.a
	$(CCF) $(DRV_OBJECTS) $(LDFLAGS) -o $@

cpin: $(CPY_OBJECTS) xlv_ktab.o $(LIB)/libxlv.a
	$(CCF) $(CPY_OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
