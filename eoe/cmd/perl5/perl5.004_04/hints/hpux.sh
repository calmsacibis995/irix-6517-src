#! /bin/sh

# hints/hpux.sh
# Perl Configure hints file for Hewlett-Packard's HP-UX 9.x and 10.x
# (Hopefully, 7.x through 11.x.)
#
# This file is based on hints/hpux_9.sh, Perl Configure hints file for
# Hewlett Packard HP-UX 9.x
#
# Use Configure -Dcc=gcc to use gcc.
#
# From: Jeff Okamoto <okamoto@corp.hp.com>
# and
# hints/hpux_10.sh, Perl Configure hints file for Hewlett Packard HP-UX 10.x
# From: Giles Lean <giles@nemeton.com.au>
# and
# Use #define CPU_* instead of comments for >= 10.x.
# Support PA1.2 under 10.x.
# Distinguish between PA2.0, PA2.1, etc.
# Distinguish between MC68020, MC68030, MC68040
# Don't assume every OS != 10 is < 10, (e.g., 11).
# From: Chuck Phillips <cdp@fc.hp.com>

# This version: August 15, 1997
# Current maintainer: Jeff Okamoto <okamoto@corp.hp.com>

#--------------------------------------------------------------------
# Use Configure -Dcc=gcc to use gcc.
# Use Configure -Dprefix=/usr/local to install in /usr/local.
#
# You may have dynamic loading problems if the environment variable
# LDOPTS='-a archive'.  Under >= 10.x, you can instead LDOPTS='-a
# archive_shared' to prefer archive libraries without requiring them.
# Regardless of HPUX release, in the "libs" variable or the ext.libs
# file, you can always give explicit path names to archive libraries
# that may not exist on the target machine.  E.g., /usr/lib/libndbm.a
# instead of -lndbm.  See also note below on ndbm.
#
# ALSO, bear in mind that gdbm and Berkely DB contain incompatible
# replacements for ndbm (and dbm) routines.  If you want concurrent
# access to ndbm files, you need to make sure libndbm is linked in
# *before* gdbm and Berkely DB.  Lastly, remember to check the
# "ext.libs" file which is *probably* messing up the order.  Often,
# you can replace ext.libs with an empty file to fix the problem.
#
# If you get a message about "too much defining", you might have to
# add the following to your ccflags: '-Wp,-H256000'
#--------------------------------------------------------------------

# Turn on the _HPUX_SOURCE flag to get many of the HP add-ons
# regardless of compiler.  For the HP ANSI C compiler, you may also
# want to include +e to enable "long long" and "long double".
#
# HP compiler flags to include (if at all) *both* as part of ccflags
# and cc itself so Configure finds (and builds) everything
# consistently:
#	-Aa -D_HPUX_SOURCE +e
#
# Lastly, you may want to include the "-z" HP linker flag so that
# reading from a NULL pointer causes a SEGV.
ccflags="$ccflags -D_HPUX_SOURCE"

# If you plan to use gcc, then you should uncomment the following line
# so you get the HP math library and not the GCC math library.
# ccflags="$ccflags -L/lib/pa1.1"

# Check if you're using the bundled C compiler.  This compiler doesn't support
# ANSI C (the -Aa flag) nor can it produce shared libraries.  Thus we have
# to turn off dynamic loading.
case "$cc" in
'') if cc $ccflags -Aa 2>&1 | $contains 'option' >/dev/null
    then
	case "$usedl" in
	 '') usedl="$undef"
	     cat <<'EOM' >&4

The bundled C compiler can not produce shared libraries, so you will
not be able to use dynamic loading. 

EOM
	     ;;
	esac
    else
	ccflags="$ccflags -Aa"	# The add-on compiler supports ANSI C
    fi
    # For HP's ANSI C compiler, up to "+O3" is safe for everything
    # except shared libraries (PIC code).  Max safe for PIC is "+O2".
    # Setting both causes innocuous warnings.
    #optimize='+O3'
    #cccdlflags='+z +O2'
    optimize='-O'
    ;;
esac

