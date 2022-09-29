#!/bin/sh

# btool_init [libdir]

# WARNING:  This file is generated from btool_init.sh.

if [ "x$1" = "x" ]
then
	# LIBDIR is replaced with the true library directory by the makefile.
	true_libdir=${TOOLROOT}/LIBDIR
else
	true_libdir=$1
fi

if [ ! -f btool_lib.c ]
then
	/bin/cp $true_libdir/btool_lib.c btool_lib.c
fi

if [ ! -f btool_defs.h ]
then
	/bin/cp $true_libdir/btool_defs.h btool_defs.h 
fi

if [ -f btool_map ] ; then /bin/rm btool_map ; fi
