#!/bin/sh

sleep 5 # this is because sometimes we get re-invoked before shutdown
	# really completes, so this holds off our first prompt...

# this script is the complement of the Backup script, for restoring
# system recovery tapes.  The restore program that is the complement
# of dump is in /usr/etc, normally.  This program should only
# exist in the miniroot.  There is a much simpler version that
# is used for simply restoring backups, not recovering systems,
# that is installed as /etc/restore on the 'real' system.
#

NETIF=/etc/config/netif.options
HOSTS=/etc/hosts
# reset to null string if restoring from localhost tapedrive.
RSHCMD='rsh guest@$REM_HOST -n'
DEVNULL=/dev/null
# DEVNULL=/dev/console # for debugging
MTB=""

# standard su path from /etc/default/login
PATH=/usr/sbin:/usr/bsd:/sbin:/usr/bin:/etc:/usr/etc:/usr/bin/X11
export PATH


# run a shell, with interrupts enabled again, then disable on
# return;  "knows" that it's only ever called when we do want
# interrupts disabled.
shell()
{
	trap 2
	stty sane intr '^c' echoe
	csh <&5 >&6 2>&1
	trap '' 2
}

# do a recovery run (as opposed to the label runs), and allow
# sigint to abort it.
do_a_tape()
{
	while :  #{
	do
		# now allow interrupts, so bru can be aborted
		trap 'echo Interrupted; intrpt=1' 2

		cd /root # extracted in "real root, with relative paths"
		intrpt=0

		# always rewind, just in case labelcmd leaves tape in strange place
		mt -t $TAPE_PATH rewind
		$extract_all

		if [ "$intrpt" = 1 ]; then #{
		while echo "    Restore was interrupted, restart it ([y]es, [n]o [sh]): \c "
		do	read resp # {
			case "$resp" {
			y*|Y*)
				break ;;
			sh*)
				shell ;;
			n*|N*)
				echo $1 restore will not be retried
				break 2;
				;;
			}
		done # }
		else break;
		fi #}
	done # }

	echo '\nRunning MAKEDEV in case devices have changed since backup \c'
	(cd /root/dev; ./MAKEDEV > $DEVNULL)
	echo '\n'

	trap '' 2 # return with sigint ignored again
}

# shutdown the system.  Can't use teleinit, because we are still in
# the middle of a level change.  So we have to fake it ourselves.
# Otherwise we often simply hang, and user has to push reset button.
# argument is 1 or 0 for reboot or halt
shut_it_down()
{
	sync
	trap 2 # allow interrupts again, so if kernel config fails
		# the can ^C and init will respawn us, if they notice
		# errors from autoconfig and are quick.

	if [ "$1" = 1 ]
	then
		echo "\n    Recovery complete, restarting system."
		# may need to reconfigure kernel; don't force it.
		echo "Checking to see if kernel needs to be reconfigured"
		/root/sbin/chroot /root /etc/init.d/autoconfig
	else
		echo "\n	Recovery aborted, halting system."
	fi
	cd /
	sleep 10
	sync
	umount -aTk efs,xfs -b /,/proc,/hw >$DEVNULL 2>&1
	uadmin 2 $1

	# make sure we shutdown; else we exit and might be restarted
	# confusing the user, before the uadmin causes init to shut down.
	sleep 30

	echo '\07Shutdown failed!\07'
}


#########################################################################
#																		#
# Ask the user to insert the incremental tape, read the tape header,	#
#																		#
#########################################################################
read_incremental_tape()
{
	while : ; do #{
		echo ""
		echo "    Insert the Incremental backup tape in the drive, then press \c"
		echo "(<enter>, [q]uit (skip this tape)): \c"
		read kstroke
		case "$kstroke" {
			"")		;;
			sh*) shell
					continue ;;
			q*|Q*)	return ;;
			*)		continue ;;
		}

		get_backup_type # get type on each tape; might be different
		if $labelcmd; then continue; fi

		echo "    Do you want to proceed ([y]es, [r]etry, [q]uit): [y] \c"
		read ans
		case "$ans" {
			""|y*|Y*)
				break ;;
			sh*)
				shell
				continue ;;
			r*|R*)
				continue ;;
			q*|Q*)
				shut_it_down 0 ;;
		}
	done #}

	do_a_tape Incremental
}

