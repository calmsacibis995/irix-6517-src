#! /sbin/sh
#Tag 0x00000f00

# XLV startup for miniroot
# Executed from mrmountrc
# "$Revision: 1.11 $"

if [ -f /root/etc/sys_id ] ; then
    name=`cat /root/etc/sys_id | sed 's/\([0-z]*\)\..*/\1/'`
elif [ -f /etc/sys_id ] ; then
    name=`cat /etc/sys_id | sed 's/\([0-z]*\)\..*/\1/'`
else
    name="IRIS"
fi

# Mark the plex from which we booted up clean; mark the others stale.
normalroot=`devnm / | sed 's/. .*/0/'`      # Jam '0' on end of swap device
/sbin/xlv_set_primary $normalroot

/sbin/rm -rf /dev/dsk/xlv > /dev/null 2>&1
/sbin/rm -rf /dev/rdsk/xlv > /dev/null 2>&1

# Start up the plex revive daemon; xlv_admin may issue plex revive 
# requests.
/sbin/xlv_plexd -m 4

# Set the geometry of the logical volume. Create the output in
# /root.  Remember, even in the miniroot, we mount devices from
# /root/dev/dsk/xlv instead of /dev/dsk/xlv (except for /root itself).
#
/sbin/xlv_assemble -Pqr /root -h $name > /dev/null 2>&1

# Make sure the configuration makes it out to disk
/sbin/sync

# Start the label daemon
# We start it after xlv_assemble because xlv_labd will exit if there
# are no logical volumes.
/sbin/xlv_labd > /dev/null 2>&1
