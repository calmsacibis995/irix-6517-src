#!smake

DEPTH=.

CFILES= mdbm_atomic.c mdbm_chain.c mdbm_repair.c mdbm_support.c mdbm_util.c 
VERSION_COPTS= -fullwarn
LDIRT=*.a *.so

# LCDEFS= -DDEBUG

MDBMCORE=../../../../irix/lib/libc/src/mdbm

VERSION_CINCS = -I$(MDBMCORE)
VPATH= $(MDBMCORE)

#if defined(STANDALONE)
CFILES += mdbm.c hash.c
#endif

SUBDIR_COPTS=-D_SGI_MP_SOURCE

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)
