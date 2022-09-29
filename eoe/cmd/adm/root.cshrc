# root's csh settings
#
# "$Revision: 1.12 $"

# List directories in columns and show hidden files
alias ls 'ls -CA'

# Remember last 100 commands
set history = 100

# For interactive shells, set the prompt to show the host name and event number.
# The sed command removes the domain from the host name.  `hostname -s`
# accomplishes the same but is not available when /usr is not mounted.
if ( (! $?ENVONLY) && $?prompt ) then
	if ( -o /bin/su ) then
		set prompt=`sed -e '/^ *$/d' -e 's/\..*//' /etc/sys_id`" \!# "
	else
		set prompt=`sed -e '/^ *$/d' -e 's/\..*//' /etc/sys_id`" \!% "
	endif
endif
