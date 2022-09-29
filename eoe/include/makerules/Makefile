#!smake
#
# Makefile to install include/make files
#

# $(ROOT)/usr/include/make/commondefs comes from here, so ...
include releasedefs
NO_RELEASEDEFS=yes
include commondefs

INST= $(INSTALL) -F /usr/include/make -m 444
LDIRT=so_locations.dev so_locations so_locations_64.dev so_locations_64 \
	so_locations_N32 so_locations_N32.dev
#
# DEVSOL - versions for ROOT
#
DEVSOL=so_locations.dev so_locations_N32.dev so_locations_64.dev

default: $(DEVSOL)

include commonrules

headers:so_locations so_locations_N32 so_locations_64
	RAWIDB= $(INSTALL) -F /usr/lib -m 644 so_locations
	RAWIDB= $(INSTALL) -F /usr/lib64 -m 644 -src so_locations_64 so_locations
	RAWIDB= $(INSTALL) -F /usr/lib32 -m 644 -src so_locations_N32 so_locations
	RAWIDB= $(INST) startversion commondefs commonrules releasedefs \
		cmdcommondefs cmdcommonrules \
		libleafrules libleafdefs librootdefs \
		librootrules ismcommonrules ismcommondefs \
		cordrules corddefs odefs oodefs o256podefs

#
# Building the BUILD version consists of taking the master version and
# changing the start address from the application start to the SGI start.
# (for 32 bit there is no difference)
#
so_locations so_locations_N32:so_locations.master
	cp so_locations.master $@
	chmod 644 $@

so_locations_64:so_locations_64.master
	sed -n '1,/# Non SGI libraries should be below 0x0000004000000000 (1\/4Tb)/p' so_locations_64.master > so_locations_64
	echo '$$start_address=0x000000000d980000' >> $@
	chmod 644 $@

#
# Building the ROOT version consists of taking the BUILD version and
# changing the mode.
#
$(DEVSOL):$$(@:.dev=)
	cp $(@:.dev=) $@
	chmod 0644 $@

install: default $(_FORCE)
	$(INST) startversion commondefs commonrules releasedefs \
		cmdcommondefs cmdcommonrules \
		libleafrules libleafdefs librootdefs \
		librootrules ismcommonrules ismcommondefs \
		cordrules corddefs odefs oodefs o256podefs

	$(INSTALL) -F /usr/lib -m 444 -src so_locations.master so_locations
	$(INSTALL) -F /usr/lib -m 644 -src so_locations.dev so_locations
	@#
	@# so_locations for 64bit libs
	@#
	$(INSTALL) -F /usr/lib64 -m 444 -src so_locations_64.master so_locations
	$(INSTALL) -F /usr/lib64 -m 644 -src so_locations_64.dev so_locations
	@#
	@# so_locations for N32bit libs
	@#
	$(INSTALL) -F /usr/lib32 -m 444 -src so_locations.master so_locations
	$(INSTALL) -F /usr/lib32 -m 644 -src so_locations_N32.dev so_locations