#########################################################################
#																		#
# This function looks for the /tmp/volhdrlist file and to get the list	#
# of the vol header contents and restores it from /stand.    			#
#																		#
#########################################################################
restore_volume_header()
{
	cd /root
	echo "Restoring the disk volume header."
	if [ -s ./tmp/volhdrlist ] #{
	then
		cat ./tmp/volhdrlist | while read nm
		do #{
			if [ -s ./stand/$nm ]
			then
			dvhtool -v creat ./stand/$nm $nm
			else
				echo Volume header file "$nm" not found, so not restored to volume header.
			fi
		done #}
		echo "Volume header restored."
	fi #}
	if [ -s ./tmp/vh_savedir ] #{
	then
		vhd="/root/`cat ./tmp/vh_savedir`"
		if [ -d "$vhd" ] #{ restore files in vh not in /stand
		then
			(cd "$vhd"; for f in *; do
				dvhtool -v c "$f" "$f"
				done
			)
		fi #}
	fi #}
}
#########################################################################
#									#
# This function searches the head of the backup tape and sets the MTB	#
# flag if the file /tmp/mtab is found and restores the file.		#
# Also looks for lvtab, xlv, and mounts XLV vols                        #
#                                                                       #
#########################################################################
look_for_mtab_file()
{
	# the idea is to get only the files we want from bru, then kill
	# it off.  Otherwise it will go through the whole tape, and even
	# prompt for additional tapes.  This requires that bru line
	# buffer output even when it is to a pipe.
	# look at first 10 files/1MB only.
	# run in subshell so the 'killed' message doesn't show up when
	# tape command is killed
	echo Extracting system configuration files ... \\c
	$get_mtab_file > /tmp/.mtf
	echo ''
	if [ -s /tmp/.mtf ] #{
	then
		if [ -s /tmp/lvtab ]; then 
			echo "\n	LV volumes are no longer supported in this release.";
			echo "	Please use lv_to_xlv(1) to convert to XLV volumes.\n";
		fi

		# check for prior existance of xlv volumes
		if [ -s /tmp/xlv_vollist ]
		then
			echo "\nXLV volumes existed during original backup - machine name required."

			# Taken from mrinitxlvrc.sh
			# Mark the plex from which we booted up clean; mark the others stale.
			normalroot=`devnm / | sed 's/. .*/0/'`      # Jam '0' on end of swap device
		xlv_set_primary $normalroot

		rm -rf /dev/dsk/xlv > $DEVNULL 2>&1
		rm -rf /dev/rdsk/xlv > $DEVNULL 2>&1

			# use '/' for root since '/root' disk may be bad
		xlv_assemble -Pq -h $machname > $DEVNULL 2>&1

			# Make sure the configuration makes it out to disk
		sync
	
			# check if any xlv vols now exist
			if [ -d /dev/dsk/xlv ]
			then
				ls /dev/dsk/xlv > /tmp/.xlv_list
			else
				cp /dev/null /tmp/.xlv_list
			fi
			# create an awk program file to compare xlv lists
			echo 'BEGIN { i=0; j=0; fno=0;}\n{if (fno==0) if ($1=="SPLIT") fno=1; else a[i++]=$1; else b[j++]=$1;}\nEND {for (x=0; x<i; x++) {fnd=0; for (y=0; y<j; y++) if (a[x]==b[y]) {fnd=1; break;} if (fnd==0) print "    XLV volume:", a[x], " missing";}}' > /tmp/.xlv_compare.awk
			cat /tmp/xlv_vollist > /tmp/.xlv_derived
			echo "SPLIT" >> /tmp/.xlv_derived
			cat /tmp/.xlv_list >> /tmp/.xlv_derived

			nawk -f /tmp/.xlv_compare.awk /tmp/.xlv_derived > /tmp/.xlv_result
			rm -f /tmp/.xlv_derived /tmp/.xlv_list /tmp/.xlv_compare.awk
			if [ -s /tmp/.xlv_result ]
			then
				#volumes missing, prompt user
				echo "\n    The following XLV volumes existed at the time this backup was created"
				echo "    which do not currently exist on your system:\n"
				cat /tmp/.xlv_result
				rm -f /tmp/.xlv_result

				echo ""
				echo "    You may continue, enter the shell and recreate the volumes manually,"
				echo "    or exit from recovery.\n"

				echo "    Do you want to manually recreate the XLV volumes?"
				echo "    ([y]es, [s]kip, [sh]ell, [q]uit (from recovery)): [y] \c" ;
				ans="`(read ans; echo $ans) < /dev/tty`" # don't read cat output
				case "$ans" {
					s|S)
						echo "\n    Skipping XLV restore...\n" ;;
					""|y*|Y*|sh*)
						echo "\nNOTE: The XLV configuration commands for the original XLV volume"
						echo "configuration can be found in '/tmp/xlv_config_script'.  If you have"
						echo "replaced or moved disks since the last backup, do not use the file as is"
						echo "since your new drive device names and partitions may be different.\n"
						echo "Type 'exit' to return to System Recovery when you're done creating volumes.\n"
						shell
						echo "\nReturning to System Recovery...\n"
						;;
					q*|Q*)
						shut_it_down 0 ;
						return 1 ;;
					*)
						;;
				}

			fi
		fi

		cat /dev/null > /tmp/mtab_extra
		typ=unknown # be paranoid
		cat /tmp/mtab | while read dev dir typ rest
		do #{
			# used to eliminate /usr also, and recover was
			# hardcoded to do it, but since some systems
			# have / and /usr on the same partition, that
			# was a bad idea...
			if [ "$dir" != / ] #{
			then
				echo "$dev $dir "$typ"" >> /tmp/mtab_extra
			fi #}
			typ=unknown # be paranoid
		done #}
		if [ -s /tmp/mtab_extra ] #{
		then
			MTB="EXTRA"
		else #}{
			MTB="BASIC"
		fi #}
	else #}{
		MTB=""
	fi #}

	echo $MTB > /tmp/.mtb
	rm /tmp/mtab /tmp/.mtf > $DEVNULL 2>&1
}

