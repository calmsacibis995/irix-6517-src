include $(ROOT)/usr/include/make/commondefs

default: MAKEDEV

include $(COMMONRULES)

#
# When installing new elements in the MAKEDEV.d directory, please ensure
# that the last of them gets tagged with the MAKEDEV post op...  The last
# one is determined by the sort order of the idb file...
#
install: default
	$(INSTALL) -dir /dev
	$(INSTALL) -dir /hw
	$(INSTALL) -dir /dev/abi
	$(INSTALL) -idb 'config(update) preop("cd $$rbase/dev;find . -type d -print|xargs chmod g-w 2>&- ; exit 0") postop("cd $$rbase/dev; ENV= ./MAKEDEV > /dev/null")' -F /dev MAKEDEV
	$(INSTALL) -dir /dev/MAKEDEV.d
	@# null out MAKEFLAGS since old (4.0) make doesn't grok -N
	MAKEFLAGS= INSTALL=$(INSTALL) /bin/make -f MAKEDEV install
