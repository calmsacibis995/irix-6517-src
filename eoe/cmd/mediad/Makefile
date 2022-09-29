#!smake
#
# Makefile for mediad(1m).
#
#	`$Revision: 1.38 $'

include $(ROOT)/usr/include/make/commondefs

DEVMODULES = dev_cdrom dev_dat dev_floppy dev_jaz dev_ls120 dev_optical \
	     dev_pccard dev_syquest dev_tape dev_zip

FMTMODULES = fmt_cdda fmt_dos fmt_efs fmt_hfs fmt_iso fmt_xfs

CXXMODULES = CallBack CompatClient CompatListener CompatServer Config \
             DSO DSReq Device \
             DeviceAddress DeviceDSO DeviceInfo \
             DeviceLibrary DeviceMonitor Enumerable FAMonitor FormatDSO \
             FormatLibrary FormatRaw Fsd Inventory IOHandler Log \
             MediaDaemon MonitorClient \
             ParallelAddress Partition PartitionAddress \
             RPCListener SCSIAddress Scheduler \
             SimpleVolume Task UnknownDevice Volume VolumeAddress \
	     bool main strplus


CXXFILES = $(CXXMODULES:=.C)
DEVSRC   = $(DEVMODULES:=.C)
FMTSRC   = $(FMTMODULES:=.C)
DEV_DSOS = $(DEVMODULES:=.so)
FMT_DSOS = $(FMTMODULES:=.so)

ALLSRC=$(CXXFILES) $(DEVSRC) $(FMTSRC)

CFILES = xdr_mc.c
LCXXDEFS = -DNDEBUG 			# Comment out to make mediad assertive.
LCXXOPTS = -fullwarn
LDIRT    = ii_files Makedepend $(TARGETS) *.pure cvdb* cvstatic.* vista.taf.*
MEDIAD_LIBRARIES = -ldisk -lds -lfam
LLDOPTS = -Wl,-I,$(ROOTRLDNAME)

MEDIAD_IDB_TAG= -idb irix_eoe.sw.rm_media
EOE_IDB_TAG= -idb irix_eoe.sw.eoe

TARGETS  = mediad $(DEV_DSOS) $(FMT_DSOS)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -m 4755 -F /usr/etc $(MEDIAD_IDB_TAG) mediad
	$(INSTALL) -m 0744 -F /etc/init.d $(MEDIAD_IDB_TAG) -src mediad.2 mediad
	$(INSTALL) -m 0744 -F /etc/config -idb "config(noupdate)" \
						-src mediad.config mediad
	$(INSTALL) -m 0644 -F /etc/config $(MEDIAD_IDB_TAG) \
			-idb "config(noupdate)" mediad.options
	$(INSTALL) -F /etc/rc2.d -lns ../init.d/mediad $(MEDIAD_IDB_TAG) S97mediad
	$(INSTALL) -F /etc/rc0.d -lns ../init.d/mediad $(MEDIAD_IDB_TAG) K09mediad
	$(INSTALL) -m 0755 -F /usr/lib $(EOE_IDB_TAG) Eject
	$(INSTALL) -F /usr/sbin -lns ../etc/mediad $(MEDIAD_IDB_TAG) eject

	$(INSTALL) -dir /usr/lib/devicelib
	$(INSTALL) -m 0755 -F /usr/lib/devicelib $(MEDIAD_IDB_TAG) $(DEV_DSOS)
	$(INSTALL) -m 0755 -F /usr/lib/devicelib $(MEDIAD_IDB_TAG) $(FMT_DSOS)

# headers:
# 	$(INSTALL) $(IMDNOSHIPIDB) -F /usr/include -m 444 $(HFILES)

mediad:	mediad.precord
	$(TOOLROOT)/usr/bin/cord -o $@ $@.precord $@.fb

mediad.precord:	$(OBJECTS)
	$(CXXF) $(OBJECTS) $(LDFLAGS) $(MEDIAD_LIBRARIES) -o $@

cvstatic.fileset: always
		echo '-cvstatic' > $@
		for i in $(ALLSRC); do echo "`pwd`/$$i NCC" >> $@; done

always:

##########  Device DSOs (in alphabetical order)  ##########

dev_cdrom.so: dev_cdrom.o
	$(CXXF) $(LDFLAGS) -shared dev_cdrom.o -o $@

dev_dat.so: dev_dat.o
	$(CXXF) $(LDFLAGS) -shared dev_dat.o -o $@

dev_floppy.so: dev_floppy.o
	$(CXXF) $(LDFLAGS) -shared dev_floppy.o -o $@

dev_jaz.so: dev_jaz.o
	$(CXXF) $(LDFLAGS) -shared dev_jaz.o -o $@

dev_ls120.so: dev_ls120.o
	$(CXXF) $(LDFLAGS) -shared dev_ls120.o -o $@

dev_optical.so: dev_optical.o
	$(CXXF) $(LDFLAGS) -shared dev_optical.o -o $@

dev_pccard.so: dev_pccard.o
	$(CXXF) $(LDFLAGS) -shared dev_pccard.o -o $@

dev_syquest.so: dev_syquest.o
	$(CXXF) $(LDFLAGS) -shared dev_syquest.o -o $@

dev_tape.so: dev_tape.o
	$(CXXF) $(LDFLAGS) -shared dev_tape.o -o $@

dev_zip.so: dev_zip.o
	$(CXXF) $(LDFLAGS) -shared dev_zip.o -o $@

##########  Format DSOs  ##########

fmt_cdda.so: fmt_cdda.o
	$(CXXF) $(LDFLAGS) -shared fmt_cdda.o -o $@

fmt_dos.so: fmt_dos.o
	$(CXXF) $(LDFLAGS) -shared fmt_dos.o -o $@

fmt_efs.so: fmt_efs.o
	$(CXXF) $(LDFLAGS) -shared fmt_efs.o -o $@

fmt_hfs.so: fmt_hfs.o
	$(CXXF) $(LDFLAGS) -shared fmt_hfs.o -o $@

fmt_iso.so: fmt_iso.o
	$(CXXF) $(LDFLAGS) -shared fmt_iso.o -o $@

fmt_xfs.so: fmt_xfs.o
	$(CXXF) $(LDFLAGS) -shared fmt_xfs.o -o $@
