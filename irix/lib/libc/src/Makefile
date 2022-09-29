#!smake
#ident "$Revision: 1.16 $"

.NOTPARALLEL:
include $(ROOT)/usr/include/make/commondefs

SUBDIRS=abi aio csu gen inet locale math mdbm mls mp net ns print proc \
	rld rpc sesmgr signal stdio strings timers iconv sys term yp

default install $(COMMONTARGS)!
	$(SUBDIRS_MAKERULE)

${SUBDIRS}!.RECURSIVE

${SUBDIRS}!
	cd $@; exec ${MAKE}
