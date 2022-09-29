# root's sh profile
#
# "$Revision: 1.17 $"

if [ -z "$ENVONLY" ]
then
	# Set the interrupt character to Ctrl-c and do clean backspacing.
	if [ -t 0 ]
	then
		stty intr '^C' echoe 
	fi

	# Set the TERM environment variable
	if [ -d /usr/lib/terminfo ]
	then
		eval `tset -s -Q`
	fi

	# save tty state in a file where wsh can find it
	if [ ! -f $HOME/.wshttymode -a -t 0 ]
	then
	    stty -g > $HOME/.wshttymode
	fi
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
