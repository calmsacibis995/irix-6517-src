divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

#
#  This is a generic configuration file for IRIX systems
#  It has support for local and SMTP mail only.  
#  IRIX will NOT overwrite this file.
#
#  To uncomment lines from this file you will need to remove `quotes' from
#  m4 directives such as `FEATURE' and the leading `#'
#
#  Regenerate this file with the command
#  configmail mc2cf
# 
divert(0)dnl
#
#
VERSIONID(`@(#)sgi-irix6.mc	8.7 (Berkeley) 2/3/1999')
OSTYPE(irix6)dnl
define(`ALIAS_FILE',`/etc/aliases,nsd:mail.aliases')dnl
FEATURE(`local_lmtp',`/usr/bin/mail.local')dnl
MAILER(local)dnl
MAILER(smtp)dnl
#
# Compatability with previous IRIX versionss
define(`DATABASE_MAP_TYPE', `dbm')dnl
#
# SMART_HOST
#
#`define(`SMART_HOST',`relay')dnl'
#`FEATURE(`smart_host_domain',`your.domain')dnl'
#
# Scale queueLA, and refuseLA based upon number of CPUs in the system
#
FEATURE(`percpu_QueueLA',10)dnl
FEATURE(`percpu_RefuseLA',12)dnl
#
# The following two FEATURES tell sendmail to accept mail from hosts and
# users that may not exist.   This is needed inside of many filewalls.
# You might want to comment out these lines.
#
FEATURE(`accept_unqualified_senders')dnl
FEATURE(`accept_unresolvable_domains')dnl
#
# The following `FEATURE' tells sendmail to relay mail to/from hosts in
# the current domain, as defined by $=m.  If you want to relay more than
# the current domain, use one of the other relay_ features, or add to the
# $=m class by uncommenting and modifying the following line
#`Cm toplevel.domain'
# 
FEATURE(`relay_entire_domain')dnl
#
# You can control the loglevel of sendmail
#
define(`confLOG_LEVEL',1)dnl
#
# The Following allows us to have "alternate" names that we will process
# for local delivery
#
define(`confCW_FILE',`-o /etc/sendmail.cw')dnl
FEATURE(`use_cw_file')dnl
