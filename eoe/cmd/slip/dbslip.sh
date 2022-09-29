#!/bin/sh
#	"$Revision: 1.2 $"

# This script/configuration file can be used for a "dial-back SLIP line."

#   Put the name of this script into the last field of the /etc/passwd
#       entry for the remote machine's account on this machine like
#       the following:
#remhost:baldeblah:0:0:Joes remote machine:/:/usr/etc/remoteslip

#   Then add an appripriate line for the remote machine in
#	/usr/lib/uucp/Systems.

#   Finally, add an appropriate line to /etc/passwd on the remote machine
#	to allow the local machine to log in with /usr/etc/dbslip as its
#	shell:
#lochost:baldeblah:0:0:Joes remote machine:/:/usr/etc/dbslip



echo "\n\nwill hang up `tty` and dial you back.\n\n"
sleep 1

# ignore SIGHUP
trap "" 1

# get rid of the incoming modem line
stty 0
exec < /dev/null > /dev/null 2>&1

# let the modems quiet down
sleep 15

case $USER in
# Joe's machine
#IRIS1)
#    exec /usr/etc/slip -p cslip -r $USER &
#    ;;

*)
    logger "unknown dial-back slip user"
    ;;
esac
