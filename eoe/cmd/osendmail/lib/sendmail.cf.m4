############################################################################
############################################################################
############################################################################
#####
#####			SENDMAIL CONFIGURATION FILE
#####		       WITH AUTOCONFIGURATION SUPPORT
#####
`#####  This file is designed to work with SGI sendmail version 'dnl
SENDMAIL_VERSION_NAME
ifdef(`SGI_INTERNAL',`#####  It was generated on 'dnl
GENERATED_DATE `on 'dnl
GENERATED_HOSTNAME
`#####  This file contains customizations for the SGI internal network
')dnl
############################################################################
############################################################################
############################################################################
#
#  Copyright (c) 1983 Regents of the University of California.
#  All rights reserved.  The Berkeley software License Agreement
#  specifies the terms and conditions for redistribution.
#
# "1.1"
#
# To install this file:
#
#       1) Review the rules preceded by the "??? unusual rule" comment.
#	2) Run the command "/etc/init.d/mail start" to restart the
#	   sendmail daemon.
#       3) Run the command "/usr/etc/configmail setup" to review and, if
#	   necessary, adjust the most important configuration settings.
#
# This file works in conjunction with the sendmail autoconfiguration script
# configmail(1M).  When sendmail parses this file, it will be directed to
# call configmail to obtain the values for all critical macros and classes.
# These values will be calculated at runtime by configmail.
#
# For most mail environments, the calculated default values returned by
# configmail should be sufficient to provide a working sendmail configuration.
# In many cases, this file will allow sendmail to "configure itself" and begin
# working without any user intervention.
#
# In some situations, it may still be necessary or desirable to manually
# configure some of the critical configuration parameters.  It should not,
# however, be necessary to make any changes to this file in order to make
# basic configuration adjustments.  If you want to modify the default
# configuration, verify the current configuration parameter settings,
# or simply get more information about the various configuration parameters
# themselves, see the configmail(1M) manual page.
#
# This file and the accompanying configmail script are provided as an
# example of how to use sendmail's new ability to read macro and class
# definitions from pipes to simplify and automate the sendmail configuration
# process.  Sophisticated sendmail users may wish to expand upon the basic
# scheme presented here.
#
# The remainder of this file is a nearly identical copy of the "default"
# suggested configuration file.  The only difference is that this file
# reads the values of the following macros and classes from the output of
# the configmail script, while the "default" configuration file must be
# edited by hand.
#
# Definitions of user configurable macros and classes:
#
#	D macro and class (local (D)omain name)
#		The D macro defines the local domain name.
#		Define this macro to contain the name of the domain in
#		which this host resides.  If no domains are used, you
#		should make sure that this macro is left empty or comment
#		it out.
#
#		The D class explicitly lists all domains to which this
#		host should send mail directly via the ethernet mailer.
#
#		Mail destined for domains not listed in the D class will
#		be sent via an exchanger, or relay host associated
#		with the destination host in preference to sending it
#		directly to the recipient host.
#
#		Mail destined for domains listed in the D class will be
#		sent directly to the destination host if possible.  No
#		attempt will be made to send the mail via an exchanger,
#		or relay host.
#
#		If this host is a relay or exchanger for a particular
#		domain, that domain should appear in the D class.
#
#		If this host is not an exchanger or relay, this class
#		may be left empty.
#
#	F macro and class ((F)orwarder hostname)
#		The F macro defines the name or alias of the host to which
#		this host will send all mail to unknown hosts or domains.
#		It is strongly suggested that the F macro contiain the
#		fully qualified domain name (FQDN) of the forwarder host.
#
#		The F class contains all known names for the host defined
#		in the F macro.
#
#		Mail will only sent to the forwarder as a last resort in
#		the event that:
#
#			a) this host cannot canonicalize the destination
#			   hostname or determine an appropriate relay or mail
#			   exchanger to deal with it.
#
#			- or -
#
#			b) the appropriate host or relay or exchanger
#			   to which this host will be sending the message
#			   turns out to exist outside of the top level
#			   domain (see the T macro below).
#
#		If no such host exists, the F macro and class should be
#		left empty.  If this host is the forwarder host, the F macro
#		should contain this host's full hostname while the F class
#		should contain all known names for this host.
#
#		In the event that this host is the forwarder host and one
#		of the above conditions for sending mail on to the forwarder
#		host is met, this host will put messages to unknown hosts or
#		domains out "on the wire" and hope for the best.
#
#	T macro ((T)op level domain)
#		This macro defines the name of the top level of the local
#		domain space.  For example, if this host resides in
#		some subdomain BAR.FOO.COM under the FOO.COM domain, and
#		if all hosts under the FOO.COM domain or any subdomain
#		under the FOO.COM domain are to be considered internal
#		hosts, you would define the T macro to be FOO.COM.
#
#		The top level domain is used in conjunction with the
#		definition of the forwarder host defined in the F macro
#		and class above.  All mail destined to hosts "outside" the
#		top level domain will be sent via the forwarder host.
#
#	K class ((K)illed hosts in the local domain)
#		This class is a list of all known "killed" or "dead" hosts
#		in the local domain.  This list is generally used by relay
#		or exchanger machines to detect mail to no-longer-existent
#		hosts.  If a message is received that is destined for one of
#		these hosts, the dead hostname will be stripped from the
#		address, and the resulting address will be used to send the
#		mail.  In common practice, this means that mail addressed
#		to "user@some.listed.dead.host" will resolve to just "user".
#		The subsequent aliasing step may then expand "user" into
#		something like "user@his.new.host".
#
#	P macro ((P)athalias database)
#		This macro defines the location of the pathalias database.
#
#	w class (Alternate names for the local host)
#		This class is a list of all the names by which the local host
#		is known.  This list is pre-set by the sendmail program to
#		contain the h_name and all h_aliases of the local host as
#		returned by gethostbyname(3N).  If additional aliases are
#		needed for proper mail handling, you should add them to
#		the w class.  Mail addressed to any of the hosts in this list
#		will be treated exactly like mail addressed directly to the
#		local host.


#####################################
# Parameters which MUST be defined. #
#####################################

#### D macro and class #####################################################
# D macro: Our local domain name.
# If domains are not used, delete or comment out this line.
DD|/usr/etc/configmail get localdomain

