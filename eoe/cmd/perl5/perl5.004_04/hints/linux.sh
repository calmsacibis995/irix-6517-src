# hints/linux.sh
# Original version by rsanders
# Additional support by Kenneth Albanowski <kjahds@kjahds.com>
#
# ELF support by H.J. Lu <hjl@nynexst.com>
# Additional info from Nigel Head <nhead@ESOC.bitnet>
# and Kenneth Albanowski <kjahds@kjahds.com>
#
# Consolidated by Andy Dougherty <doughera@lafcol.lafayette.edu>
#
# Updated Thu Feb  8 11:56:10 EST 1996

# Updated Thu May 30 10:50:22 EDT 1996 by <doughera@lafcol.lafayette.edu>

# Updated Fri Jun 21 11:07:54 EDT 1996
# NDBM support for ELF renabled by <kjahds@kjahds.com>

# No version of Linux supports setuid scripts.
d_suidsafe='undef'

# perl goes into the /usr tree.  See the Filesystem Standard
# available via anonymous FTP at tsx-11.mit.edu in
# /pub/linux/docs/linux-standards/fsstnd.
# Allow a command line override, e.g. Configure -Dprefix=/foo/bar
case "$prefix" in
'') prefix='/usr' ;;
esac

# gcc-2.6.3 defines _G_HAVE_BOOL to 1, but doesn't actually supply bool.
ccflags="-Dbool=char -DHAS_BOOL $ccflags"

# libc6, aka glibc2, seems to need STRUCT_TM_HASZONE defined.
# Thanks to Bart Schuller <schuller@Lunatech.com>
# See Message-ID: <19971009002636.50729@tanglefoot>
# This is currently commented out for maintenance releases
# but should probably be uncommented for 5.005 or after
# more widespread testing.
#POSIX_cflags='ccflags="$ccflags -DSTRUCT_TM_HASZONE"'

# BSD compatability library no longer needed
set `echo X "$libswanted "| sed -e 's/ bsd / /'`
shift
libswanted="$*"

# Configure may fail to find lstat() since it's a static/inline
# function in <sys/stat.h>.
d_lstat=define

# Explanation?
case "$usemymalloc" in
'') usemymalloc='n' ;;
esac

case "$optimize" in
'') optimize='-O2' ;;
esac

# Are we using ELF?  Thanks to Kenneth Albanowski <kjahds@kjahds.com>
# for this test.
cat >try.c <<'EOM'
/* Test for whether ELF binaries are produced */
#include <fcntl.h>
#include <stdlib.h>
main() {
	char buffer[4];
	int i=open("a.out",O_RDONLY);
	if(i==-1)
		exit(1); /* fail */
	if(read(i,&buffer[0],4)<4)
		exit(1); /* fail */
	if(buffer[0] != 127 || buffer[1] != 'E' ||
           buffer[2] != 'L' || buffer[3] != 'F')
		exit(1); /* fail */
	exit(0); /* succeed (yes, it's ELF) */
}
EOM
if ${cc:-gcc} try.c >/dev/null 2>&1 && ./a.out; then
    cat <<'EOM' >&4

You appear to have ELF support.  I'll try to use it for dynamic loading.
If dynamic loading doesn't work, read hints/linux.sh for further information.
EOM

#For RedHat Linux 3.0.3, you may need to fetch
# ftp://ftp.redhat.com/pub/redhat-3.0.3/i386/updates/RPMS/ld.so-1.7.14-3.i386.rpm
#

else
    cat <<'EOM' >&4

You don't have an ELF gcc.  I will use dld if possible.  If you are
using a version of DLD earlier than 3.2.6, or don't have it at all, you
should probably upgrade. If you are forced to use 3.2.4, you should
uncomment a couple of lines in hints/linux.sh and restart Configure so
that shared libraries will be disallowed.

