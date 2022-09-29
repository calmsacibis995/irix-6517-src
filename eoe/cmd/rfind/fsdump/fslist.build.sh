
# fslist.build
#
# This script is intended to be run from
# an inst exitop just after it is installed.
#
# This script could have been put directly in
# the exitop, except that it was difficult to
# read and maintain it that way.
#
# The purpose of this script is to setup the file "fslist"
# of all locally mounted efs file systems, the first time
# that the user installs the server side of rfind/fsdump.
# Thereafter, this script should leave alone whatever the
# user already has in fslist.  It does this by relying on the
# fact that the shipped, config(noupdate), fslist has the
# special phrase "Dummy marker" in it, and only over writing
# fslist if it has this marker.  The fslist file is shipped
# config(noupdate) so that it will only be installed if there
# is no such file already present.
#
# Silicon Graphics
# 28 April 92

test -d /var/rfindd || exit 0
cd /var/rfindd
test -f fslist || exit 0
grep 'Dummy marker' fslist > /dev/null || {
	# echo '    ... Skipping fslist build - /var/rfind/fslist already present'
	exit 0
}

(
	echo '# List of local efs file systems to be tracked using fsdump.'
	echo '# Must specify file system mount point as root based path.'
	echo '# The script runfsdump reads this list, each time cron runs it.'
	echo '# By default, the dump file name is the basename of the mount point.'
	echo '# If a second field is provided, then that is used instead.'
	df -l | sed -e 1d -e 's/.* //'
) > fslist  2>/dev/null
	    # in case of clean targ disk w/o /etc/mtab, silence df complaints
exit 0
