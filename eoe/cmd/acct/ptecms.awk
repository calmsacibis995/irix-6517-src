#!/usr/bin/awk -f


#ident	"@(#)acct:common/cmd/acct/ptecms.awk	1.6.5.2"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/acct/RCS/ptecms.awk,v 1.2 1993/11/05 04:27:24 jwag Exp $"

BEGIN {
	MAXCPU = 20.0		# report if cpu usage greater than this
	MAXKCORE = 1000.0	# report if KCORE usage is greater than this
}

NF == 4	{ print "\t\t\t\t" $1 " Time Exception Command Usage Summary" }

NF == 3	{ print "\t\t\t\tCommand Exception Usage Summary" }

NR == 1	{
	MAXCPU = MAXCPU + 0.0
	MAXKCORE = MAXKCORE + 0.0
	print "\t\t\t\tTotal CPU > " MAXCPU " or Total KCORE > " MAXKCORE
}

NF <= 4 && length != 0	{ next }

$1 == "COMMAND" || $1 == "NAME"	{ print; next }

NF == 10 && ( $4 > MAXCPU || $3 > MAXKCORE ) && $1 != "TOTALS"

NF == 13 && ( $5 + $6 > MAXCPU || $4 > MAXKCORE ) && $1 != "TOTALS"

length == 0


