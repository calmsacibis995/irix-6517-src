#!smake
#
# $Id: Makefile,v 1.16 1997/12/11 04:45:49 markgw Exp $
#

PCP_SRC_DEPTH = ../..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

IDBTAG		= PCP_EOE_MON
BASEFTIS	= pcp.archive.fti pcp.folio.fti pcp.logger.fti \
		  pcp.pmcd.fti pcp.pmda.fti pcp.pmns.fti
ALLFTIS		= $(BASEFTIS) pcp.oview.fti pcp.tty.fti
ABOUTEXITOP	= `cat exitop.AboutPCP`
OVIEWEXITOP	= `cat exitop.oview`
TTYAPPEXITOP	= `cat exitop.tty`

#if $(ISM_NAME) != "irix"
# this macro is defined in commondefs in kudzu
FTR_EXITOP_IDB= -idb 'exitop( " \
      if [ -r \$$rbase/usr/lib/filetype/Makefile ]; then \
        chroot $$rbase /bin/sh -c \"cd usr/lib/filetype; make > /dev/null\"; \
      fi" )'
#endif

default:	$(ALLFTIS) pcpirix.ftr AboutPCP pcp.chest

include $(COMMONRULES)

install: default
#if $(BEFORE_IRIX6_5)
	$(INSTALL) -F /usr/lib/X11/app-chests -m 444 -idb "$(IDBTAG)" pcp.chest
#endif
	$(INSTALL) -F /var/pcp/config/desktop -m 444 \
				-idb "$(IDBTAG) $(ABOUTEXITOP)" AboutPCP
	$(INSTALL) -F /usr/lib/filetype/install/iconlib -m 444 \
				-idb "$(IDBTAG)" $(BASEFTIS)
	$(INSTALL) -F /usr/lib/filetype/install/iconlib -m 444 \
				-idb "$(IDBTAG) $(OVIEWEXITOP)" pcp.oview.fti
	$(INSTALL) -F /usr/lib/filetype/install/iconlib -m 444 \
				-idb "$(IDBTAG) $(TTYAPPEXITOP)" pcp.tty.fti
	$(INSTALL) -F /usr/lib/filetype/install -m 444 \
				-idb $(IDBTAG) $(FTR_EXITOP_IDB) pcpirix.ftr
