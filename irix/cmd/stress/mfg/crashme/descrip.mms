! VMS MAKEFILE (for MMS)
!

CFLAGS = /DEBUG/LIST/SHOW=(NOSOURCE)/MACHINE/OPTIMIZE=(NOINLINE)/STANDARD=PORTABLE

crashme.exe depends_on crashme.obj
 link crashme.obj,crashme.opt/opt
 ! re-execute the next line in your superior process:
 crashme == "$" + f$env("DEFAULT") + "CRASHME"

crashme-dbg.exe depends_on crashme.obj
 link/debug/exe=crashme-dbg.exe crashme.obj,crashme.opt/opt

! note: do not use continuation character here.
DIST_FILES = crashme.1,crashme.c,makefile,descrip.mms,crashme.opt,read.me,shar.db

crashme.shar depends_on $(DIST_FILES)
 minishar crashme.shar shar.db

crashme.1_of_1 depends_on $(DIST_FILES)
 define share_max_part_size 1000
 vms_share $(DIST_FILES) crashme
