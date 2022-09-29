#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

. mrprofrc

# see comments in mrinstrc for details.  No longer
# has much to do...

case $1 in

    bootnorm)	# mark us has having completed normally.
		oval="`nvram OSLoadOptions`"
		case "$oval" {
		INST*)	MAJ=0; MIN=0
			dvhtool -v get sash /tmp/sash$$
			eval `strings /tmp/sash$$ 2>&- | sed -n \
			      's/^.*Version \([0-9]*\)\.\([0-9]*\) .*$/MAJ=\1; MIN=\2;/p'`
			rm /tmp/sash$$ 2>&-
			if [ "$MAJ" -lt 6 -o \( "$MAJ" -eq 6 -a "$MIN" -lt 4 \) ]; then
			        # On pre-6.4 sash, clear OSLoadOptions since setting it
			        # to instauto will cause sash to set it to just auto, which
			        # will cause sash not to autoboot on the second reboot.
				oval=""
			else
				# On 6.4 and later, sash understands that "instfoo" means
				# a miniroot install completed successfully, and sets it
				# to NULL before returning to multi-user mode.
				oval="inst${oval#INST}"
			fi
			nvram OSLoadOptions "$oval"
		}
	;;

    *)
	echo Usage: mrvolhdrrc bootnorm

esac
