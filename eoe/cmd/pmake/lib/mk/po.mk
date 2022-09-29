#
# Transformation rules for creating profiled object files with the .po suffix.
#
# $Revision: 1.1 $
#

#ifndef _PO_MK
_PO_MK		=

.SUFFIXES	: .po

ASFLAGS		?=
PCFLAGS		?=

.c.po		:
		$(CC) $(PCFLAGS) $(CFLAGS) -S -p $(.IMPSRC)
		$(AS) $(ASFLAGS) -o $(.TARGET) $(.PREFIX).s
		rm -f $(.PREFIX).s
.s.po		:
		$(AS) $(ASFLAGS) -o $(.TARGET) $(.IMPSRC)

#endif _PO_MK
