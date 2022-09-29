#! /bin/sh
#
# @(#)ypinit.sh	1.3 90/07/23 4.1NFSSRC SMI
#
# ypinit.sh - 
#	set up a populated NIS directory structure on a master or slave server
#

# set -xv

yproot_dir=/var/yp
XFR=/usr/sbin/ypxfr
hf=/tmp/ypinit.hostlist.$$
YPM=${YPMAKE-$yproot_dir/ypmake}
chkconfig=/sbin/chkconfig

masterp=F
slavep=F
host=""
def_dom=""
master=""
got_host_list=F
exit_on_error=F
errors_in_setup=F

PATH=$PATH:$yproot_dir
export PATH 

trap "rm -f $hf; exit 0" 2 15 

usage()
{
	exec 1>&2
	echo 'usage:'
	echo '	ypinit -m'
	echo '	ypinit -s master_server'
	echo ""
	echo "\
where -m is used to build a master NIS server data base, and -s is used for"
	echo "\
a slave data base.  master_server must be an existing reachable NIS server."
	exit 1
}

case $# in
1)	case $1 in
	-m)	masterp=T;;
	*)	usage;;
	esac;;

2)	case $1 in
	-s)	slavep=T; master=$2;;
	*)	usage;;
	esac;;

*)	usage;;
esac


if [ $slavep = T ]
then
	maps=`ypwhich -m | egrep $master$| awk '{ printf("%s ",$1) }' -`
	if [ -z "$maps" ]
	then
		echo "\nCan't enumerate maps from $master. Please check that $master is running." 1>&2
		echo "Please make sure ypbind is running on this host." 1>&2
		exit 1
	fi
fi

host=`hostname`

if [ $? -ne 0 ]
then 
	echo "Can't get local host's name.  Please check your path." 1>&2
	exit 1
fi

if [ -z "$host" ]
then
	echo "The local host's name hasn't been set.  Please set it." 1>&2
	exit 1
fi

def_dom=`domainname`

if [ $? -ne 0 ]
then 
	echo "Can't get local host's domain name.  Please check your path." 1>&2
	exit 1
fi

if [ -z "$def_dom" ]
then
	echo "The local host's domain name hasn't been set.  Please set it." 1>&2
	exit 1
fi

domainname $def_dom

if [ $? -ne 0 ]
then 
	echo "\
You have to be the superuser to run this.  Please log in as root." 1>&2
	exit 1
fi

if [ ! -d $yproot_dir -o -f $yproot_dir ]
then
    echo "\
The directory $yproot_dir doesn't exist.  Restore it from the distribution." 1>&2
	exit 1
fi

if [ $slavep = T ]
then
	if [ $host = $master ]
	then
		echo "\
The host specified should be a running master NIS server, not this machine." 1>&2
		exit 1
	fi
fi

if [ "$setup" != "yes" ]; then
	echo "Installing the NIS data base will require that you answer a few questions."
	echo "Questions will all be asked at the beginning of the procedure."
	echo ""
	echo "Do you want this procedure to quit on non-fatal errors? [y/n: n]  \c"
	read doexit
else
	doexit=yes
fi

case $doexit in
y* | Y*)	exit_on_error=T;;
*)	echo "\
OK, please remember to go back and redo manually whatever fails.  If you"
	echo "\
don't, some part of the system (perhaps the NIS itself) won't work.";;
esac

echo ""

for dir in $yproot_dir/$def_dom
do

	if [ -d $dir ]; then
		if [ "$setup" != "yes" ]; then
			echo "Can we destroy the existing $dir and its contents? [y/n: n]  \c"
			read kill_old_dir
		else 
			kill_old_dir=yes
		fi

		case $kill_old_dir in
		y* | Y*)	rm -r -f $dir

			if [ $?  -ne 0 ]
			then
			echo "Can't clean up old directory $dir.  Fatal error." 1>&2
				exit 1
			fi;;

		*)    echo "OK, please clean it up by hand and start again.  Bye"
			exit 0;;
		esac
	fi

	mkdir $dir

	if [ $?  -ne 0 ]
	then
		echo "Can't make new directory $dir.  Fatal error." 1>&2
		exit 1
	fi

done

