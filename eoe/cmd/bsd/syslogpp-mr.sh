#!/bin/sh 

# Filter out the SYSLOG lines generated every time
# syslogd is stopped and restarted in the miniroot
# since this will be a common occurrence.

read line
case "$line" {
	syslogd:*restart* | syslogd:*signal\ 15* )
		exit 0
		;;
}

echo $line
exit 0
