# guest's csh login profile
#
# The commands in this file are executed when you first
# log in as guest.  This file is processed after .cshrc.
#
# $Revision: 1.8 $
#

if (! $?ENVONLY) then
	# Set the interrupt character to Ctrl-c and do clean backspacing.
	if (-t 0) then
	    stty intr '^C' echoe 
	endif

	# Set the TERM environment variable
	eval `tset -s -Q`

	# save tty state in a file where wsh can find it
	if ((! -f $HOME/.wshttymode) && (-t 0)) then
	    stty -g > $HOME/.wshttymode
	endif
endif

# Set the default X server.
if ($?DISPLAY == 0) then
    if ($?REMOTEHOST) then
	setenv DISPLAY ${REMOTEHOST}:0
    else
	setenv DISPLAY :0
    endif
endif
