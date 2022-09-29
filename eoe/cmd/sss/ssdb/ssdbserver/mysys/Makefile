#!smake
# Makefile for mysys under ssdb
#	"$Revision: 1.1 $"
#
OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

SRCDIR = .

BASE = /usr/local
LIBS = -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 -L$(ROOT)/usr/lib32/internal \
	-lnsl -lm  -lsocket  -lcrypt -ldl -lgen -lcurses -lpthread

LCINCS = -nostdinc -I$(SRCDIR)/../include -I$(SRCDIR)/.. -I.. -I$(SRCDIR) -I$(ROOT)/usr/include

LCDEFS  = -DDEFAULT_BASEDIR=\"$(BASE)\" -DDBUG_OFF -DHAVE_CONFIG_H

OPTIMIZER = -O

TARGETS = libmysys.a


LIBMYSYS_OBJS =	my_init.o my_getwd.o mf_getdate.o mf_path.o \
		mf_loadpath.o my_open.o my_create.o my_seek.o my_read.o my_pread.o \
		my_write.o mf_reccache.o mf_keycache.o mf_iocache.o mf_cache.o \
		my_lock.o mf_brkhant.o my_alarm.o my_malloc.o my_realloc.o my_once.o \
		mulalloc.o my_alloc.o safemalloc.o my_fopen.o my_fstream.o my_error.o \
		errors.o my_div.o my_messnc.o mf_format.o mf_same.o mf_dirname.o \
		mf_fn_ext.o mf_pack.o mf_pack2.o mf_unixpath.o mf_stripp.o mf_casecnv.o \
		mf_soundex.o mf_wcomp.o mf_wfile.o mf_qsort.o mf_sort.o mf_radix.o \
		queues.o tree.o list.o hash.o array.o string.o typelib.o my_copy.o \
		my_append.o my_lib.o my_delete.o my_rename.o my_redel.o my_tempnam.o \
		my_chsize.o my_lread.o my_lwrite.o my_clock.o my_quick.o my_lockmem.o \
		my_static.o getopt.o getopt1.o getvar.o my_mkdir.o sgidefault.o \
		my_compress.o

LIBMYSADD_OBJS = thr_alarm.o thr_lock.o thr_mutex.o my_pthread.o my_thr_init.o

COMMONPREF =    SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

libmysys.a : $(LIBMYSYS_OBJS) $(LIBMYSADD_OBJS)
	ar ccurl libmysys.a $(LIBMYSYS_OBJS) $(LIBMYSADD_OBJS)