# D class: Domains into which we should send mail directly.
# Explicit list of all domains into which we can and should send mail
# directly (without the use of any forwarder, relay, or exchanger hosts.)
# If this host is a relay or exchanger for a particular domain, that domain
# should be listed here, otherwise this class may be left empty.
FD|/usr/etc/configmail get directdomains

#### F macro and class #####################################################
# F macro: Forwarder hostname.
# The F macro must contain the name of the forwarder host.
# If no such host exists this macro should be empty.  If this host is the
# forwarder, this macro should be defined to contain this host's name.
DF|/usr/etc/configmail get forwarder

# F class: Alternate forwarder hostnames.
# The F class should contain all known hostnames for the forwarder host
# defined above.  If the F macro is empty, the F class should be empty as
# well.
FF|/usr/etc/configmail get forwarder
FF|/usr/etc/configmail get forwarder -s

#### T macro and class #####################################################
# T macro: Our top-level domain name.
# Defines the top level of our local domain space.  Mail to any machines
# which exist in this domain or any sub-domain under this domain will be
# considered local or internal (as opposed to foreign).  It will be
# considered O.K. to query for relay hosts in this domain or any sub-domain
# under this domain.
#
# Mail to any machine which does not exist in this domain or any sub-domain
# under this domain will be sent to the forwarder host (defined above) for
# further disposition.
#
# If you do not use domain addressing, this macro should be left blank.
DT|/usr/etc/configmail get rootdomain


#### Q macro and class #####################################################
# Q class: No rewrite class.  (Support for Gauntlet email configuration.)
# Defines the direct domains that a relay machine is not to rewrite for
# outbound electronic mail.  For example, mail originating from a local
# domain, passing through an Internet Relay might rewrite the From: line
# to user@ficticious.domain.  If a local subdomain is defined in class Q,
# such as sub.ficticious.domain, the From: line will remain intact (only
# a host name would be stripped off, e.g. user@machine.sub.ficticious.domain 
# would become user@sub.ficticious.domain.
FQ|/usr/etc/configmail get recsubdomains | /usr/bin/tr "\011\040" "\012\012"

############################################
# Parameters which MAY need to be defined. #
############################################

#### K class ###############################################################
# Killed machines within the local domain:  These machines have been
# permanently turned off.  All mail for them should be redirected to the
# forwarder.  This class is used by domain forwarders to redirect mail
# to known bad, dead, renamed, etc. hosts.  This class only needs to be
# defined on domain forwarders.
#
# Note that this class can be defined inline via the CK command and/or
# may be read from the sendmail.killed file.
FK|/usr/etc/configmail get deadhosts

# Additionally for the K class...
# Many machines generate UUCP return paths which include '!somewhere!' 
# when they get confused.  It is a good idea to make sure that your
# K class contains at least 'somewhere'.
CKsomewhere

#### P macro ###############################################################
# Look here for path-alias database.
# Uncomment this if you are using pathalias
#DP|/usr/etc/configmail get pathalias

#### w class ###############################################################
# Alternate names by which this host may be known.
# This class is pre-set to contain the h_name and all h_aliases of this host
# as returned by gethostbyname(3N).  If additional aliases are required,
# they may be added here.
Cw

#############################################################
# Other Parameters which come pre-configured and SHOULD NOT #
# need to be redefined.                                     #
#############################################################

# Official hostname
Dj$w

# R macro: Relay hostname.
# This macro defines the hostname (or an alias) used by
# all hosts which act as relay machines.  Relay machines
# are "forwarders" to known internal domains and are themselves
# defined by the use of this relay hostname as their hostname
# or alias.
#
# This macro comes pre-configured as "relay" which is strongly
# suggested.
#
# This macro must not be left blank although it is not necessary
# for any actual relay machines to be configured in the
# network.
#
# Mail relay hosts implement a sort of "poor man's" MX scheme.
# They may also be useful as an emergency "back-up" to the use
# of MX records.
DR|/usr/etc/configmail get relayname

# T class: Our top-level domain name.
# Must be identical to the definition of the T macro above or all hell
# will break loose.  Defined via the T macro, do not touch!
CT$T

# V class: UUCP machines
# This class is the list of all machines to which we can send mail
# via UUCP
FV-o /etc/uucp/Systems %[-_a-zA-Z0-9]

# N class: UUCP machines that understand domains
# This class is the list of all machines to which we can send mail
# via UUCP and which understand domain-style addressing.  We don't
# need to rewrite addresses into uucp-style when mailing to these
# machines.
FN-o /etc/uucp/Systems.domain-machines

#######################
### Initialize maps ###
#######################