LABEL_ERR='\n     *** ERROR ***
    Unable to read the recovery tape, check
    tape device name and format.\n'

# get label from a bru tape
brulabel()
{
	TAPE_LABEL=/tmp/.label # where we put the extracted tape label

		bru -g -f $TAPE_PATH > $TAPE_LABEL 2>&1
		if [ $? -ne 0 ] #{
		then
			echo "$LABEL_ERR"
			return 0
		fi #}
		cat $TAPE_LABEL

		return 1;
}

# get label from a tar tape.
# since tar has no real labels, we simply fake this one.
# if it's written in the "sgi system backup format", it
# has such a label, otherwise we fake it.
# basicly, we assume the first file is small, and extract it.
# of course, unlike bru, we have it only on the first tape
# in a backup, normally...
tarlabel()
{
		eval $RSH dd if=$TAPE bs=1024k count=1 > /tmp/.$$L 2>$DEVNULL
		file="`tar eRtf /tmp/.$$L 2>&1 | sed -n 1p`"
		case "$file" {
		*tmp/.info_*) # a label
			finfo="`tar Rxvf /tmp/.$$L $file 2>&1 | sed -n 1p`"
			if [ ! -f "$file" ] #{
			then
				echo "$LABEL_ERR"
				return 0
			fi #}
			cat "$file"
			;;
		}
		return 1;
}

