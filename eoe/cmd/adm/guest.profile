# guest's sh profile
#
# The commands in this file are executed when an sh user first
# logs in.
#
# $Revision: 1.11 $
#

if [ -z "$ENVONLY" ]
then
	# Set the interrupt character to Ctrl-c and do clean backspacing.
	if [ -t 0 ]
	then
		stty intr '^C' echoe
	fi

	# Set the TERM environment variable
	eval `tset -s -Q`
fi

# Set the default X server.
if [ ${DISPLAY:-setdisplay} = setdisplay ]
then
    if [ ${REMOTEHOST:-islocal} != islocal ]
    then
        DISPLAY=${REMOTEHOST}:0
    else
        DISPLAY=:0
    fi
    export DISPLAY
fi