if [ $slavep = T ]
then

	echo "\
There will be no further questions. The remainder of the procedure should take"
	echo "a few minutes, to copy the data bases from $master."

	for dom in  $def_dom
	do
		for map in $maps
		do
			echo "Transferring $map..."
			if $chkconfig ypsecuremaps
			then
				/sbin/suattr -C CAP_PRIV_PORT+ep -c \
				    $XFR' -h '$master' -c -S -d '$dom' '$map
			else
				$XFR -h $master -c -d $dom $map
			fi

			if [ $?  -ne 0 ]
			then
				errors_in_setup=T

				if [ $exit_on_error = T ]
				then
					exit 1
				fi
			fi
		done
	done

	echo ""

	if [ $errors_in_setup = T ]
	then
		echo "${host}'s NIS database has been set up with errors." 1>&2
		echo "Please remember to figure out what went wrong, and fix it." 1>&2
	else
		echo "${host}'s NIS database has been set up without any errors."
		if $chkconfig ypserv; then :
		else
		    echo ""
		    echo "Start NIS slave daemons during system startup? [y/n: y] \c"
		    read startyp
		    case $startyp in
			n* | N*)
			    ;;
			*)
			    $chkconfig -f ypserv on
			    $chkconfig -f yp on
			    ;;
		    esac
		fi
	fi

	echo ""
	echo "\
At this point, make sure that /etc/passwd, /etc/hosts, /etc/networks,"
	echo "\
/etc/group, /etc/protocols, /etc/services, /etc/rpc and /etc/netgroup have"
	echo "\
been edited so that when the NIS is activated, the data bases you"
	echo "\
have just created will be used."

	exit 0
else

	rm -f $yproot_dir/*.time
	touch $yproot_dir/ypmake.log.old # prevent warning first time ypmake runs

	while [ $got_host_list = F ]; do
		echo $host >$hf
		if [ "$setup" != "yes" ]; then
			echo ""
			echo "\
At this point, we have to construct a list of the hosts which will run NIS"
			echo "\
servers.  $host is in the list of NIS server hosts."
			echo "\
Please continue to add the names for the other hosts, one per line."
			echo "\
When you are done with the list, type a <control-D>."
			echo "	next host to add:  $host"
			echo "	next host to add:  \c"

			while read h
			do
				echo "	next host to add:  \c"
				if test -z "$h"; then
				    :
				else
				    echo $h >>$hf
				fi
			done

			echo ""
			echo "The current list of NIS servers looks like this:"
			echo ""

			cat $hf
			echo ""
			echo "Is this correct?  [y/n: y]  \c"
			read hlist_ok

			case $hlist_ok in
			n* | N*)
				got_host_list=F
				echo "Let's try the whole thing again...";;
			*)	got_host_list=T;;
			esac
		else 
			got_host_list=T
		fi
	done

	echo "\
There will be no further questions. The remainder of the procedure should take"
	echo "5 to 10 minutes."

	mv $hf $yproot_dir/ypservers
	if [ $?  -ne 0 ]
	then
		echo "\
Couldn't create NIS $yproot_dir/ypservers file." 1>&2 
		errors_in_setup=T

		if [ $exit_on_error = T ]
		then
			exit 1
		fi
	fi

	in_pwd=`pwd`
	cd $yproot_dir
	echo "Building NIS databases:"
	$YPM NOPUSH=1 

	if [ $?  -ne 0 ]
	then
		echo "\
Error running NIS makefile." 1>&2
		errors_in_setup=T
		
		if [ $exit_on_error = T ]
		then
			exit 1
		fi
	fi

	cd $in_pwd
	echo ""

	if [ $errors_in_setup = T ]
	then
		echo "$host has been set up as the NIS master server with errors." 1>&2
		echo "Please remember to figure out what went wrong, and fix it." 1>&2
	else
		echo "$host has been set up as the NIS master server without any errors."

		if $chkconfig ypmaster; then :
		else
		    echo ""
		    echo "Start NIS master daemons during system startup? [y/n: y] \c"
		    read startyp
		    case $startyp in
			n* | N*)
			    ;;
			*)
			    $chkconfig -f ypmaster on
			    $chkconfig -f ypserv on
			    $chkconfig -f yp on
			    ;;
		    esac
		fi
	fi

	echo ""
	echo "\
If there are running slave NIS servers, run yppush now for any data bases"
	echo "\
which have been changed.  If there are no running slaves, run ypinit on"
	echo "\
those hosts which are to be slave servers."

fi
