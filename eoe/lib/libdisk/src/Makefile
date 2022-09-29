#!smake
#
#ident	"$Revision: 1.13 $"
#

DEPTH=.
CFILES=aliases.c diskname.c diskinfo.c nextdisk.c diskheader.c superblk.c \
	pathinfo.c bigio.c diskinventory.c mountinfo.c disk_stats.c \
	disk_blksz.c remmed.c

include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)

# turns out the 7.1 linker thinks that -non_shared and -r are
# mutually exclusive.
RELOC_OPTS != echo $(LDOPTS) | sed 's/-non_shared//'

remmed.o: remmed.c
	$(CC) $(CFLAGS) -c $< && \
	$(LD) $(RELOC_OPTS) $(INCDIRS) $(LIBGNUM)  -r $(OBJST) $*.o -o $$$$.o \
	&& mv $$$$.o $*.o

