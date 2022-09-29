#!/bin/sh
#	"$Revision: 1.5 $"

# This script/configuration file can be used as the 'login shell' for
#   an incoming SLIP line.

#   Put the name of this script into the last field of the /etc/passwd
#   entry for the remote machine's account on this machine like
#   the following:
#remhost:habefghijsu46:0:0:Joes remote machine:/:/usr/etc/remoteslip

#   Then remove "IRIS*" and add "remhost" or whatever to the case
#   statement below.  If the machine needs special parameters, add
#   them to the arg's to slip, or create special case in the switch
#   statement for the machine.


case $USER in
# turn on cslip compression for some
IRIS1|IRIS3)
    exec /usr/etc/slip -p cslip -i -r $USER
    ;;

# turn on SGI compression for some
IRIS2|IRIS4)
    exec /usr/etc/slip -p comp -i -r $USER
    ;;

# simplest default
*)
    exec /usr/etc/slip -i -r $USER
    ;;
esac
