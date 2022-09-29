#! /bin/sh

# This script is intended to be invoked from rfindd's crontab once each
# hour, or some other regular period such as each 30 minutes, each 12
# hours or daily, depending on how much compute resources you choose to
# spend updating the rfind data.
#
# It will invoke fsdump once on each file system listed
# in ./fslist, with the fsdump options specified in ./fsoptions.
#
# Both fslist and fsoptions may have comments, beginning with
# any # character and ending with the next newline.  Also empty lines
# or lines containing only white space after comment stripping are ignored.
#
# Silicon Graphics
# 27 Apr 92

listfile=fslist			# List of File Systems to Dump
optfile=fsoptions		# Desired fsdump options on each run, e.g. -R, -Q
stamp=.log.rotate.timestamp	# Date stamp file of time logs last rotated

# Maintain a pid-containing lockfile, to prevent
# multiple concurrent copies of this script
# working at once.  Multiple copies work fine,
# and have for a long time -- the only problem
# is that they load down the system too much.

if test -s fsdump.pid -a -r fsdump.pid
then
    pid=`sed 1q fsdump.pid`
    kill -0 $pid 2>/dev/null && exit
fi

echo $$ > fsdump.pid

logfilelist=.log.file.list
rm -f $logfilelist

# the sed script strips comments.
test -f "$optfile" && options=`sed < $optfile -n -e '/#.*/s///' -e '/[ 	]*$/s///' -e '/./p'` || options=

# the sed script strips comments.
sed < $listfile -n -e '/#.*/s///' -e '/[ 	]*$/s///' -e '/./p' |
while read mntpt name
do
    base=`basename "$mntpt"`
    test "/" = "$base" && base=root
    fsname=${name:-$base}

    fs_df=`df -i $mntpt` || { echo skipping $mntpt .. df failed 1>&2; continue; }
    fs_iuse=`echo "$fs_df" | sed 1d | tr -s ' ' '\011' | cut -f7`
    fs_ifree=`echo "$fs_df" | sed 1d | tr -s ' ' '\011' | cut -f8`
    fs_itotal=`expr "$fs_iuse" + "$fs_ifree"`;

    # set fsdump -M to 1/30000 total number of inodes , plus a minute
    # XXX Should have 30000 factor vary with cpu speed
    Mopt=`expr "$fs_itotal" / 30000 + 1 2>/dev/null`

    log="$fsname".log
    echo "$log" >> $logfilelist

    ./fsdump -L"$log" -F"$fsname" $options -M"$Mopt" "$mntpt"
    fsdumpExitVal=$?

    # Occassionally, we'll get a bad fsdump data file that will
    # cause fsdump to repeatedly dump core.  Clean it out (and
    # the usually large core file) and let it rebuild next run.
    # Typically, the core file is either in /var/rfindd or in
    # the directory "$mntpt".
    #
    # We only end up removing the fsdump data file if both
    #	1) the log did not end with a TOTAL line, and
    #	2) fsdump exited with a core dump signal
    #
    # We only end up removing a core file if the above conditions
    # are met, and the core is from fsdump, and the core is in
    # either /var/rfindd (`pwd`) or in $mntpt.

    tail -1 "$log" | grep TOTAL > /dev/null || {
	test $fsdumpExitVal -gt 128 && {
	    exitSignal=`expr $fsdumpExitVal - 128`
	    case $exitSignal in
	    4|5|6|7|8|10|11|12)	    # ILL|TRAP|ABRT|EMT|FPE|BUS|SEGV|SYS)
		corepath=
		for i in `pwd`/core "$mntpt"/core
		do
		    test -f "$i" &&
			( file "$i" | grep "dump of 'fsdump'" ) >&- 2>&- && {
			    rm -f "$i"
			    corepath="$i"
			    break
			}
		done

		rm -f "$fsname"

		# echo's end up in email from cron
		if test -n "$corepath"
		then
		    echo Removed corrupted: `pwd`/"$fsname", and fsdump core: "$corepath".
		else
		    echo Removed corrupted `pwd`/"$fsname"
		fi |  tee -a "$log"
		echo You might check /var/adm/SYSLOG for disk errors.

		# Lets try rebuilding it, rather than wait until next run
		./fsdump -L"$log" -F"$fsname" $options -M"$Mopt" "$mntpt"
		fsdumpExitVal=$?
		test $fsdumpExitVal -eq 0 &&
		    echo Successfully rebuilt `pwd`/"$fsname".
	    esac
	}
    }

    sleep 20		# give other stuff a chance to page in
done

# Rotate logs if not done so for last 7 days
if test ! -f $stamp
then
	touch $stamp
else
	if test -f "`find . -mtime +6 -name $stamp -print`" -a -r $logfilelist
	then
		./rotatelogs `cat $logfilelist`
		rm -f $stamp
		touch $stamp
	fi
fi

rm -f fsdump.pid		# not necessary - just being a neatnick