# Determine the architecture type of this system.
# Keep leading tab below -- Configure Black Magic -- RAM, 03/02/97
	xxOsRevMajor=`uname -r | sed -e 's/^[^0-9]*//' | cut -d. -f1`;
	#xxOsRevMinor=`uname -r | sed -e 's/^[^0-9]*//' | cut -d. -f2`;
if [ "$xxOsRevMajor" -ge 10 ]
then
	# This system is running >= 10.x

	# Tested on 10.01 PA1.x and 10.20 PA[12].x.  Idea: Scan
	# /usr/include/sys/unistd.h for matches with "#define CPU_* `getconf
	# CPU_VERSION`" to determine CPU type.  Note the part following
	# "CPU_" is used, *NOT* the comment.
	#
	# ASSUMPTIONS: Numbers will continue to be defined in hex -- and in
	# /usr/include/sys/unistd.h -- and the CPU_* #defines will be kept
	# up to date with new CPU/OS releases.
	xxcpu=`getconf CPU_VERSION`; # Get the number.
	xxcpu=`printf '0x%x' $xxcpu`; # convert to hex
	archname=`sed -n -e "s/^#[ \t]*define[ \t]*CPU_//p" /usr/include/sys/unistd.h |
	    sed -n -e "s/[ \t]*$xxcpu[ \t].*//p" |
	    sed -e s/_RISC/-RISC/ -e s/HP_// -e s/_/./`;
else
	# This system is running <= 9.x
	# Tested on 9.0[57] PA and [78].0 MC680[23]0.  Idea: After removing
	# MC6888[12] from context string, use first CPU identifier.
	#
	# ASSUMPTION: Only CPU identifiers contain no lowercase letters.
	archname=`getcontext | tr ' ' '\012' | grep -v '[a-z]' | grep -v MC688 |
	    sed -e 's/HP-//' -e 1q`;
	selecttype='int *'
fi


# Remove bad libraries that will cause problems
# (This doesn't remove libraries that don't actually exist)
# -lld is unneeded (and I can't figure out what it's used for anyway)
# -ldbm is obsolete and should not be used
# -lBSD contains BSD-style duplicates of SVR4 routines that cause confusion
# -lPW is obsolete and should not be used
# The libraries crypt, malloc, ndir, and net are empty.
# Although -lndbm should be included, it will make perl blow up if you should
# copy the binary to a system without libndbm.sl.  See ccdlflags below.
set `echo " $libswanted " | sed -e 's@ ld @ @' -e 's@ dbm @ @' -e 's@ BSD @ @' -e 's@ PW @ @'`
libswanted="$*"

# By setting the deferred flag below, this means that if you run perl
# on a system that does not have the required shared library that you
# linked it with, it will die when you try to access a symbol in the
# (missing) shared library.  If you would rather know at perl startup
# time that you are missing an important shared library, switch the
# comments so that immediate, rather than deferred loading is
# performed.  Even with immediate loading, you can postpone errors for
# undefined (or multiply defined) routines until actual access by
# adding the "nonfatal" option.
# ccdlflags="-Wl,-E -Wl,-B,immediate $ccdlflags"
# ccdlflags="-Wl,-E -Wl,-B,immediate,-B,nonfatal $ccdlflags"
ccdlflags="-Wl,-E -Wl,-B,deferred $ccdlflags"

usemymalloc='y'
alignbytes=8
# For native nm, you need "-p" to produce BSD format output.
nm_opt='-p'

# When HP-UX runs a script with "#!", it sets argv[0] to the script name.
toke_cflags='ccflags="$ccflags -DARG_ZERO_IS_SCRIPT"'

# If your compile complains about FLT_MIN, uncomment the next line
# POSIX_cflags='ccflags="$ccflags -DFLT_MIN=1.17549435E-38"'

# Comment this out if you don't want to follow the SVR4 filesystem layout
# that HP-UX 10.0 uses
case "$prefix" in
'') prefix='/opt/perl5' ;;
esac

# Date: Fri, 6 Sep 96 23:15:31 CDT
# From: "Daniel S. Lewart" <d-lewart@uiuc.edu>
# I looked through the gcc.info and found this:
#   * GNU CC compiled code sometimes emits warnings from the HP-UX
#     assembler of the form:
#          (warning) Use of GR3 when frame >= 8192 may cause conflict.
#     These warnings are harmless and can be safely ignored.
