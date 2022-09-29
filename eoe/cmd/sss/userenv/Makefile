#!smake

include $(ROOT)/usr/include/make/commondefs

IDB_TAG = -idb SSS_EOE

FTR_DST_PATH  = /usr/lib/filetype/install
FTI_DST_PATH  = $(FTR_DST_PATH)/iconlib
LAUNCHER_PATH = /usr/sbin
ESPURL_PATH   = /var/esp/ssc
ICONCAT_PATH  = /usr/lib/desktop/iconcatalog/pages/C/SupportTools
ICONCAT_ICONNAME = "Embedded_Support_Partner"


default: ESPartner.fti ESPartner.ftr ESPartnerURL launchESPartner


install: default
	$(INSTALL) $(IDB_TAG) -m 444 -F $(FTI_DST_PATH)       ESPartner.fti
	$(INSTALL) $(IDB_TAG) -m 555 -F $(LAUNCHER_PATH)      launchESPartner
	$(INSTALL) $(IDB_TAG) -m 444 -F $(ESPURL_PATH)        ESPartnerURL
	${INSTALL} $(IDB_TAG)        -F $(ICONCAT_PATH) -lns  $(ESPURL_PATH)/ESPartnerURL $(ICONCAT_ICONNAME)
	$(INSTALL) $(FTR_EXITOP_IDB) $(IDB_TAG) -m 444 -F $(FTR_DST_PATH)  ESPartner.ftr

include $(COMMONRULES)
