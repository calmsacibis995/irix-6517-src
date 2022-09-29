#!/bin/sh
#
#	NetVisualyzer Installation Script for Sun Data Station Product
#
#	$Revision: 1.5 $
#

VERSION=2.0
DATA_STATION=""
INSTPATH=""
SGI_SNOOPD=391000

#
# Check if sgi_snoopd rpc number is registered
#
check_rpc() {
	YP=`ypwhich | fgrep "not running ypbind"`
	if [ "$YP" != "" ]
	then
		# No NIS, use local files
		YN=""
		while [ "$YN" != "y" ]
		do
			echo
			echo "The following line must be added to /etc/rpc on `hostname`:"
			echo
			echo "sgi_snoopd      $SGI_SNOOPD  snoopd snoop"
			echo
			echo "Add this line on `hostname` now? (y or n)"
			echo -n "[y] -> "
			read YN
			YN=${YN:=y}
			case $YN in
				y*)
					YN=y
					break;;
				n*)
					return;;
				*)
					echo "Enter y or n please."
					echo
			esac
		done
		echo -n "Adding lines to /etc/rpc..."
		echo "sgi_snoopd      $SGI_SNOOPD  snoopd snoop" >> /etc/rpc
		if [ "$?" -ne 0 ]
		then
			echo "Could not the add lines.  Please do so by hand."
			exit 0
		fi
		echo "successful."
		RESTART="y"
		return
	fi

	# Using NIS.
	STATUS=`ypmatch $SGI_SNOOPD rpc.bynumber`
	if [ "$STATUS" = "sgi_snoopd $SGI_SNOOPD snoopd snoop" ]
	then
		return
	fi
	MASTER=`ypwhich -m | awk '$1 == "rpc.bynumber" { print $2 }'`
	if [ "$MASTER" != "" ]
	then
		echo
		echo "The following line must be added to the rpc.bynumber map on the NIS master $MASTER:"
		echo
		echo "sgi_snoopd      $SGI_SNOOPD  snoopd snoop"
		echo
	fi
	return
}

#
# Check if /etc/inetd.conf contains sgi_snoopd
#
check_inetd() {
	STATUS=`fgrep rpc.snoopd /etc/inetd.conf`
	if [ "$STATUS" != "" ]
	then
		return
	fi
	echo
	echo "The following line must be added to /etc/inetd.conf on `hostname`:"
	echo
	echo "sgi_snoopd/1 stream rpc/tcp wait    root    /usr/etc/rpc.snoopd     snoopd"
	echo
	YN=""
	while [ "$YN" != "y" ]
	do
		echo "Add this line now? (y or n)"
		echo -n "[y] -> "
		read YN
		YN=${YN:=y}
		case $YN in
			y*)
				YN=y
				break;;
			n*)
				return;;
			*)
				echo "Enter y or n please."
				echo
		esac
	done
	echo
	echo -n "Adding lines to /etc/inetd.conf..."
	echo "# NetVisualyzer snoopd" >> /etc/inetd.conf
	echo "sgi_snoopd/1 stream rpc/tcp wait    root    /usr/etc/rpc.snoopd     snoopd" >> /etc/inetd.conf
	if [ "$?" -ne 0 ]
	then
		echo "Could not the add lines.  Please do so by hand."
		exit 0
	fi
	echo "successful."
	RESTART="y"
}

restart_inetd() {
	echo -n "Restarting inetd..."
	ps -ax | awk '$5 == "inetd" {print $1}' | xargs kill -HUP
	if [ "$?" -ne 0 ]
	then
		echo "Could not restart inetd.  Please refer to the release notes."
		exit 0
	fi
	echo "successful."
}

#
# START
#
if uname | fgrep -s IRIX
then
	clear
	echo
	echo
	echo "		NetVisualyzer Sun Data Station Product"
	echo "			SGI Remote Installation"
	echo
	echo

	# Get name of Data Station
	while [ "$DATA_STATION" = "" ]
	do
		echo "Enter the name of the Sun Data Station to install:"
		echo "[] -> \c"
		read DS
		if [ "$DS" != "" ]
		then
			echo
			echo "Testing login to $DS...\c"
			rsh $DS -n "echo testing > /dev/null"
			if [ "$?" -eq 0 ]
			then
				echo "successful."
				DATA_STATION=$DS
			else
				echo "failed."
			fi
		fi
		echo
	done

	# Get path
	while [ "$INSTPATH" = "" ]
	do
		echo "Enter the path where you would like the software installed:"
		echo "[/usr/netvis] -> \c"
		read P
		P=${P:=/usr/netvis}
		STATUS=`rsh $DATA_STATION -n "test -d $P || echo ok`
		if [ "$STATUS" = "ok" ]
		then
			echo
			echo "Creating directory $P on $DATA_STATION...\c"
			STATUS=`rsh $DATA_STATION -n "mkdir $P && echo ok"`
			if [ "$STATUS" = "ok" ]
			then
				echo "successful."
				INSTPATH=$P
			else
				echo "failed."
			fi
		else
			echo
			echo "Directory $P already exists on $DATA_STATION.  Continue? (y or n)"
			echo "[y] -> \c"
			read YN
			YN=${YN:=y}
			case $YN in
				y*)
					INSTPATH=$P
					break;;
				n*)
					;;
				*)
					echo "Enter y or n please."
			esac
		fi
	done

	# Install
	echo
	echo "Copying to $DATA_STATION..."
	cat netvis.tar | rsh $DATA_STATION "(cd $INSTPATH; tar xvBf -)"
	if [ "$status" -eq 0 ]
	then
		echo "...copy to $DATA_STATION successful."
	else
		echo "Installation failed!"
		echo
		echo "Please check your responses and try again."
		exit 1
	fi

	# Run installation script on Sun to set permissions, etc.
	echo
	echo "Installing on $DATA_STATION..."
	rsh $DATA_STATION "cd $INSTPATH; nvinst sunlinks"
	STATUS=`rsh $DATA_STATION -n "test -f $INSTPATH/version && echo ok"`
	if [ "$STATUS" = "ok" ]
	then
		echo "...install on $DATA_STATION successful."
	else
		echo "Installation failed!"
		echo
		echo "Please check your responses and try again."
		exit 1
	fi
	echo
	echo "Installation completed successfully."
	exit 0