# get label from a cpio tape.
cpiolabel()
{
		file="`cpio -ti -I $1 2>&1 | sed -n 1p`"
		if [ $? -ne 0 ] #{
		then
			echo "Error trying to read label file"
			return 0
		fi #}
		# get the label file; first file on tape
		(cd /; cpio -i -I $1 $file >/dev/null 2>&1; if [ -f "$file" ]; 
			then cat $file; rm -f $file; fi)
		return 1;
}

# get the stashed mtab file from the backup; it is assumed to be
# in the first few files on archive.  The dd keeps us from going 
# all the way through the tape (not as much of a problem as with thar)
# also stop after first 15, just to be paranoid (avoid running out
# of disk space)
bru_get_mtab()
{
	x      4k of     436k [1] tmp/.ip.fragmatches.cache

	(cd /; eval $RSH dd if=$TAPE bs=1024k count=1 2>$DEVNULL | bru -jxvf - 2>&1 |
	  (cnt=0 tabf=1; while read x sz of tot links name
	  do
		case "$file" {
		# could be full, which drops /, or ./, which doesn't with bru
		*tmp/mtab|*tmp/lvtab|*tmp/vh_savedir|*tmp/xlv_vollist|*tmp/xlv_config_script)
			tabf=0
			echo $file
			;;
		}
		cnt=`expr $cnt + 1`
		if [ $cnt -gt 15 ]
		then killall bru; exit $tabf
		fi
	done
	exit $tabf
	)
	)
}

# get the stashed mtab file from the backup; it is assumed to be
# in the first few files on archive.  The dd keeps us from going 
# all the way through the tape looking for any later copies...
# also stop after first 15, just to be paranoid (avoid running out
# of disk space)
tar_get_mtab()
{
	(cd /; eval $RSH dd if=$TAPE bs=1024k count=1 2>$DEVNULL | tar -BRvxf - 2>&1 |
	   (cnt=0 tabf=1; IFS=",$IFS" ; while read x file rest
	  do
		case "$file" {
		tmp/mtab|tmp/lvtab|tmp/vh_savedir|tmp/xlv_vollist|tmp/xlv_config_script)
			tabf=0
			echo $file
			;;
		}
		cnt=`expr $cnt + 1`
		if [ $cnt -gt 15 ]
		then killall tar; exit $tabf
		fi
	done
	exit $tabf
	)
	)
}

cpio_get_mtab()
{
	(cd /; eval $RSH dd if=$TAPE bs=256k count=1 2>$DEVNULL | cpio -idmuvk 2>&1 |
	   (cnt=0 tabf=1; IFS=",$IFS" ; while read file rest
	  do
		case "$file" {
		tmp/mtab|tmp/lvtab|tmp/vh_savedir|tmp/xlv_vollist|tmp/xlv_config_script)
			tabf=0
			echo $file
			;;
		}
		cnt=`expr $cnt + 1`
		if [ $cnt -gt 15 ]
		then killall cpio; exit $tabf
		fi
	done
	exit $tabf
	)
	)
}

