# guest's cshrc settings
#
# The commands in this file are executed each time a new csh shell
# is started.
#
# $Revision: 1.9 $
#

# Remember last 100 commands
set history = 100

# For interactive shells, set the prompt to show the host name and event number.
if ( (! $?ENVONLY) && $?prompt ) then
	if ( -o /bin/su ) then
		set prompt="`hostname -s` \!# "
		alias ls 'ls -CA'	# List directories in columns
	else
		set prompt="`hostname -s` \!% "
		alias ls 'ls -C'	# List directories in columns
	endif
endif
