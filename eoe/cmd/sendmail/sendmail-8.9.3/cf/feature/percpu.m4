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
VERSIONID(`@(#)percpu.m4	8.9 (Berkeley) 10/6/1998')
divert(-1)

undefine(`_num_cpu_')dnl

#
# Define `_num_cpu_' for ``OSTYPE'' irix6
#
ifelse(`__OSTYPE__', `irix6',dnl
define(`__cpu_tmp__',maketemp(/tmp/percpu.XXXXX))dnl
syscmd(`ksh -c "echo \"define(_num_cpu_,$(mpadmin -u | wc -l))\" > '__cpu_tmp__`"')dnl
include(__cpu_tmp__)dnl
syscmd(rm -f __cpu_tmp__)dnl
undefine(`__cpu_tmp__')dnl
)dnl
#
# Catchall if `_num_cpu_' is not yet set
#
ifdef(`_num_cpu_',`',`define(`_num_cpu_',1)')dnl

)
