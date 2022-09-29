#!smake
#ident "$Revision: 1.45 $"

DEPTH	= .
CFILES  = irix.c indom.c pmda.c hinv.c hinv-helper.c \
	  aio.c alldk.c cntrl.c disk.c engr.c evctr.c filesys.c \
	  getblkstats.c hubstat.c hwrouter.c if.c igetstats.c kmemread.c \
	  kna.c lpage.c minfo.c msg.c ncstats.c nfs3_client.c \
	  nfs3_server.c nfs_client.c nfs_server.c nodeinfo.c null.c \
	  numa_node.c rpc_client.c rpc_server.c sem.c shm.c shm_kmem.c \
	  socket.c spindle.c swap.c syserr.c sysinfo.c sysinfo_mp.c sysmp.c \
	  var.c vnodestats.c xbow.c xfs.c xlv.c xpc.c stream.c

include $(ROOT)/usr/include/make/releasedefs
include versiondefs
COMPILATION_MODEL       = $(OBJECT_STYLE)
include $(ROOT)/usr/include/make/kcommondefs
include $(ROOT)/usr/include/make/libleafdefs
include $(LIBLEAFRULES)

LIBRARY_CDEFS	+= $(PGSZOPT)
LDIRT		= foo.* foo eek.* audit hinv-helper.c pmda_version.h

audit:	audit.c
	$(CCF) -o audit $(CDEFS) -nostdinc -I$(ROOT)/usr/include audit.c

spindle:  
	$(CCF) -o $@ -DPCP_SPINDLE_TESTING spindle.c -ldisk -lpcp


pmda.c kmemread.c: pmda_version.h
indom.c filesys.c: filesys.h

pmda_version.h:
	rm -f pmda_version.h
	echo "static char *buildversion = \"`$(TOOLROOT)/usr/sbin/mkversionnum \
	    -r "$(RELEASE_NUM)" -b "$(BUILDER)" -t "$(TREE_ID)"`\";" >pmda_version.h

hinv-helper.c:	mk-hinv-helper
	./mk-hinv-helper -DIRIX$(RELEASE_MAJOR)_$(RELEASE_MINOR)

foo:
	echo $(PGSZOPT)
