#! /sbin/sh
#Tag 0x00000800
#
# Take the system multi user.  This is a nop if the system is already
# multi user.
#
# $Revision: 1.5 $

/sbin/telinit 2