######
#
# Determine type of backup tape (bru, tar, xfsdump, cpio)
# and set commands to be used in the rest of the script.
# the rest
# xfsdump and cpio aren't yet really working for anything but "extra" tapes
#
######
get_backup_type()
{
	eval $RSH dd if=$TAPE bs=256k count=1 > /tmp/$$type 2>$DEVNULL
	LANG=C ttype="`file /tmp/$$type`"
	case "$ttype" {
	*xfsdump*) echo Backup is an xfsdump archive
		echo Only supported for additional tapes, not recovery tape
		labelcmd=
		# only supported for incrementals or extra tapes, not primary
		# this only really works for the first tape
		extract_all="xfsrestore -v verbose -r -f $TAPE_PATH /root"
		get_mtab_file=
		;;
	*cpio*) echo Backup is a cpio archive
		labelcmd="cpiolabel /tmp/$$type"
		# no relative pathname option, so so this is a bit of an issue...
		# only supported for incrementals or extra tapes, not primary
		extract_all="cpio -idmuvk -I $TAPE_PATH"
		get_mtab_file=cpio_get_mtab
		;;
	*bru*) echo Backup is a bru archive
		labelcmd=brulabel
		extract_all="bru -xvj -ur -f $TAPE_PATH"
		get_mtab_file=bru_get_mtab
		;;
	*tar*) echo Backup is a tar archive
		labelcmd=tarlabel
		extract_all="tar -xvRf $TAPE_PATH"
		get_mtab_file=tar_get_mtab
		;;
	*) echo Unable to determine backup type, assuming tar
		labelcmd=tarlabel
		extract_all="tar -xvRf $TAPE_PATH"
		get_mtab_file=tar_get_mtab
		;;
	}
}

#########################################################################
#																		#
# Ask the user to insert the first tape, read the tape header,			#
# do some error checking, read the /tmp/mtab file off			#
# the tape and set flag MTB if anything other than / and /usr			#
# needs to be mounted.													#
#																		#
#########################################################################
read_full_backup_header()
{
	while : ; do #{
		echo ""
		echo "    Insert the first Backup tape in the drive, then"
		echo "    press (<enter>, [q]uit (from recovery), [r]estart): \c"
		read kstroke
		case "$kstroke" {
			"")		;;
			r*|R*)	echo ; return 1;;
			sh*)	shell
					continue ;;
			q*|Q*)	shut_it_down 0 ;
					break ;;
			*)		continue ;;
		}

		get_backup_type

		if $labelcmd; then continue; fi

		echo "    Do you want to proceed ([y]es, [r]etry, [q]uit): [y] \c"
		read ans
		case "$ans" {
			""|y*|Y*)
				break ;;
			sh*)
				shell
				continue ;;
			r*|R*)
				continue ;;
			q*|Q*)
				shut_it_down 0 ;;
		}

	done #}

	look_for_mtab_file
	####  At this point the MTB flag specifies the existance of valid mtab
	####  file on tape.
	return 0
}


# do minimal network setup for just ourselves; we needed
# some of this for some code I removed, but it's actually useful
# to have hostname return the right thing in some places, and will
# make it easier on customer if they shell out. It's cleaner to do
# this up front.
# There is no point in checking /root/etc/sys_id,
# because /root is never mounted at this point...
setup_minimal_net()
{
	while : ; do #{
		echo ""
		# use ours as default if known, but allow to change in case of typos
		myname=`hostname` 2>$DEVNULL
		echo "    Please enter your hostname (system name)  \c"
		if [ "$myname" ]; then echo "[$myname]\c"; fi
		echo : \\c
		read host
		if [ -z "$host" ] #{
		then
			if [ "$myname" ]
			then MY_NAM=$myname; break
			else continue
			fi
		else #}{
			MY_NAM=$host
			break
		fi #}
	done #}

	# set up IP addresses, etc. for all interfaces (usually just one)

	# always write name and interface into NETIF, because
	# hinv order may not match order (probaby won't) that
	# network script uses to determine primary interface
	# for the purposes of this program, it doesn't matter
	# which is called host, and which is called gate-host,
	# etc., as long as the correct IP address gets configured
	# to the correct interface.
	# Always ask for all network interfaces.  Could try to
	# check if already configured (in case a restart), but
	# one of the reasons for restarting is that you answered
	# a question about a host address incorrectly!
	# same argument goes for re-creating netif every time
	# if more than 2 interfaces; ln /unix so if user does
	# a shell escape, netstat, etc. will work.
	rm -f $NETIF

	# move old host file aside, and then append, because
	# first match wins, and we might have been through
	# this code more than once on restarts, etc.
	if [ -r $HOSTS ]; then mv $HOSTS ${HOSTS}-; fi

	# need localhost for later use
	echo '127.1\tlocalhost' > $HOSTS

	# IFS diddling is to make parsing easier.
	hinv -c network | (
		IFS=":," gate=0
		while read name interface junk; do
		case "$name" {
		*ISDN*) continue ;; # we never use isdn in miniroot; won't work; no driver
		}
		# strip spaces left in by IFS changes
		interface=`echo $interface|sed 's/[ 	]//g'`
		if [ $gate -eq 0 ] #{
		then hname=$MY_NAM  gate=1
		elif [ $gate -eq 1 ]
		then hname=gate-$MY_NAM  gate=2
		else
			hname=gate$gate-$MY_NAM
			gate=`expr $gate + 1`
		fi #}
		echo if${gate}name=$interface >> $NETIF
		echo if${gate}addr=$hname >> $NETIF
		ipa=
		guessipa="`nvram netaddr 2>$DEVNULL`"
		while [ -z "$ipa" ]; do # in case they hit CR
		  echo "    Please enter the IP address for $MY_NAM's"
		  echo "       $name interface ($interface): $guessipa\c"
		  ipa="`(read ipa; echo $ipa) < /dev/tty`" # don't read hinv output
		  if [ -z "$ipa" ]; then ipa="$guessipa"; fi
		done
		echo "$ipa\t$hname" >> $HOSTS
	done )

	echo Starting networking with primary hostname $MY_NAM
	echo $MY_NAM > /etc/sys_id
	hostname $MY_NAM
	chkconfig -f network on
	/etc/init.d/network start
}


