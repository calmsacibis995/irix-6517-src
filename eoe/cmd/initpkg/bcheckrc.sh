#! /sbin/sh
#Tag 0x00000f00
#ident	"$Revision: 1.63 $"
#
# This script runs those commands necessary to check file systems, to make
# generic device links, and to do anything else that should be done before
# mounting file systems.
#

# create /tmp (used by shell for tmp files)
test -r /tmp || mkdir /tmp > /dev/null 2>&1

# Build device aliases before fsck
if [ -d /hw/disk ]; then
	/sbin/suattr -C CAP_MAC_READ,CAP_MAC_WRITE,CAP_MKNOD+ipe -c "/sbin/ioconfig -f /hw"
fi

# Make symlink "/usr/bin -> ../../sbin".  Must be done *before*
# /usr is mounted, so programs executed as /bin/* will work in
# single user mode.  Do not use the same algorithm as libc.so.1 since
# /usr/bin may be a real directory if root and usr are merged.
#
/sbin/ls /usr/bin > /dev/null 2>&1
if [ $? != 0 ]; then
	/sbin/ln -s ../sbin /usr/bin <&- > /dev/null 2>&1
fi

# Make symlink /usr/var/config -> ../../etc/config
# Must be done *before* /usr is mounted, so programs can use
# config files in single user mode.
#
if [ ! -s /usr/var/config ] ; then
	/sbin/mkdir -p /usr/var > /dev/null 2>&1
	/sbin/rm -rf /usr/var/config > /dev/null 2>&1
	/sbin/ln -s ../../etc/config /usr/var/config <&- > /dev/null 2>&1
fi


if [ "`/sbin/nvram diskless 2> /dev/null`" -eq 1 ] ; then
    if [ -f /var/boot/makedev ] ; then
	echo "Making client's devices..."
	cd /dev; ./MAKEDEV > /dev/null
	rm -f /var/boot/makedev
    fi
else
    set `/sbin/devnm /`
    rootfs=$1
    /sbin/fsstat ${rootfs}  >/dev/null 2>&1
    if [ $? -eq 1 ] ; then
	echo "The root file system, ${rootfs}, is being checked automatically."
	/sbin/fsck -y ${rootfs}
    fi
fi

if [ -x /etc/init.d/failover ] ; then
        /etc/init.d/failover init
fi

if [ -x /sbin/xlv_assemble ] ; then
	/etc/init.d/xlv init
fi

# Remove ps temporary file
rm -rf /tmp/.ps_data

# Make symlink /usr/lib/libc.so.1 -> ../../lib/libc.so.1
# and /usr/lib32/libc.so.1 -> ../../lib32/libc.so.1
# Must be done *before* /usr is mounted, so dynamic linked
# programs can be run in single user mode.
if [ ! -s /usr/lib/libc.so.1 ]
then
	mkdir -p /usr/lib > /dev/null 2>&1
	rm -rf /usr/lib/libc.so.1 > /dev/null 2>&1
	ln -s ../../lib/libc.so.1 /usr/lib/libc.so.1 <&- > /dev/null 2>&1
fi
if [ ! -s /usr/lib32/libc.so.1 ]
then
	mkdir -p /usr/lib32 > /dev/null 2>&1
	rm -rf /usr/lib32/libc.so.1 > /dev/null 2>&1
	ln -s ../../lib32/libc.so.1 /usr/lib32/libc.so.1 <&- > /dev/null 2>&1
fi

