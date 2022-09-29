#!smake
#
# "$Revision: 1.5 $"

include ../../Makedefs

default lint debug $(COMMONTARGS): $(_FORCE)

PROG	=	fsMgr
XRSRC	=	xfsmFsMgr
SCRIPTS	=	advEfsPnl advNfsPnl advXfsPnl exportDlg exportPnl fsInfoDlg \
		fsMntDlg fsSrchDlg fsSrchVpDlg mkfsDlg mntPtPnl unexportDlg

install:
	$(INSTALL) -u root -g sys -m 0644 -idb $(CLIENT_TAG) \
			-F $(XFSMDIR) $(SCRIPTS)
	$(INSTALL) -u root -g sys -m 0644 -idb $(CLIENT_TAG) \
			-F $(XAPPSDIR) -src $(XRSRC) Xfsm
	$(INSTALL) -u root -g sys -m 0755 -idb $(CLIENT_TAG) \
			-F $(INSTXFSMBIN) -src $(PROG) xfsm

$(_FORCE):
