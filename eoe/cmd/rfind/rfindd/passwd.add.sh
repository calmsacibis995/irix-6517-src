
# passwd.add
#
# This script is intended to be run from
# an inst exitop just after it is installed.
#
# The purpose of this script *used to be* to add the user
#
#	uid=66(rfindd) gid=10(nuucp)
#
# with the line:
#
#	rfindd:*LK*:66:10:Rfind Daemon and Fsdump:/var/rfindd:
#
# to the /etc/passwd file, and to mark its password
# as being locked.
#
# But now this script just forces the ownership of a few
# files (some of /var/rfindd/* and /etc/init.d/rfindd)
# to be owned by uid == 66, in anticipation that when the
# installation is finished, and the user has merged in
# the new /etc/passwd file from SGI, that then there
# will an entry in /etc/passwd for uid == 66, named "rfindd".
#
# If not on the miniroot, this script also restarts the fsdump
# entry from crontabs/rfindd, and the rfindd daemon with /etc/init.d/rfindd.

PATH=/bin:/usr/bin export PATH

cd /var/rfindd
chown 66.10 . rfindd rotatelogs .forward README

cd /etc/init.d
chown 66.10 rfindd

test x"$mr" = xtrue && exit 0

grep '^rfindd:' >/dev/null /etc/passwd || exit 0

/bin/sh ./rfindd stop
/bin/sh ./rfindd start

cat /var/spool/cron/crontabs/rfindd | su rfindd -c crontab &&
	echo Scheduling fsdump from /var/spool/cron/crontabs/rfindd
