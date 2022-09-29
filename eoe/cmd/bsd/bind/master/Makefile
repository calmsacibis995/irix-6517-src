# $Revision: 1.332 $

include $(ROOT)/usr/include/make/commondefs

NDIR= /var/named
INST= $(INSTALL) -idb "std.sw.named"

EXAMPLES= named.boot.slave named.boot.master named.hosts \
	localhost.rev named.rev root.cache

MKFILES	=subfoo.soa subfoo.hosts.soa local.make.script
MKSCRIPT=mkdns

all default:

install: default
	$(INST) -F /etc -lns ../var/named named.d
	$(INST) -F /usr/etc -lns ../../var/named named.d
	$(INST) -m 755 -F $(NDIR) -src named.inst.sh -idb \
		'postop($$rbase/var/named/.named.inst.sh)' .named.inst.sh
	$(INST) -m 444 -F $(NDIR)          README
	$(INST) -m 444 -F $(NDIR)/Examples $(EXAMPLES)
	$(INST) -m 444 -F $(NDIR)/mkdns    $(MKFILES)
	$(INST) -m 755 -F $(NDIR)/mkdns -idb 'config(suggest)' $(MKSCRIPT)

include $(COMMONRULES)
