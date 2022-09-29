#ident  "ide/godzilla/cache_thrasher/Makefile:  $Revision: 1.3 $"
#
#  This makes the diagnostics for cache_thrasher tests (destructive!).
#

DIAGDEPTH= ../..

include ${DIAGDEPTH}/Makedefs

HFILES =
# override common POBJECTS so only necessary files compile for PROM
CFILES= ct_sweep.c ct_turbo.c ct_misc_lib.c 
ASFILES= ct_libmp.s ct_lock.s

include ${DIAGDEPTH}/Makerules

# place all diag header files in ide/godzilla/include dir
# LINCLUDES is used in stand/arcs/ide/Makedefs
LINCLUDES = -I../include