# get the name of the tape device to use.
get_tape_name()
{
####### Get the tape device, if remote, start networking after setting
####### up the hosts file, hostname and hostid.
#
set $@ `gettapedev`
REM_HOST=$1 TAPE=$2
if [ "$REM_HOST" = NULL ]; then REM_HOST=localhost RSH=;
else RSH="$RSHCMD"; fi
while :; do #{
	resp=n
	if [ "$TAPE" != NULL ]
	then
		if [ "$REM_HOST" != "localhost" ] || mt -t "$TAPE" exist
		then
			echo "Restore will be from $TAPE\c"
			if [ "$REM_HOST" != "localhost" ]
			# hostname may be long enough to overflow line
			then echo " on remote host $REM_HOST.\n  \c"
			fi
			echo "  OK? ([y]es, [n]o): [y] \c"
			read resp
		fi
	fi
	case "$resp" {
		"")
			break;
			;;
		y|Y*)
			break
			;;
		sh*)
			shell
			;;
		n*|N*)
			while :
			do
			echo "Remote or local restore ([r]emote, [l]ocal):\c"
			if [ "$REM_HOST" != "localhost" ]
			then
				echo " [r] \c"
				rlr=r
			else
				echo " [l] \c"
				rlr=l
			fi
			read resp
			if [ "$resp" = "" ]
			then
				resp=$rlr
			fi
			case "$resp" {
				r*|R*)
					echo "Enter the name of the remote system: \c"
					if [ "$REM_HOST" != "localhost" ]
					then
						echo "[$REM_HOST] \c"
					fi
					read resp
					if [ "$resp" = "sh" ]
					then
						shell
						continue
					fi
					if [ "$resp" != "" ]
					then
						REM_HOST=$resp RSH="$RSHCMD"
					fi
					if [ "$REM_HOST" = "localhost" ]
					then
						RSH=
						continue;
					fi
					;;
				l*|L*)
					REM_HOST=localhost RSH=
					;;
				sh*)
					shell
					continue
					;;
			}
			if [ "$REM_HOST" != "localhost" ]
			then
				echo "Enter the name of the tape device on $REM_HOST: \c"
			else
				echo "Enter the name of the tape device: \c"
			fi
			if [ "$TAPE" != "NULL" ]
			then
				echo "[$TAPE] \c"
			fi
			read resp
			if [ "$resp" = "sh" ]
			then
				shell
				continue
			fi
			if [ "$resp" != "" ]
			then
				TAPE=$resp
			fi
			if [ "$REM_HOST" = "localhost" ]
			then
				if mt -t $TAPE exist
				then
					:
				else
					echo "Tape drive $TAPE is not available."
					return 1
				fi
			fi
			break
			done
			;;
	}
