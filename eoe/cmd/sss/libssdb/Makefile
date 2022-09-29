#!smake
#
# Makefile for SSDB API's
#

BASELIB = libssdb.a
ALTLIBS = libssdb_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

LIBDIR = /usr/lib32
TARGETS=libssdb_n32_M3.so
DSONAMEOPT=-soname libssdb.so
LLDLIBS= ../ssdb/ssdbserver/client/libmysqlclient.a ../ssdb/ssdbserver/mysys/libmysys.a \
	 ../ssdb/ssdbserver/strings/libmystrings.a ../ssdb/ssdbserver/dbug/libdbug.a \
	 -lpthread -lc
LCINCS=../ssdb/ssdbserver/include
LD_NO_UNRESOLVED=
LD_QUICKSTART_INFO=
LDIRT=*.a
LLDDSOOPTS_32=-woff 85,69
LLDDSOOPTS_N32=-woff 85,69

default: $(TARGETS)

include $(LIBROOTRULES)

install exports: default
	$(INSTALL) -idb stripdso -F /usr/lib32/internal -src libssdb_n32_M3.so libssdb.so
