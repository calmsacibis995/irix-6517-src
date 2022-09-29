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

divert(0)
VERSIONID(`@(#)smart_host_domain.m4	8.11 (Berkeley) 2/10/1999')
divert(-1)

LOCAL_CONFIG
ifelse(_ARG_, `', `dnl',`
# The following domains are locally connected
# and should not be passed off to the SMART_HOST
CT`'_ARG_')

LOCAL_NET_CONFIG
ifelse(_ARG_,`',`dnl
# Hosts in the current domain $m are locally connected
R$* < @ $* .$m > $*	$#smtp $@ $2.$m $: $1 < @ $2.$m> $3
R$* < @ $* .$m. > $*	$#smtp $@ $2.$m. $: $1 < @ $2.$m.> $3',
`dnl
# Hosts listed as local in the $=T class are locally connected
R$* < @ $* .$=T > $*	$#smtp $@ $2.$3 $: $1 < @ $2.$3> $4
R$* < @ $* .$=T. > $*	$#smtp $@ $2.$3. $: $1 < @ $2.$3.> $4')
