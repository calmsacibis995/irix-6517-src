# miniroot's csh settings
#
# "$Revision: 1.9 $"

# List directories in columns and show hidden files
alias ls 'ls -CA'

# Remember last 100 commands
set history = 100

# Add target system to our path
set path = (/usr/sbin /usr/bsd /usr/bin /sbin /bin /etc /usr/etc /root/usr/sbin /root/usr/bsd /root/usr/bin /root/bin /root/etc /root/usr/etc)

# use target system terminal database
setenv TERMINFO /root/usr/lib/terminfo
# use text port intelligently
if ($?L0) then
    if ("$L0" == NOGRAPHICS) set term="iris-tp"
endif

umask 022

# hook to allow other mr*rc scripts to fiddle prompt
source /.prompt >& /dev/null