else
	if [ "$1" = "sunlinks" ]
	then
		# Create links
		echo -n "Linking rpc.snoopd into /usr/etc..."
		rm -rf /usr/etc/rpc.snoopd
		STATUS=`/usr/5bin/ln -fs $PWD/rpc.snoopd /usr/etc && echo ok`
		if [ "$STATUS" = "ok" ]
		then
			echo "successful."
		else
			echo "Installation failed!"
			exit 1
		fi
		if [ -f /usr/etc/rpc.snoopd.auth ]
		then
			echo "The file /usr/etc/rpc.snoopd.auth exists, it will not be replaced."
		else
			echo -n "Linking rpc.snoopd.auth into /usr/etc..."
			STATUS=`/usr/5bin/ln -fs $PWD/rpc.snoopd.auth /usr/etc && echo ok`
			if [ "$STATUS" = "ok" ]
			then
				echo "successful."
			else
				echo "Installation failed!"
				exit 1
			fi
		fi

		# Create a version stamp
		echo -n "Creating version stamp..."
		echo "Version $VERSION installed `date`." > version
		if [ "$?" -eq 0 ]
		then
			echo "successful."
		else
			echo "Installation failed!"
			exit 1
		fi

		# Test if /etc/rpc contains rpc.snoopd
		RESTART="n"
		check_rpc

		# Test if inetd.conf contains rpc.snoopd
		check_inetd

		# Restart inetd if necessary
		if [ "$RESTART" = "y" ]
		then
			restart_inetd
		fi
		exit 0
	fi

	clear
	echo
	echo
	echo "		NetVisualyzer Sun Data Station Product"
	echo "			Sun Local Installation"
	echo
	echo

	# Get path
	while [ "$INSTPATH" = "" ]
	do
		echo "Enter the path where you would like the software installed:"
		echo -n "[/usr/netvis] -> "
		read P
		P=${P:=/usr/netvis}
		if [ ! -d $P ]
		then
			echo
			echo -n "Creating directory $P..."
			STATUS=`mkdir $P && echo ok`
			if [ "$STATUS" = "ok" ]
			then
				echo "successful."
				INSTPATH=$P
			else
				echo "failed."
			fi
		else
			echo
			echo "Directory $P already exists.  Continue? (y or n)"
			echo -n "[y] -> "
			read YN
			case $YN in
				y*)
					INSTPATH=$P
					break;;
				n*)
					;;
				*)
					echo "Enter y or n please."
			esac
		fi
	done

	# Install
	echo
	echo "Copying to $INSTPATH..."
	cat netvis.tar | (cd $INSTPATH; tar xvBf -)
	if [ "$status" -eq 0 ]
	then
		echo "...copy to $INSTPATH successful."
	else
		echo "Installation failed!"
		echo
		echo "Please check your responses and try again."
		exit 1
	fi

	# Create links
	echo
	echo -n "Linking rpc.snoopd into /usr/etc..."
	rm -rf /usr/etc/rpc.snoopd
	STATUS=`/usr/5bin/ln -fs $INSTPATH/rpc.snoopd /usr/etc && echo ok`
	if [ "$STATUS" = "ok" ]
	then
		echo "successful."
	else
		echo "Installation failed!"
		exit 1
	fi
	if [ -f /usr/etc/rpc.snoopd.auth ]
	then
		echo "The file /usr/etc/rpc.snoopd.auth exists, it will not be replaced."
	else
		echo -n "Linking rpc.snoopd.auth into /usr/etc..."
		STATUS=`/usr/5bin/ln -fs $INSTPATH/rpc.snoopd.auth /usr/etc && echo ok`
		if [ "$STATUS" = "ok" ]
		then
			echo "successful."
		else
			echo "Installation failed!"
			exit 1
		fi
	fi

	# Create a version stamp
	echo
	echo -n "Creating version stamp..."
	echo "Version $VERSION installed `date`." > $INSTPATH/version
	if [ "$?" -eq 0 ]
	then
		echo "successful."
	else
		echo "Installation failed!"
		exit 1
	fi

	# Test if /etc/rpc contains rpc.snoopd
	RESTART="n"
 	check_rpc

	# Test if inetd.conf contains rpc.snoopd
	check_inetd

	# Restart inetd if necessary
	if [ "$RESTART" = "y" ]
	then
		restart_inetd
	fi

	echo
	echo "Installation completed successfully."
	exit 0
fi