EOM
    lddlflags="-r $lddlflags"
    # These empty values are so that Configure doesn't put in the
    # Linux ELF values.
    ccdlflags=' '
    cccdlflags=' '
    ccflags="-DOVR_DBL_DIG=14 $ccflags"
    so='sa'
    dlext='o'
    nm_so_opt=' '
    ## If you are using DLD 3.2.4 which does not support shared libs,
    ## uncomment the next two lines:
    #ldflags="-static"
    #so='none'

	# In addition, on some systems there is a problem with perl and NDBM
	# which causes AnyDBM and NDBM_File to lock up. This is evidenced 
	# in the tests as AnyDBM just freezing.  Apparently, this only 
	# happens on a.out systems, so we disable NDBM for all a.out linux
	# systems.  If someone can suggest a more robust test
	#  that would be appreciated.
	#
	# More info:
	# Date: Wed, 7 Feb 1996 03:21:04 +0900
	# From: Jeffrey Friedl <jfriedl@nff.ncl.omron.co.jp>
	#
	# I tried compiling with DBM support and sure enough things locked up
	# just as advertised. Checking into it, I found that the lockup was
	# during the call to dbm_open. Not *in* dbm_open -- but between the call
	# to and the jump into.
	# 
	# To make a long story short, making sure that the *.a and *.sa pairs of
	#   /usr/lib/lib{m,db,gdbm}.{a,sa}
	# were perfectly in sync took care of it.
	#
	# This will generate a harmless Whoa There! message
	case "$d_dbm_open" in
	'')	cat <<'EOM' >&4

Disabling ndbm.  This will generate a Whoa There message in Configure.
Read hints/linux.sh for further information.
EOM
		# You can override this with Configure -Dd_dbm_open
		d_dbm_open=undef
		;;
	esac
fi

rm -f try.c a.out

if /bin/bash -c exit; then
  echo ''
  echo 'You appear to have a working bash.  Good.'
else
  cat << 'EOM' >&4

*********************** Warning! *********************
It would appear you have a defective bash shell installed. This is likely to
give you a failure of op/exec test #5 during the test phase of the build,
Upgrading to a recent version (1.14.4 or later) should fix the problem.
******************************************************
EOM

fi

# On SPARClinux,
# The following csh consistently coredumped in the test directory
# "/home/mikedlr/perl5.003_94/t", though not most other directories.

#Name        : csh                    Distribution: Red Hat Linux (Rembrandt)
#Version     : 5.2.6                        Vendor: Red Hat Software
#Release     : 3                        Build Date: Fri May 24 19:42:14 1996
#Install date: Thu Jul 11 16:20:14 1996 Build Host: itchy.redhat.com
#Group       : Shells                   Source RPM: csh-5.2.6-3.src.rpm
#Size        : 184417
#Description : BSD c-shell

# For this reason I suggest using the much bug-fixed tcsh for globbing
# where available.

if [  ! "`csh -c 'echo $version' 2>/dev/null`"  ] 
then
    echo 'Real csh found (might break); looking for tcsh ...'
    # Use ../UU/loc to find tcsh.  (We run in the hints/ directory.)
    if xxx=`../UU/loc tcsh blurfl $pth`; $test -f "$xxx"; then
	echo "Found tcsh.  I'll use it for globbing."
	# We can't change Configure's setting of $csh, due to the way
	# Configure handles $d_portable and commands found in $loclist.
	# We can set the value for CSH in config.h by setting full_csh.
	full_csh=$xxx
    else
	echo "Couldn't find tcsh.  BEWARE:  GLOBBING MIGHT BE BROKEN."
    fi
else
    echo 'Your csh is really tcsh.  Good.'
fi

# Shimpei Yamashita <shimpei@socrates.patnet.caltech.edu>
# Message-Id: <33EF1634.B36B6500@pobox.com>
# 
# MkLinux (osname=linux,archname=ppc-linux), which differs slightly from other
# linuces, needs special flags passed in order for dynamic loading to work.
# instead of the recommended:
# ccdlflags='-rdynamic'
# 
# it should be:
# ccdlflags='-Wl,-E'

