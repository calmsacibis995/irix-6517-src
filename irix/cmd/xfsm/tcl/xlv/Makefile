#!smake
#
# "$Revision: 1.5 $"

include ../../Makedefs

default lint debug $(COMMONTARGS): $(_FORCE)

PROG	=	vmMgr
XRSRC	=	xfsmXlvMgr
SCRIPTS	=	vmAttDlg vmDetDlg vmInfoDlg vmMkPlxDlg vmMkVeDlg vmMkVePnl \
		vmMkVolDlg vmPlxGraphPnl vmPlxSumPnl vmPlxTblPnl vmPtPnl \
		vmSrchDlg vmSrchPtDlg vmVeInfoDlg vmVolSumPnl

install:
	$(INSTALL) -u root -g sys -m 0644 -idb $(CLIENT_TAG) \
			-F $(XLVMDIR) $(SCRIPTS)
	$(INSTALL) -u root -g sys -m 0644 -idb $(CLIENT_TAG) \
			-F $(XAPPSDIR) -src $(XRSRC) Xlvm
	$(INSTALL) -u root -g sys -m 0755 -idb $(CLIENT_TAG) \
			-F $(INSTXFSMBIN) -src $(PROG) xlvm

$(_FORCE):