ifelse(eval(SENDMAIL_VERSION >= 80700),1,
`# Initialize the bestmx map (sendmail 8.7.0+)
Kbestmx bestmx

# Initialize the dequote map
Kdequote dequote

')dnl


######################
### Version Number ###
######################

DZ980728.SGI.AUTOCF

###########################
## Used for lookup logic ##
###########################

DYFAIL
CY$Y

##########################
##### Special macros #####
##########################

# my name
DnMAILER-DAEMON

# UNIX header format
DlFrom $g  $d

# delimiter (operator) characters
Do.:%@!^=/[]

# format of a total name
Dq$g$?x ($x)$.

# SMTP login message - WARNING - Do not remove the space before $j
De $j Sendmail $v/$Z ready at $b


###################
###   Options   ###
###################

# Location of local alias file.   The order of this is significant.  
# Sources with aliases that are correct for this host should precede network
# sources such as nis or nsd.
ifelse(eval(IRIX_VERSION >= 60500),1,
`OA/etc/aliases
# The nsd nameservice daemon can provide aliases.
# To enable this, uncomment the following line, and comment +:+ in /etc/aliases
#OAnsd:mail.aliases',
`OA/etc/aliases')

ifelse(eval(IRIX_VERSION >= 60500),1,
`ifelse(eval(SENDMAIL_VERSION >= 80808), 1,
`# Location of the userDB file
#O UserDatabaseSpec=/etc/userdb.db')')

# wait up to x (originally ten) minutes for alias file rebuild
Oa9

# substitution for space (blank) characters
OB.

# (don't) connect to "expensive" mailers
#Oc

# default delivery mode (deliver in background)
Odbackground
#Odqueue

# temporary file mode
OF0600

# default UID
Ou998

# default GID
Og998

# do "fuzzy" matching on GECOS field
OG

# location of help file
OH/etc/sendmail.hf

# log level (messages sent to SYSLOG)
#
#	0: No logging.
#	1: Serious system failures and potential security problems.
#	2: Network problems and protocol failures.
#	3: Forwarding and received message errors.
#	4: Minor errors.
#	5: Received messages/message collection stats.
#	6: Creation of error messages, VRFY and EXPN commands.
#	7: Message delivery failures.
#	8: Successful deliveries.
#	9: Messages being deferred (due to a host being down, etc.).
#	10: Alias/forward expansion.
#	12: Connecting hosts.
#	20: Attempts to run locked queue files.
#
#	Note: Each level logs its own information plus all information
#	      logged by lower levels.
#
OL|/usr/etc/configmail get loglevel

# default messages to old style
Oo

# queue directory
OQ/var/spool/mqueue

# read timeout -- violates protocols
Or2h

# status file
OS/var/sendmail.st

# queue up everything before starting transmission
Os

# default timeout interval
OT7d

# time zone (set w/ no value is correct setting on IRIX)
Ot

# Attempt to deliver to A record if we are the best MX 
Ow

# This version of sendmail adds the number of sendmail processes to the
# load avg.   To disable this, add ",l" to a the numeric setting of the
# Ox or OX option.   (eg: Ox20,l)

ifelse(eval(SENDMAIL_VERSION >= 80805),1,
`# load average at which we just queue messages -- scaled by number of CPUs
# SGI sendmail 8.8.x specific
Ox|/usr/etc/configmail percpu 10',
`# load average at which we just queue messages
Ox20')

ifelse(eval(SENDMAIL_VERSION >= 80805),1,
`# load average at which we refuse connections -- scaled by number of CPUs
# SGI sendmail 8.8.x specific
OX|/usr/etc/configmail percpu 12',
`# load average at which we refuse connections
OX25')

# rebuild alias file
OD

# Up the Listen queue
OOL=1000

# ??? unusual rule
# Define NIS mail.byaddr (reverse alias) file.
# Also see ruleset 11 below for use of this database.
#KA nis mail.byaddr

###############################
###   Message precedences   ###
###############################

Pfirst-class=0
Pspecial-delivery=100
Pbulk=-60
Pjunk=-100

#########################
###   Trusted users   ###
#########################

Troot
Tdaemon
Tuucp
Tuucpadm

#############################
###   Format of headers   ###
#############################

HReceived: $?sfrom $s $.$?_($?s$|from $.$_) $.by $j ($v/$Z)$?r via $r$. id $i$?u for $u$.; $b
H?P?Return-Path: <$g>
H?D?Resent-Date: $a
H?D?Date: $a
H?F?Resent-From: $q
H?F?From: $q
H?x?Full-Name: $x
HSubject:
# HPosted-Date: $a
# H?l?Received-Date: $b
H?M?Resent-Message-Id: <$t.$i@$j>
H?M?Message-Id: <$t.$i@$j>
# H?R?Return-Receipt-To: $g


#############################
###   Anti-spam support   ###
#############################
#
# Sendmail 8.8.8 has a number of features that allow for the control of 
# Unsolicited Commercial Email (spam).
# There is no way to build a correct anti-spam ruleset that will satisify
# everyones needs.  The best place to look for the current set of rules
# and techniques known is http://www.sendmail.org/antispam.html
#

ifelse(eval(SENDMAIL_VERSION >=80806),1,
`# The following rule is an example of how to control relaying of spam
# through your system.  You can uncomment the check_rcpt ruleset and use it
# or install more powerful and special purpose filters in a similar manner.
# 
# This version of the check_rcpt ruleset requires you to uncomment the
# Parse0 ruleset as well.
#
# It will reject any mail that does not originate or terminate in a
# in a local domain.   The list of local domains should be listed one domain
# per line in the file /etc/sendmail.cR

## domains that are not us but which we will relay
#FR-o /etc/sendmail.cR

#CO% @ : !

#Scheck_rcpt
## anything terminating locally is ok
#R$*			$: $>Parse0 $>3 $1
#R$+ < @ $* . > $*	$: $1 < @ $2 >
#R$+ < @ $=w >		$@ OK
#R$+ < @ $* $=R >	$@ OK

## anything originating locally is ok
#R$*			$: $(dequote "" $&{client_name} $)
#R$=w			$@ OK
#R$* $=R			$@ OK
#R$@			$@ OK

## anything else is bogus
#R$*			$#error $: "550 Relaying Denied"

## 
## END of Scheck_rcpt anti-spam ruleset
##

##
## Parse0.  Borrowed from sendmail.org sendmail-8.8.8 sendmail.cf m4 macros
##
#SParse0
#R<@>			$#local $: <@>		special case error msgs
#R$* : $* ; <@>		$#error $@ 5.1.3 $: "list:; syntax illegal for recipient addresses"
#R<@ $+>			$#error $@ 5.1.1 $: "user address required"
#R$*			$: <> $1
#R<> $* < @ [ $+ ] > $*	$1 < @ [ $2 ] > $3
#R<> $* <$* : $* > $*	$#error $@ 5.1.1 $: "colon illegal in host name part"
#R<> $*			$1
#R$* < @ . $* > $*	$#error $@ 5.1.2 $: "invalid host name"
#R$* < @ $* .. $* > $*	$#error $@ 5.1.2 $: "invalid host name"

## handle numeric address spec
#R$* < @ [ $+ ] > $*	$: $>98 $1 < @ [ $2 ] > $3	numeric internet spec
#R$* < @ [ $+ ] > $*	$#esmtp $@ [$2] $: $1 < @ [$2] > $3	still numeric: send

## now delete the local info -- note $=O to find characters that cause forwarding
#R$* < @ > $*		$@ $>Parse0 $>3 $1		user@ => user
#R< @ $=w . > : $*	$@ $>Parse0 $>3 $2		@here:... -> ...
#R$- < @ $=w . >		$: $(dequote $1 $) < @ $2 . >	dequote "foo"@here
#R< @ $+ >		$#error $@ 5.1.1 $: "user address required"
#R$* $=O $* < @ $=w . >	$@ $>Parse0 $>3 $1 $2 $3	...@here -> ...

#R$- < @ $=w  >		$: $(dequote $1 $) < @ $2 >	dequote "foo"@here
#R$* $=O $* < @ $=w >	$#error $@ 5.1.1 $: "Invalid user name""

##
## End of SParse0
##')

###########################
###   Rewriting rules   ###
###########################

#############################################################
# insert this handy debugging line wherever you have problems
#R$*				$:$>99$1

######################################
#  Debugging ruleset - Leave empty.  #
######################################
S99

################################
#  Sender Field Pre-rewriting  #
################################
S1

###################################
#  Recipient Field Pre-rewriting  #
###################################
S2

#################################
#  Final Output Post-rewriting  #
#################################
S4

R@			$@				handle <> error addr

# convert mixed UUCP and domains into UUCP
#	but only in some simple cases where it is not wrong
R$+!$+<@$*>		$:$>5$1!$2<@$3>

R$*<$+>$*		$1$2$3				defocus
R@$+:@$+:$+		@$1,@$2:$3			<route-addr> canonical



###########################
#  Name Canonicalization  #
###########################
S3

# handle "from:<>" special case
R<>			$@@				turn into magic token

# basic textual canonicalization -- note RFC733 heuristic here
R$*<$*<$*<$+>$*>$*>$*	$4				3-level <> nesting
R$*<$*<$+>$*>$*		$3				2-level <> nesting
R$*<$+>$*		$2				basic RFC821/822 parsing
#R$+ at $+		$1@$2				"at"->"@" for RFC 822

R$j!$+			$1				strip our host name
R$=w!$+			$2				even if it has dots
R$=w.$D!$+		$2

# make sure <@a,@b,@c:user@d> syntax is easy to parse -- undone later
R@$+,@$+		@$1:@$2				change all "," to ":"

# localize and dispose of route-based addresses
R@$+:$+			$@<@$1>:$2			handle <route-addr>

# more miscellaneous cleanup
R$+:$*;@$+		$@$1:$2;@$3			list syntax
R$+:$*;			$@$1:$2;			list syntax
R$*@$+			$:$1<@$2>			focus on domain
R$*<$+@$+>		$1$2<@$3>			move gaze right
R$*<@$+>		$@$1<@$2>			already canonical

# convert old-style addresses to a domain-based address
R$-:$+			$@$2<@$1>			host:user
R$+^$+			$1!$2				convert ^ to !
R$-.$+!$+		$@$3<@$1.$2>			host.domain!user
R$-!$+			$@$2<@$1>			host!user (uucp)
R$+%$+			$:$1<@$2>			user%host
R$+<@$+%$+>		$1%$2<@$3>			move gaze right
R$*<@$+>		$@$1<@$2>			now % is canonical
R$-=$+			$@$2<@$1.BITNET>		host=user (bitnet)
#R$-.$+			$@$2<@$1>			host.user (? XXX ?)



#########################
# retry rule for rule 0 #
#########################
# This is used to avoid having to always invoke rule 3 at the start of
#	rule 0, as standard configurations do.  It is invoked to re-start
#	parsing.
S29
R:$*			$:$1				remove routing debris
R<@$*>:@$*		<@$1>,@$2

R$+!$+			$:$>5$1!$2			make UUCP style pure

R$*<$*>$*		$1$2$3				defocus
R$+			$:$>3$1				make canonical
R$+			$@$>0$1				try rule 0 again



###########################
# general address parsing #
###########################
S0

# Handle some special cases.....
R@			$#local$:$n			handle <> form
R$*<@[$+]>$*		$:$1<@$[[$2]$]>$3		numeric internet spec

# Canonicalize the host name.  Detect any failures.
R$*<@$+>$*		$:$1<@$[$2$:$2.$Y$]>$3

# If we successfully canonicalize a hostname, but the result contains
# no dots, make it relative to our domain.
R$*<@$->$*		$1<@$2.$D>$3
R$*<$*.>$*		$1<$2>$3			drop any trailing dot
R$*<$*..$Y>$*		$1<$2.$Y>$3			tidy any double dot

# (Support for Gauntlet email configuration.)
R$*<@$D>$*		$:$?G$#local$:$1$|$1<@$D>$2$.

# now delete the local info
R$+			$:$>30$1			detect local info
R$*<@@$*>$*		$@$>29$1$3			remove local and retry

R<@>$*			$@$>29$1			route strip & retry
R$*<@>			$@$>29$1			strip null & retry

# Hostname is now non-local and as canonical as we are going to get.
# If the initial canonicalization step failed, ".FAIL" ($Y) has been
# appended to the hostname.

# If the mail is destined for a known dead host, strip that host and
# retry.
R$*<@$=K>$*		$@$>29$1$3
R$*<@$=K.$Y>$*		$@$>29$1$3			even on failures

# If the mail is destined for a host in a domain for which we are
# responsible, send it directly.
R$*<@$-.$=D>$*		$#ether$@$2.$3$:$1<@$2.$3>$4

# The above is true even if we failed to canonicalize.  We are supposed
# to know how to deal with the message.  If we can't, who can?
R$*<@$-.$=D.$Y>$*	$#ether$@$2.$3$:$1<@$2.$3>$4

# If the mail is destined for the forwarder, send it directly.
R$*<@$=F>$*		$#ether$@$2$:$1<@$2>$3

# See if an MX record is available for the resulting hostname.
ifelse(eval(SENDMAIL_VERSION >= 80700),1,
`R$*<@$+$~Y>$*		$(bestmx $2$3$:$Y$)^$1^$2$3^$4
R$*.^$*^$*^$*		$1^$2^$3^$4		Need to Remove trailing dot',
`R$*<@$+$~Y>$*		${@$2$3$:$Y$}^$1^$2$3^$4')

# Do the MX lookup even on failures since some host may choose to
# advertise itself as an exchanger for some otherwise uncanonicalizable
# hostname.
ifelse(eval(SENDMAIL_VERSION >= 80700),1,
`R$*<@$+.$Y>$*		$(bestmx $2$:$Y$)^$1^$2.$Y^$3
R$*.^$*^$*^$*		$1^$2^$3^$4		Need to Remove trailing dot',
`R$*<@$+.$Y>$*		${@$2$:$Y$}^$1^$2.$Y^$3')

# If we fail to find any MX record and the address is of the form:
# user@domain.for.which.we.are.responsible, strip and retry.
R$Y$*^$*^$=D^$*		$@$>29$2$4
R$Y$*^$*^$=D.$Y^$*	$@$>29$2$4

# See if we have to use a forwarder to mail to the exchanger.
R$~Y$*^$*		$:$>31@@<@$1$2>^$3		found one!
R@@<@@@$+>$*		$:@@@@$2			use the forwarder
R@@<@@$+>$*		$:@@@$2				we are forwarder

# Use the appropriate mailer.
R@@@@$*^$*^$*.$Y^$*	$@$>29@$F:$2@$3$4		use forwarder
R@@@@$*^$*^$*^$*	$@$>29@$F:$2@$3$4

# (Support for Gauntlet email configuration.)
R@@@$*^$*^$*.$Y^$*	$#forgnout$@$3$:$2<@$3>$4	exchanger is external
R@@@$*^$*^$*^$*		$#forgnout$@$3$:$2<@$3>$4

R@@$*^$*^$*.$Y^$*	$#ether$@$3$:$2<@$3>$4		exchanger is internal
R@@$*^$*^$*^$*		$#ether$@$3$:$2<@$3>$4

R$Y^$*^$*^$*		$1<@$2>$3			no (or bad) MX, tidy

# If we successfully canonicalized the hostname, but couldn't find any good
# MX record to tell us what to do with the message, see if we should send the
# message on to a forwarder.
R$*<@$*$~Y>$*		$:$>31^$1<@$2$3>$4		mark if canonical
R^$*<@@@$*>$*		$@$>29@$F:$1@$2$3		send to forwarder
# (Support for Gauntlet email configuration.)
R^$*<@@$*>$*		$#forgnout$@$2$:$1<@$2>$3	we are forwarder
R^$*			$:$1				unmark

# ??? unusual rule
# Notice news stuffing, and stuff mail for newsgroups into a suitable script
#R$*<@news-stuff.$D>	$#mailnews$:$1
#R$*<@news-stuff.$D.$Y>	$#mailnews$:$1

# The message is now either to an uncanonicalizable host, or an internal
# host for which no MX record exists.

# For internal hosts, see if there's an appropriate relay
# machine (poor man's MX).

# This code has been known to cause problems with systems that are not
# using DNS, and may need to be commented out

# If within our domain space, try "relay".addressee.domain
# (even if we failed to canonicalize)

R$*<@$*$=T>$*		$:^<@$[$R.$2$T$:$Y.$2$T$]>$1^$2$T^$4
R$*<@$*$=T.$Y>$*	$:^<@$[$R.$2$T$:$Y.$2$T.$Y$]>$1^$2$T.$Y^$4
R^<@$Y.$-.$*$=T>$*	$:^<@$[$R.$2$T$:$Y.$1.$2$T$]>$4
R^<@$Y.$-.$*$=T.$Y>$*	$:^<@$[$R.$2$T$:$Y.$1.$2$T.$Y$]>$4

R^<@$~Y$*>$*^$*^$*	$:$>30<@$1$2>$3^$4^$5
R^<@$Y.$*>$*^$*^$*	$:$2<@$3>$4

R<@@$*>$*		$:<@$Y@>$2			check for self

R<@$~Y$*>$*^$*.$Y^$*	$#ether$@$1$2$:$3<@$4>$5	strip any .FAIL & bye!
R<@$~Y$*>$*^$*^$*	$#ether$@$1$2$:$3<@$4>$5	bye bye!

R<$*>$*^$*^$*		$:$2<@$3>$4			tidy

# No appropriate relay (or relay is us). If we successfully
# canonicalized and the resulting hostname is in or under our
# domain, or contains no dots, send it directly.

R$*<@$*.$Y>$*		$:$Y^$1<@$2>$3
R$~Y$*<@$*.$D>$*	$#ether$@$3.$D$:$1$2<@$3.$D>$4	bye bye
R$~Y$*<@$->$*		$#ether$@$3$:$1$2<@$3>$4	bye bye

# Since we have determined that we may be the relay for the addressee domain,
# see if the addressee host is actually some uncanonicalizable nickname
# for ourselves (canonicalizable nicknames would have been stripped as local
# info at the top of this ruleset).  If so, strip and retry.

R$Y^$*			$:$>30$1
R$*<@@$*>$*		$@$>29$1$3

# All notion of "uncanonicalized" is now stripped (.FAIL removed from
# addressee hostname).  See if our "uncanonicalizable" host can be found
# in the UUCP maps or if a hard-coded exchanger exists.

# From UUCP host to UUCP host
#	"goo.UUCP" is the "goo" in the UUCP maps, which may not be the same
#	as "goo.UUX" which is the goo to which we talk directly.
R$*<@$=V.$D>$*		$:@@$1<@$2.$D>$3
R$*<@$=V.UUX>$*		$:@@$1<@$2.UUX>$3
R$*<@$=V>$*		$:@@$1<@$2.UUX>$3

R@@$*<@$*$=N.UUX>$*	$#dom$@$3$:$1<@$2$3>$4
R@@$*<@$*$=N.$+>$*	$#dom$@$3$:$1<@$2$3.$4>$5
R@@$*<@$-.$+>$*		$#uucp$@$2$:$1<@$2>$4

#re-alias for dead hosts
R$*<@$=K>$*		$@$>29$1$3

# ??? unusual rule
# Talk to Internet forwarders.  In the absence of MX record handling
#	this set of rules implement a useful kludge.  Of course, ...
#R$*<@$*fozul.com>$*	$#forgn$@fozul.com$:$1<@$2fozul.com>$3


# look for a UUCP/pathalias path to a host
#	"goo.UUCP" is the "goo" in the UUCP maps, which may not be the same
#	as "goo.UUX" which is the goo to which we talk directly.
# ??? unusual rule
#	It is not usually a good idea to send all host names through the maps.
#	IMPORTANT, un-comment at most 1 of the following lines.
R$+<@$->			$:^$1<@$[!$P!$2$]><@$2>
#R$+!$+<@$->			$:^$1!$2<@$[!$P!$3$]><@$3>
#
R$+<@$-.UUCP>			$:^$1<@$[!$P!$2$]><@$2.UUCP>

R^$+<@!$P!$=V><@$+>		$@$>29$1@$2
R^$+<@!$P!$-><@$+>		$:$1<@$3>

# if UUCP resolution succeeds, start over
R^$+<@$+><@$+>			$@$>29$2!$1

# resolve unknown UUCP domains with pathalias
R$+<@$+.UUCP>			$:^$1<@$2><$[!$P!$2$]>
R^$+<@$+><!$P!$*.$D>		$@$>29$1@$2		recognize LOCAL.uucp
R^$+<@$+><!$P!$*>		$:$1<@$2.UUCP>
R^$+<@$+><$*>			$@$>29$3!$2!$1		use generated path

# resolve unknown domains with pathalias
R$*<@$+.$+>$*			$:^$1<@$2.$3><$[!$P!$2.$3$]>$4
R^$*<@$+><!$P!$*>$*		$:$1<@$2>$4
R^$*!$*<@$+><$=N>		$@$>29$4!$3!$1!$2	domain to good neighbor
R^$*!$*<@$+><$=N!$+>		$@$>29$4!$3!$1!$2
R^$+<@$+><$=N>			$@$>29@$3:$1@$2
R^$+<@$+><$=N!$+>		$@$>29@$3:$1@$2
R^<@$+><$+!$+>$+		$@$>29@$2:@$1$4		use generated path
R^<@$+><$+>$+			$@$>29@$2:@$1$3
R^$+<@$+><$+>			$@$>29$3!$2!$1
# we cannot convert nasty stuff--where do the !'s go?
R^$*<@$+><$*>$*			$:$1<@$2>$4

# O.K.  I give up!  Send this message to someone wiser.

# See if we need to use a forwarder.
R$*<@$*>$*		$:$>31$1<@$2>$3
R$*<@@@$*>$*		$@$>29@$F:$1@$2$3		send to forwarder
# (Support for Gauntlet email configuration.)
R$*<@@$*>$*		$#forgnout$@$2$:$1<@$2>$3	we are forwarder

# Destination is internal.  Try "relay".our.domain
R$*<@$*>$*		$:$>30<@$[$R.$D$:$Y$]>$1^$2^$3
R<@@$*>$*		$:<@$Y@>$2			check for self
R<@$~Y$*>$*^$*^$*	$#ether$@$1$2$:$3<@$4>$5	bye bye!
R<$*>$*^$*^$*		$:$2<@$3>$4			tidy

# Message is rated PG (Parental Guidance suggested)...
# First, determine our domain's location relative to the top level.
R$*<@$*>$*		$:$D^$1^$2^$3
R$*$=T^$*		$:$1^$3
R$*.^$*			$:$1^$2

# If our domain isn't the top domain, try and send this message
# to "relay".our.parent.domain
R$+.$+^$*		$:$>30<@$[$R.$2$T$:$Y$]>$1.$2^$3
R<@@$*>$*		$:<@$Y@>$2			check for self
R<@$~Y$*>$*^$*^$*^$*	$#ether$@$1$2$:$4<@$5>$6	bye bye!
R<$*>$*^$*		$:$2^$3
R$+^$*			$:$>30<@$[$R.$T$:$Y$]>$1^$2
R<@@$*>$*		$:<@$Y@>$2			check for self
R<@$~Y$*>$*^$*^$*^$*	$#ether$@$1$2$:$4<@$5>$6	bye bye!
R<$*>$*^$*		$:$2^$3

# If the top domain isn't our domain, or our immediate parent domain,
# try and send this message to "relay".top.domain
R$+.$+^$*		$:$>30<@$[$R.$T$:$Y$]>$1.$2^$3
R<@@$*>$*		$:<@$Y@>$2			check for self
R<@$~Y$*>$*^$*^$*^$*	$#ether$@$1$2$:$4<@$5>$6	bye bye!

# Try just "relay"
R<$*>$*			$:$2
R$*^$*^$*^$*		$:$2^$3^$4
R$*^$*^$*		$:$>30<@$[$R$:$Y$]>$1^$2^$3
R<@@$*>$*		$:<@$Y@>$2			check for self
R<@$~Y$*>$*^$*^$*	$#ether$@$1$2$:$3<@$4>$5	bye bye!
R<$*>$*^$*^$*		$:$2<@$3>$4
R^$*^$*^$*		$:$1<@$2>$3
R$*<@$*$=T>$*		$#ether$@$2$3$:$1<@$2$3>$4	BOING!?!?

# Send completely unknown, external-looking stuff to the forwarder.
R$*<@$*>$*		$:$>30<@$F>$1^$2^$3
R<@@$*>$*		$:<@$Y@>$2			check for self
R<@$~Y$*>$*^$*^$*	$@$>29@$F:$3@$4$5		bye bye!
R<$*>$*^$*^$*		$:$1<@$2>$3

# Drat!  Not much else we can do! 
# Complain about unknown machines and domains
R$*<@$*>$*		$#ether$@$2$:$1<@$2>$3		BOING!?!?

ifdef(`SGI_INTERNAL',
`# ??? unusual rule (SGI internal specific)
# Stuff mail for newsgroups into a suitable script
Rmail.$*		$@$>29<@mailnews.engr.sgi.com:mail.$1@news-stuff.sgi.com>
Rsgi.$*			$@$>29<@mailnews.engr.sgi.com:sgi.$1@news-stuff.sgi.com>
Rnews.$*		$@$>29<@mailnews.engr.sgi.com:$1@news-stuff.sgi.com>

')dnl
# everything else must be a local name or alias
R$*			$#local$:$1

################################
# convert to UUCP style routes #
################################
S5
R$*<$+>$*		$1$2$3			defocus
R@$+:@$+:$+		@$1,@$2:$3		<route-addr> canonical
R@$+,@$+		@$1!$2			@a,@b:user@c to @a!b:user@c
R@$+:$+@$+		$:$1!$3!$2		@a!b:user@c to a!b!c!user
R$+@$+			$2!$1			simple domain address

R$-.UUCP!$+		$1!$2
R$-.UUX!$+		$1!$2


########################
# strip internal paths #
########################
S7
R$*			$:$>5$1				convert to UUCP style

# remove our name for mailers which will add our hostname in any case
R$=w!$+			$@$>7$2
R$j!$+			$@$>7$1


##################################
# resolve unknown UUCP addresses #
##################################
S8
R$*<@$*$D>$*			$@$1<@$2$D>$3		recognize our domain
R$*<@$=w.UUCP>$*		$@$1<@$2.UUCP>$3

R$*<@$=V.UUCP>$*		$@$1<@$2.UUCP>$3	notice our modems

R$*				$:$w!$1			prepend "ourself!"
R$~F!$*				$@$2			quit if not a forwarder
R$~F.$+!$*			$@$3
R$+!$*				$:$2

# look for a path to a host
R$*<@$-.UUCP>			$:!!$1<@$[!$P!$2$]>
R!!$*<@!$P!$->			$@$1<@$2.UUCP>
R!!$*<@$*>			$@$2!$1			use generated path

# resolve unknown UUCP domains
R$*<@$+.UUCP>			$:!!$1<@$2><$[!$P!$2$]>
R!!$*<@$+><!$P!$+>		$@$1<@$2.UUCP>
R!!$*<@$+><$+>			$@$3!$2!$1		use generated path

# resolve unknown domains
R$*<@$+.$+>			$:!!$1<@$2.$3><$[!$P!$2.$3$]>
R!!$*<@$+><!$P!$*>		$@$1<@$2>
R!!<@$+><$+>			$@$2!$1
R!!$+<@$+><$+>			$@$3!$2!$1


############################################################
############################################################
#	Useful utilities
############################################################
############################################################

# S30 - Detect our hostname
# If the hostname in focus is us, mark it by doubling the "@"
# if so.  Canonicalization falures are of no consequence here.
# Ignore appended $Y's.
S30
# ??? - unusual rule
# Find other names for ourself.
#	XXX this should be handled in the $=w class.
#R$*<@alternatename.foo.com>$*		$@$1<@@$j>$2
#R$*<@alternatename.foo.com.$Y>$*	$@$1<@@$j>$2

R$*<@>$*		$@$1<@@>$2			null host is thishost
R$*<@.$Y>$*		$@$1<@@.$Y>$2			 ignore failures
R$*<@$=w>$*		$@$1<@@$2>$3			alternate names
R$*<@$=w.$Y>$*		$@$1<@@$2.$Y>$3			 ignore failures
R$*<@$j>$*		$@$1<@@$j>$2			this host
R$*<@$j.$Y>$*		$@$1<@@$j.$Y>$2			 ignore failures

# ??? - unusual rule
# Uncomment the following lines if you don't have a wildcard MX record for
# the LOCAL domain and if you want to treat foo@LOCAL as foo@THISHOST.
#R$*<@$D>$*		$@$1<@@$D>$2			LOCAL
#R$*<@$D.$Y>$*		$@$1<@@$D.$Y>$2			 ignore failures
#R$*<@.$D>$*		$@$1<@@.$D>$2			.LOCAL
#R$*<@.$D.$Y>$*		$@$1<@@.$D.$Y>$2		 ignore failures

R$*<@$=w.UUCP>$*	$@$1<@@$2.UUCP>$3		thishost.UUCP
R$*<@$=w.UUCP.$Y>$*	$@$1<@@$2.UUCP.$Y>$3		 ignore failures
R$*<@$j.UUCP>$*		$@$1<@@$j.UUCP>$2
R$*<@$j.UUCP.$Y>$*	$@$1<@@$j.UUCP.$Y>$2

# S31 - See if we should use the forwarder
# Return with the "@" in front of the focused host modified as follows:
#	unchanged - host is internal, use of forwarder should be unnecessary.
#	@ -> @@   - host is external and we are the forwarder or forwarder
#		    loop detected.
#	@ -> @@@  - host is external and we are not the forwarder.
S31
# If no top level domain, treat as internal...
R$*			$:<@$T@>$1
R<@$=T@>$*		$:$2
R<@$*@>$*		$@$2

R$*<@$*$=T>$*		$@$1<@$2$T>$4			not external
R$*			$:$>30<@$F>$1
R<@@$*>$*<@$*>$*	$@$2<@@$3>$4			we are the forwarder
R<@$*>$*<@$=F>$*	$@$2<@@$3>$4			forwarder loop, send!
R<@$*>$*<@$*>$*		$@$2<@@@$3>$4			send to forwarder!

############################################################
############################################################
#####
#####		Local and Program Mailer specification
#####
############################################################
############################################################

# ??? unusual rule
# If you don't want /bin/mail to try and validate a local user by
# searching for that user's home directory on the local host, add the
# -P command line flag after the -d, and before the $u in the A= field
# of the local mailer below.  This means that /bin/mail will consider
# a user to be a valid local user if an entry for that user can be found
# using getpwent(3C).  This may be useful in networks where user's home
# directories are remote mounted to the local host.
#
# WARNING: If you use the -P flag and a remote user's passwd entry is
# visible to the local host (via NIS) and for any reason the remote
# user's alias is not visible to the local host, mail to that user may
# be erroneously delivered on the local host if addressed to just "user".

# To maintain compatability with previous versions of irix sendmail,
# the "u" flag (allow Uppercase user names) has been removed from Mlocal.

Mlocal, P=/bin/mail, F=EDFMlsh, S=10, R=20, A=mail -s -d $u
Mprog,	P=/bin/sh,   F=lsDFMe, S=10, R=20, A=sh -c $u

S10
R$+!$+			$:$>7$1!$2			clean UUCP style
# ??? unusual rule
# In the interest of brevity and aesthetics, the following four rewrite
# rules attempt to strip any local host and/or domain name from all sender
# addresses prior to final delivery.  If this behaviour is not desired,
# comment out the following four rewrite rules, and all sender addresses
# will appear in their fully canonical form.
R$*<@$j>		$@$1
R$*<@$=w>		$@$1
R$*<@$=w.$D>		$@$1
R$*<@$-.$D>		$1<@$2>

S20
R$+!$+			$:$>7$1!$2			clean UUCP style
R$*<@>			$@$1
# ??? unusual rule
# In the interest of brevity and aesthetics, the following four rewrite
# rules attempt to strip any local host and/or domain name from all recipient
# addresses prior to final delivery.  If this behaviour is not desired,
# comment out the following four rewrite rules, and all recipient addresses
# will appear in their fully canonical form.
R$*<@$j>		$@$1
R$*<@$=w>		$@$1
R$*<@$=w.$D>		$@$1
R$*<@$-.$D>		$1<@$2>


############################################################
############################################################
#####
#####		Ethernet Mailer specification
#####
############################################################
############################################################

Mether,	P=[IPC], F=mDFMhuXC, S=11, R=21, E=\r\n, A=IPC $h

S11
# Canonicalize any hostname.  Mark failures.
R$*<@$+>$*		$:$1<@$[$2$:$2.$Y$]>$3

# If successfully canonicalize to single-token hostname, make relative
# to our local domain.
R$*<@$->$*		$:$1<@$2.$D>$3

# Tidy up a bit.
R$*<@$+.$Y>$*		$:$1<@$2>$3		strip any .FAIL
R$*<@$+.>$*		$1<@$2>$3		strip any trailing dot(s)

# ??? unusual rule
# Try and match the whole gory thing in the reverse aliases map.
# Also see KA database definition above.
#R$*			$:^$1
#R^$*<@$*>$*		$:$(A$1@$2$3$:$1<@$2>$3$)
#R^$+			$:$(A$1$:$1$)

R$+!$+			$:$>7$1!$2		clean UUCP style
R$-<@$=V>		$2!$1			convert UUCP neighbors
R$+!$+			$:$w!$1!$2		tack on our hostname
R$+!$+			$@$>5$1!$2
R$*<@$+>$*		$@$1<@$2>$3
ifdef(`SGI_INTERNAL',`
# ??? unusual rule  (SGI internal specific)
# dont qualify newsgroup names, just leave them alone...
Rmail.$*		$@mail.$1
Rsgi.$*			$@sgi.$1
Rnews.$*		$@news.$1

')dnl
ifdef(`SGI_INTERNAL',`# ??? unusual rule (SGI internal specific) 
# gross hack; make all mail on this machine look like it came from
# "your_machine"; this is used when you want to hide the fact that
# this machine exists, for example if it is a home machine, and you want
# to send mail, but have replies come to your normal machine, not the
# home machine.  If used, edit and uncomment the next line, and comment
# the following line instead.
#R$+			$@$1<@your_machine.engr.sgi.com>

')dnl
R$+			$@$1<@$j>

S21
R$+!$+			$@$>5$1!$2		make UUCP style pure
R$+<@[$+]>		$@$1<@[$2]>		pass IP numbers

# Canonicalize any hostname.  Mark failures.
R$*<@$+>$*		$:$1<@$[$2$:$2.$Y$]>$3

# If successfully canonicalize to single-token hostname, make relative
# to our local domain.
R$*<@$->$*		$:$1<@$2.$D>$3

# Tidy up a bit.
R$*<@$+.$Y>$*		$:$1<@$2>$3		strip any .FAIL
R$*<@$+.>$*		$1<@$2>$3		strip any trailing dot(s)

R$-<@$=V>		$@$>5$1<@$2>		convert UUCP short hand
ifdef(`SGI_INTERNAL',
`
# ??? unusual rule  (SGI internal specific)
# dont qualify newsgroup names, just leave them alone...
Rmail.$*		$@mail.$1
Rsgi.$*			$@sgi.$1
Rnews.$*		$@news.$1

')dnl
R$-			$:$1<@$j>

############################################################
############################################################
#####
#####		UUCP Mailer specifications
#####
############################################################
############################################################


# Domain UUCP, which can take >1 destination per transaction, and understands
#	domains.  These guys like 'remote from' lines, unlike ethernet users.
Mdom, P=/usr/bin/uux, F=sDFMhumUC, S=13, R=23, A=uux - $h!rmail ($u)

S13
R$*<@$-.UUCP>		$:$>7$1<@$2.UUCP>
R$*			$:$>11$1
R$-.$D!$+		$1!$2			remove our domain if UUCP

S23
R<@$+>$+		$:$>5<@$1>$2		822 route to UUCP route
R$*<@$-.UUCP>		$:$>5$1<@$2.UUCP>
R$*<@$-.UUX>		$:$>5$1<@$2.UUCP>
R$*			$@$>21$1


# dumb UUCP to the great, outside world
#	Notice we assume they can handle >1 addressee / msg
Muucp, P=/usr/bin/uux, F=sDFMhumUC, S=14, R=24, A=uux - $h!rmail ($u)

S14
R$*			$:$>7$1			convert to UUCP style
R$*			$:$w!$1			add gateway name
R$-.$D!$+		$1!$2			remove our domain

S24
R$+			$:$>8$1			resolve uucp hosts
R$*			$:$>5$1			convert to UUCP style



############################################################
############################################################
#####
#####		Usenet 'Mailer' specification
#####
############################################################
############################################################

# ??? unusual rule
# stuff news articles into local news groups
Mmailnews, P=/usr/lib/news/mailnews, F=lsDFemC, S=15, R=25, A=mailnews $u

S15
R$+!$+			$:$>7$1!$2			clean UUCP style

S25
R$+!$+			$:$>7$1!$2			clean UUCP style



############################################################
############################################################
#####
#####		Foreign TCP/SMTP Mailer specification
#####
############################################################
############################################################

# This is the same as the local ethernet mailer.
Mforgn,	P=[IPC], F=mDFMhuXC, S=16, R=21, E=\r\n, A=IPC $h

S16
R$*			$:$>11$1	same as normal ethernet

# ??? unusual rule
# Rewrite From: lines to hide internal domains (the infamous "%-hack".)
#R$-<@$j>		$@$1<@$j>
#R$-<@$+.$D>		$@$1%$2.$D<@$j>
#R$-<@$+.$D>		$@$1%$2<@$D>


#S26
#R$*			$@$>21$1	same as normal ethernet


############################################################
############################################################
#####
#####		OUTBOUND Foreign TCP/SMTP Mailer specification
#####		(With support for Gauntlet email configuration.)
#####
############################################################
############################################################

# This is the mailer for outbound traffic
Mforgnout,	P=[IPC], F=mDFMhuXC, S=37, R=21, E=\r\n, A=IPC $h

S37
# Canonicalize any hostname.  Mark failures.
R$*<@$+>$*		$:$1<@$[$2$:$2.$Y$]>$3

# If successfully canonicalize to single-token hostname, make relative
# to our local domain.
R$*<@$->$*		$:$1<@$2.$D>$3

# Tidy up a bit.
R$*<@$+.$Y>$*		$:$1<@$2>$3		strip any .FAIL
R$*<@$+.>$*		$1<@$2>$3		strip any trailing dot(s)

R$+!$+			$:$>7$1!$2		clean UUCP style
R$-<@$=V>		$2!$1			convert UUCP neighbors
R$+!$+			$:$w!$1!$2		tack on our hostname
R$+!$+			$@$>5$1!$2

#R$*<@$+>$*		$@$1<@$2>$3
#R$+			$@$1<@$j>
R$*<@$+.$=Q.$T>		$@$?G$1<@$3.$T>$|$1<@$2.$3.$T>$.
R$*<@$+>$*		$@$?G$1<@$D>$|$1<@$2>$3$.
R$+			$@$?G$1<@$D>$|$1<@$j>$.
