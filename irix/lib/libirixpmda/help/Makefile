#!smake
#
# $Id: Makefile,v 1.27 1999/04/28 07:02:32 tes Exp $
#

include $(ROOT)/usr/include/make/commondefs

TARGETS		= help.dir help.pag
HELPFILES	= minfo.help sysinfo.help sysinfo_mp.help disk.help \
		rpc_client.help rpc_server.help nfs_client.help \
		nfs_server.help nfs3_client.help nfs3_server.help \
		swap.help kna.help sysmp.help var.help syserr.help \
		if.help ncstats.help getblkstats.help vnodestats.help \
		igetstats.help filesys.help shm.help sem.help msg.help \
		xfs.help hinv.help hw.help engr.help kaio.help hwrouter.help \
		numa.help nodeinfo.help lpage.help pmda.help xbow.help \
		socket.help hubstat.help xlv.help stream.help

LDIRT		= *.pag *.dir
IDBTAG		= PCP_EOE
PMDADIR		= /var/pcp/pmdas/irix

default: $(TARGETS)

install: default
	$(INSTALL) -f $(PMDADIR) -m 444 -idb $(IDBTAG) help.pag
	$(INSTALL) -f $(PMDADIR) -m 444 -idb $(IDBTAG) help.dir

help.dir:	help.pag
help.pag: 	$(HELPFILES) ../root_irix
	rm -f help.dir help.pag
	$(ROOT)/usr/pcp/buildtools/newhelp -v 2 -n ../root_irix -o help $(HELPFILES)

clean clobber:
	rm -f $(LDIRT)

exports headers:
