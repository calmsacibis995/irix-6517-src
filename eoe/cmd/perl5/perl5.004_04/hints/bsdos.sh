# hints/bsdos.sh
#
# hints file for BSD/OS (adapted from bsd386.sh)
# Original by Neil Bowers <neilb@khoros.unm.edu>; Tue Oct  4 12:01:34 EDT 1994
# Updated by Tony Sanders <sanders@bsdi.com>; Sat Aug 23 12:47:45 MDT 1997
#     Added 3.1 with ELF dynamic libraries
#     SYSV IPC tested Ok so I re-enabled.
#
# To override the compiler on the command line:
#     ./Configure -Dcc=gcc2
#
# The BSD/OS distribution is built with:
#     ./Configure -des -Dbsdos_distribution=defined

signal_t='void'
d_voidsig='define'

usemymalloc='n'

# setre?[ug]id() have been replaced by the _POSIX_SAVED_IDS versions.
# See http://www.bsdi.com/bsdi-man?setuid(2)
d_setregid='undef'
d_setreuid='undef'
d_setrgid='undef'
d_setruid='undef'

# we don't want to use -lnm, since exp() is busted (in 1.1 anyway)
set `echo X "$libswanted "| sed -e 's/ nm / /'`
shift
libswanted="$*"

# X libraries are in their own tree
glibpth="$glibpth /usr/X11/lib"
ldflags="$ldflags -L/usr/X11/lib"

# Avoid telldir prototype conflict in pp_sys.c
pp_sys_cflags='ccflags="$ccflags -DHAS_TELLDIR_PROTOTYPE"'

case "$optimize" in
'')     optimize='-O2' ;;
esac

case "$bsdos_distribution" in
''|undef|false)	;;
*)
	d_dosuid='define'
	d_portable='undef'
	prefix='/usr/contrib'
	perlpath='/usr/bin/perl5'
	startperl='#!/usr/bin/perl5'
	scriptdir='/usr/contrib/bin'
	privlib='/usr/libdata/perl5'
	man1dir='/usr/contrib/man/man1'
	man3dir='/usr/contrib/man/man3'
	# phlib added by BSDI -- we share the *.ph include dir with perl4
	phlib="/usr/libdata/perl5/site_perl/$(arch)-$osname/include"
	phlibexp="/usr/libdata/perl5/site_perl/$(arch)-$osname/include"
	;;
esac

case "$osvers" in
1.0*)
	# Avoid problems with HUGE_VAL in POSIX in 1.0's cc.
	POSIX_cflags='ccflags="$ccflags -UHUGE_VAL"' 
	;;
1.1*)
	# Use gcc2
	case "$cc" in
	'')	cc='gcc2' ;;
	esac
	;;
2.0*|2.1*|3.0*)
	so='o'

	# default to GCC 2.X w/shared libraries
	case "$cc" in
	'')	cc='shlicc2'
		cccdlflags=' ' ;; # Avoid the dreaded -fpic
	esac

	# default ld to shared library linker
	case "$ld" in
	'')	ld='shlicc2'
		lddlflags='-r' ;; # this one is necessary
	esac

	# Must preload the static shared libraries.
	libswanted="Xpm Xaw Xmu Xt SM ICE Xext X11 $libswanted"
	libswanted="rpc curses termcap $libswanted"
	;;
3.1*)
	# ELF dynamic link libraries starting in 3.1
        useshrplib='true'
	so='so'
	dlext='so'

	case "$cc" in
	'')	cc='cc'			# cc is gcc2 in 3.1
		cccdlflags="-fPIC"
		ccdlflags=" " ;;
	esac

	case "$ld" in
	'')	ld='ld'
		lddlflags="-shared -x $lddlflags" ;;
	esac
	;;
esac

