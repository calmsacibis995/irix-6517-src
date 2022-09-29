#!smake
#

include $(ROOT)/usr/include/make/commondefs

TARGETS	  = genxlt_2sbcs genxlt_mo genxlt_sbcs2ucs genxlt_ucs2dbcs genxlt_ucs2euc genxlt_xkeys2ucs4 genxlt_ucs42xkeys

CFILES    = genxlt_2sbcs.c genxlt_mo.c genxlt_sbcs2ucs.c genxlt_ucs2dbcs.c genxlt_ucs2euc.c genxlt_xkeys2ucs4.c genxlt_ucs42xkeys.c

LCINCS    = -I../include

targets: $(TARGETS)

include $(COMMONRULES)

all default: $(TARGETS)

install: all default
	$(INSTALL) -F /usr/bin -idb TOOLS $(TARGETS)

genxlt_sbcs2ucs: genxlt_sbcs2ucs.o
	cc -o genxlt_sbcs2ucs genxlt_sbcs2ucs.o

genxlt_sbcs2ucs.o: genxlt_sbcs2ucs.c
	cc -c  $(INCLUDES) genxlt_sbcs2ucs.c

genxlt_ucs2dbcs: genxlt_ucs2dbcs.o
	cc -o genxlt_ucs2dbcs genxlt_ucs2dbcs.o

genxlt_ucs2dbcs.o: genxlt_ucs2dbcs.c
	cc -c  $(INCLUDES) genxlt_ucs2dbcs.c

genxlt_ucs2euc: genxlt_ucs2euc.o
	cc -o genxlt_ucs2euc genxlt_ucs2euc.o

genxlt_ucs2euc.o: genxlt_ucs2euc.c
	cc -c  $(INCLUDES) genxlt_ucs2euc.c

genxlt_2sbcs: genxlt_2sbcs.o
	cc -o genxlt_2sbcs genxlt_2sbcs.o

genxlt_2sbcs.o: genxlt_2sbcs.c
	cc -c  $(INCLUDES) genxlt_2sbcs.c

genxlt_mo: genxlt_mo.o
	cc -o genxlt_mo genxlt_mo.o

genxlt_mo.o: genxlt_mo.c
	cc -c  $(INCLUDES) genxlt_mo.c

genxlt_xkeys2ucs4: genxlt_xkeys2ucs4.o
	cc -o genxlt_xkeys2ucs4 genxlt_xkeys2ucs4.o

genxlt_xkeys2ucs4.o: genxlt_xkeys2ucs4.c
	cc -DHASH_NUM_FACTOR=1.3 -c  $(INCLUDES) genxlt_xkeys2ucs4.c

genxlt_ucs42xkeys: genxlt_ucs42xkeys.o
	cc -o genxlt_ucs42xkeys genxlt_ucs42xkeys.o

genxlt_ucs42xkeys.o: genxlt_ucs42xkeys.c
	cc -c  $(INCLUDES) genxlt_ucs42xkeys.c
