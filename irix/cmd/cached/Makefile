include $(ROOT)/usr/include/make/commondefs

TARGETS=cached
LCOPTS=-fullwarn

# XXX install on IP28 and IP30 when cached frees up resources between runs.
# IDBTAG_ON=-idb "mach(CPUBOARD=IP25 CPUBOARD=IP27)"
# IDBTAG_OFF=-idb "mach(CPUBOARD=IP28 CPUBOARD=IP30 CPUBOARD=IP32)"

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "mach(CPUARCH=R10000)" -F /usr/diags/bin $(TARGETS)
#	$(INSTALL) $(IDBTAG_ON) -m 644 -F /etc/config -src config/cached -O cached
#	$(INSTALL) $(IDBTAG_OFF) -m 644 -F /etc/config -src config/cached.off -O cached

