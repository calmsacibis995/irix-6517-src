#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983, 1995 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

################################################################
## ` -=+#+=- -=+#+=- -=+#+=- /\oo/\ -=+#+=- -=+#+=- -=+#+=- ' ##
################################################################
##                                                            ##
##     WARNING: This file was automatically generated         ##
##              from the sendmail.mc file.  If you edit       ##
##              this file by hand, your changes will go       ##
##	        away at the next reboot if sendmail_cf is     ##
##              chkconfig-ed on.                              ##
##                                                            ##
##              Hacking/editing on sendmail.cf directly       ##
##              is not supported.  You should change the      ##
##              sendmail.mc file instead.                     ##
##                                                            ##
################################################################
## ` -=+#+=- -=+#+=- -=+#+=- /\../\ -=+#+=- -=+#+=- -=+#+=- ' ##
################################################################

######################################################################
######################################################################
#####
#####		SENDMAIL CONFIGURATION FILE
#####
define(`TEMPFILE', maketemp(/tmp/cfXXXXXX))dnl
syscmd(sh _CF_DIR_`'sh/makeinfo.sh _CF_DIR_ > TEMPFILE)dnl
include(TEMPFILE)dnl
syscmd(rm -f TEMPFILE)dnl
#####
######################################################################
######################################################################

divert(-1)

changecom()
undefine(`format')
undefine(`hpux')
ifdef(`pushdef', `',
	`errprint(`You need a newer version of M4, at least as new as
System V or GNU')
	include(NoSuchFile)')
define(`PUSHDIVERT', `pushdef(`__D__', divnum)divert($1)')
define(`POPDIVERT', `divert(__D__)popdef(`__D__')')
define(`OSTYPE',
	`PUSHDIVERT(-1)
	ifdef(`__OSTYPE__', `errprint(`duplicate OSTYPE'($1))')
	define(`__OSTYPE__', $1)
	define(`_ARG_', $2)
	include(_CF_DIR_`'ostype/$1.m4)POPDIVERT`'')
define(`MAILER',
`ifdef(`_MAILER_$1_', `dnl`'',
`define(`_MAILER_$1_', `')PUSHDIVERT(7)include(_CF_DIR_`'mailer/$1.m4)POPDIVERT`'')')
define(`DOMAIN', `PUSHDIVERT(-1)define(`_ARG_', $2)include(_CF_DIR_`'domain/$1.m4)POPDIVERT`'')
define(`FEATURE', `PUSHDIVERT(-1)define(`_ARG_', $2)include(_CF_DIR_`'feature/$1.m4)POPDIVERT`'')
define(`HACK', `PUSHDIVERT(-1)define(`_ARG_', $2)include(_CF_DIR_`'hack/$1.m4)POPDIVERT`'')
define(`VERSIONID', ``#####  $1  #####'')
define(`LOCAL_RULE_0', `divert(3)')
define(`LOCAL_RULE_1',
`divert(9)dnl
#######################################
###  Ruleset 1 -- Sender Rewriting  ###
#######################################

S1
')
define(`LOCAL_RULE_2',
`divert(9)dnl
##########################################
###  Ruleset 2 -- Recipient Rewriting  ###
##########################################

S2
')
define(`LOCAL_RULESETS',
`divert(9)

')
define(`LOCAL_RULE_3', `divert(2)')
define(`LOCAL_CONFIG', `divert(6)')
define(`MAILER_DEFINITIONS', `divert(7)')
define(`LOCAL_NET_CONFIG', `define(`_LOCAL_RULES_', 1)divert(1)')
define(`UUCPSMTP', `R DOL(*) < @ $1 .UUCP > DOL(*)	DOL(1) < @ $2 > DOL(2)')
define(`CONCAT', `$1$2$3$4$5$6$7')
define(`DOL', ``$'$1')
define(`SITECONFIG',
`CONCAT(D, $3, $2)
define(`_CLASS_$3_', `')dnl
ifelse($3, U, Cw$2 $2.UUCP, `dnl')
define(`SITE', `ifelse(CONCAT($'2`, $3), SU,
		CONCAT(CY, $'1`),
		CONCAT(C, $3, $'1`))')
sinclude(_CF_DIR_`'siteconfig/$1.m4)')
define(`EXPOSED_USER', `PUSHDIVERT(5)CE$1
POPDIVERT`'dnl`'')
define(`LOCAL_USER', `PUSHDIVERT(5)CL$1
POPDIVERT`'dnl`'')
define(`MASQUERADE_AS', `define(`MASQUERADE_NAME', $1)')
define(`MASQUERADE_DOMAIN', `PUSHDIVERT(5)CM$1
POPDIVERT`'dnl`'')
define(`MASQUERADE_DOMAIN_FILE', `PUSHDIVERT(5)FM$1
POPDIVERT`'dnl`'')
define(`GENERICS_DOMAIN', `PUSHDIVERT(5)CG$1
POPDIVERT`'dnl`'')
define(`GENERICS_DOMAIN_FILE', `PUSHDIVERT(5)FG$1
POPDIVERT`'dnl`'')
define(`RELAY_DOMAIN', `PUSHDIVERT(5)CR$1
POPDIVERT`'dnl`'')
define(`RELAY_DOMAIN_FILE', `PUSHDIVERT(5)FR$1
POPDIVERT`'dnl`'')
define(`_OPTINS', `ifdef(`$1', `$2$1$3')')

m4wrap(`include(_CF_DIR_`m4/proto.m4')')

# set up default values for options
define(`ALIAS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/aliases', `/etc/aliases'))
define(`confMAILER_NAME', ``MAILER-DAEMON'')
define(`confFROM_LINE', `From $g  $d')
define(`confOPERATORS', `.:%@!^/[]+')
define(`confSMTP_LOGIN_MSG', `$j Sendmail $v/$Z; $b')
define(`confRECEIVED_HEADER', `$?sfrom $s $.$?_($?s$|from $.$_)
	$.by $j ($v/$Z)$?r with $r$. id $i$?u
	for $u; $|;
	$.$b')
define(`confSEVEN_BIT_INPUT', `False')
define(`confEIGHT_BIT_HANDLING', `pass8')
define(`confALIAS_WAIT', `10')
define(`confMIN_FREE_BLOCKS', `100')
define(`confBLANK_SUB', `.')
define(`confCON_EXPENSIVE', `False')
define(`confDELIVERY_MODE', `background')
define(`confTEMP_FILE_MODE', `0600')
define(`confMCI_CACHE_SIZE', `2')
define(`confMCI_CACHE_TIMEOUT', `5m')
define(`confUSE_ERRORS_TO', `False')
define(`confLOG_LEVEL', `9')
define(`confCHECK_ALIASES', `False')
define(`confOLD_STYLE_HEADERS', `True')
define(`confPRIVACY_FLAGS', `authwarnings')
define(`confSAFE_QUEUE', `True')
define(`confTO_QUEUERETURN', `5d')
define(`confTO_QUEUEWARN', `4h')
define(`confTIME_ZONE', `USE_SYSTEM')
define(`confCW_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/local-host-names', `/etc/sendmail.cw'))
define(`confMIME_FORMAT_ERRORS', `True')
define(`confFORWARD_PATH', `$z/.forward.$w:$z/.forward')
define(`confCR_FILE', `-o /etc/mail/relay-domains')

divert(0)dnl
VERSIONID(`@(#)cfhead.m4	8.23 (Berkeley) 10/6/1998')
