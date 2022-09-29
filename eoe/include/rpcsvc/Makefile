#
# Makefile for rpcsvc header file installation.
#
include $(ROOT)/usr/include/make/commondefs

PUBLIC_FILES= \
	bootparam.h \
	ether.h \
	klm_prot.h klm_prot.x \
	mount.h \
	nfs_prot.x nlm_prot.h nlm_prot.x \
	rex.h \
	rquota.h rquota.x rstat.h rusers.h rwall.h \
	sm_inter.h sm_inter.x spray.h \
	yp_prot.h ypclnt.h yppasswd.h ypupdate_prot.h ypv1_prot.h

HPUT=$(INSTALL) -m 444 -F /usr/include/rpcsvc

default: install

include $(COMMONRULES)

install:
	$(HPUT) $(PUBLIC_FILES)