done #}

if [ "$REM_HOST" != "localhost" ] #{
then
	while : ; do #{
		echo ""
		echo "    Please enter the IP address of "$REM_HOST" : \c"
		read ipa
		if [ "$ipa" = "" ] #{
		then
			continue
		else #}{
			REM_IPADDR=$ipa
			break
		fi #}
	done #}

	echo "$REM_IPADDR\t$REM_HOST" >> $HOSTS
	if [ -r ${HOSTS}- ]
	then cat ${HOSTS}- >> $HOSTS  && rm -f ${HOSTS}-
	fi

	TAPE_PATH=guest@$REM_HOST:$TAPE
	echo "Checking tape drive $TAPE on remote host $REM_HOST \c"
else #}{
	TAPE_PATH=$TAPE
	echo "Checking tape drive local tape drive $TAPE \c"
fi #}

if mt -t $TAPE_PATH exist
then :
else
	echo "\nTape drive $TAPE is not available."
	return 1
fi

echo "\nInformation for device $TAPE_PATH is: "
mt -t $TAPE_PATH status 2>&1 | grep -v Status:
return 0

} # end of get_tape_name

helperase()
{
	echo ""
	echo "If you would like to erase your disk(s) and start"
	echo "with empty filesystem(s), then answer yes when prompted."
	echo ""
	echo "If you answer yes, you will be prompted to confirm for"
	echo "each filesystem individually before it is erased."
	echo ""
	echo "If you answer no, then your filesystems will be checked"
	echo "and mounted in their current state if possible."
	echo "For any filesystems that are damaged beyond recovery,"
	echo "you will be prompted for each damaged filesystem,"
	echo "asking if you want to erase it."
	echo ""
	echo "You can answer with \"sh\" if you want to escape to the shell"
	echo "to examine and salvage the data yourself."
	echo ""
	echo "Modifications made to files after the backup tape"
	echo "was created will be overwritten by the restore."
	echo ""
}

# ask if they want to create the filesystems.
do_filesystems()
{
while : ; do #{
	echo ""
	helperase
	echo "Erase all old filesystems and make new ones ([y]es, [n]o, [sh]): [n] \c "
	read answer
	case "$answer" {
		n*|N*|'') echo ""
			  if [ "$MTB" = EXTRA ] #{
			  then
			  recover -f /tmp/mtab_extra
			  else
			  recover
			  fi #}
			  if [ $? -ne 0 ]
			  then
				  echo Filesystem check and/or mount failed
				  continue
			  fi
			  echo ""
			  break	;;
		y*|Y*)	  echo "" ;
			  umount -aT efs,xfs -b / >$DEVNULL 2>&1
			  if [ "$MTB" = "EXTRA" ] #{
			  then
			  recover -m -f /tmp/mtab_extra
			  else
			  recover -m
			  fi #}
			  if [ $? -ne 0 ] ; then
				  echo Filesystem check and/or mount failed
				  continue
			  fi
			  echo "" ;
			  break	;;
		sh*)	  shell
			  continue ;;
		q*|Q*)	  echo Recovery aborted, shutting down
			  shut_it_down 0 ;
			  break ;;
		h*|H*) helperase ;;
		*)  echo Type \"help\" for more information. ;;
	}
done #}
	dusage="`df | grep -v '/$'`"
	case "$dusage" {
	"") echo "Warning: no filesystems appear to be mounted; restore will likely fail" ;;
	*)	echo "\nStarting recovery with these filesystems mounted (where /root is your"
		echo "normal / directory).\n"
		echo "$dusage" ;;
	}
}

