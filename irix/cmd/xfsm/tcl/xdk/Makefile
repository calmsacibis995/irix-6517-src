#!smake
#
# "$Revision: 1.6 $"

include ../../Makedefs

default lint debug $(COMMONTARGS): $(_FORCE)

PROG	=	dkMgr
XRSRC	=	xfsmDkMgr
SCRIPTS	=	dkInfoDlg dkMPtDlg dkPtDlg dkSrchDlg

install:
	$(INSTALL) -u root -g sys -m 0644 -idb $(CLIENT_TAG) \
			-F $(XDKMDIR) $(SCRIPTS)
	$(INSTALL) -u root -g sys -m 0644 -idb $(CLIENT_TAG) \
			-F $(XAPPSDIR) -src $(XRSRC) Xdkm
	$(INSTALL) -u root -g sys -m 0755 -idb $(CLIENT_TAG) \
			-F $(INSTXFSMBIN) -src $(PROG) xdkm

$(_FORCE):