restore_incremental()
{
while : ; do #{
	echo ""
	echo "    Do you have incremental backup tapes to restore ([y]es, [n]o (none)): [n] \c "
	read resp
	case "$resp" {
		y*|Y*)
			echo "" ;
			read_incremental_tape ;
			continue ;;
		sh*)
			shell
			continue ;;
		""|*)
			break ;;
	}
done #}
}

restore_quotas()
{
	if [ -s tmp/quotatab ] 
	then
		echo "Restoring quotas."
		cd /
		cp tmp/quotatab /root/var/tmp/restore_quotas
		umount /root
		mount -o quotas `devnm / | sed 's/. .*/0/'` /root
	fi
}


#########################################################################
#																		#
#																		#
################    START OF THE MAIN SCRIPT    #########################
#																		#
#																		#
#########################################################################
# {

trap '' 2 # ignore int for most of script
stty sane intr '^c' echoe

echo ""
echo ""
echo "    ************************************************************"
echo "    *                                                          *"
echo "    *                     CRASH    RECOVERY                    *"
echo "    *                                                          *"
echo "    ************************************************************"
echo ""
echo ""
echo "You may type  sh  to get a shell prompt at most questions"

# Grab input for later shell escapes, in scripts and subscripts
# that have to read from stdin for various reasons.
# similarly for stdout.  Used in shell(), mainly, to be
# sure we get good stdin and out.
		exec 5<&0
		exec 6>&1

####### Ask the user to insert the first tape, read the tape header,
####### do all kinds of error checking, read the /tmp/mtab file off
####### the tape and set flag MTB if anything other than / and /usr
####### needs to be mounted.  call function  -- read_full_backup_header --
#

cd / # be paranoid

setup_minimal_net # do minimal setup for at least local networking
	# if remote tape, we'll do more below.

while :
do
	echo '\nChecking for tape devices'
	if get_tape_name
	then 
		if read_full_backup_header; then break; fi
	fi
done

#
#######
#
MTB="`cat /tmp/.mtb`" 
if [ "$MTB" = "" ] #{
then
	while : ; do #{
		echo ""
		echo "    Could not find the /tmp/mtab file on tape. Only the /"
		echo "    and /usr filesystems will be mounted and restored"
		echo "    unless you 'sh' and mkfs and mount others yourself."
		echo "    Do you want to proceed with recovery ([y]es, [q]uit (from recovery)): [y]\c "
		read resp
		case "$resp" {
			y*|Y*|"")
				break ;;
			sh*)
				shell
				continue ;;
			q*|Q*)
				shut_it_down 0
				break ;;
		}
	done #}
fi #}

while : ; do # {
	do_filesystems
	while : ; do # { # reloop on question if sh
		echo "Is this the correct list of filesystems and sizes?"
		echo "If not, answer no, to be asked again if you want to"
		echo "erase and mount all filesystems.  ([y]es, [n]o, [sh]): [y] \c"
		read resp
		case "$resp" {
			""|y*|Y*) break 2;;
			sh*) shell;;
			n*|N*|"") break;;
	}
	done #}
done #}

while : ; do # {
	do_a_tape Full
	restore_volume_header
	restore_incremental
	restore_quotas

	cd /

	while : ; do # { # loop on question if sh or invalid reply
	echo "Reboot, start over, or read first tape again? ([r]eboot, [s]tart, [f]irst) [r] \c"
	read resp
	case "$resp" {
		r*|R*|"") break 2;;
		sh*) shell;;
		s*) exit 0;; # let init re-exec us
		f*|F*) break;;
	}
	done #}
done #}

# remove INST from OSLoadOptions nvram variable

	oval="`nvram OSLoadOptions`"
	case "$oval" {
		INST*) oval="inst${oval#INST}"
			nvram OSLoadOptions "$oval" ;;
	}


shut_it_down 1

# } end of MAIN
