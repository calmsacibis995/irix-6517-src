# hints/sunos_4_1.sh
# Last modified:  Thu Feb  8 11:46:05 EST 1996
# Andy Dougherty  <doughera@lafcol.lafayette.edu>

case "$cc" in
*gcc*)	usevfork=false 
	# GNU as and GNU ld might not work.  See the INSTALL file.
	;;
*)	usevfork=true ;;
esac

# Configure will issue a WHOA warning.  The problem is that
# Configure finds getzname, not tzname.  If you're in the System V
# environment, you can set d_tzname='define' since tzname[] is
# available in the System V environment.
d_tzname='undef'

# Configure will issue a WHOA warning.  The problem is that unistd.h
# contains incorrect prototypes for some functions in the usual
# BSD-ish environment.  In particular, it has
# extern int	getgroups(/* int gidsetsize, gid_t grouplist[] */);
# but groupslist[] ought to be of type int, not gid_t.
# This is only really a problem for perl if the
# user is using gcc, and not running in the SysV environment.
# The gcc fix-includes script exposes those incorrect prototypes.
# There may be other examples as well.  Volunteers are welcome to
# track them all down :-).  In the meantime, we'll just skip unistd.h
# for SunOS in most of the code. The POSIX extension is built with
# unistd.h because, even though unistd.h has problems, if used with
# care, it helps create a better POSIX extension.
i_unistd='undef'

cat << 'EOM' >&4

You will probably see  *** WHOA THERE!!! ***  messages from Configure for
d_tzname and i_unistd.  Keep the recommended values.  See
hints/sunos_4_1.sh for more information.
EOM

# SunOS 4.1.3 has two extra fields in struct tm.  This works around
# the problem.  Other BSD platforms may have similar problems.
POSIX_cflags='ccflags="$ccflags -DSTRUCT_TM_HASZONE"'

# The correct setting of groupstype depends on which version of the C
# library is used.  If you are in the 'System V environment'
# (i.e. you have /usr/5bin ahead of /usr/bin in your PATH), and
# you use Sun's cc compiler, then you'll pick up /usr/5bin/cc, which
# links against the C library in /usr/5lib.  This library has
# groupstype='gid_t'.
# If you are in the normal BSDish environment, then you'll pick up
# /usr/ucb/cc, which links against the C library in /usr/lib.  That
# library has groupstype='int'.
#
# If you are using gcc, it links against the C library in /usr/lib
# independent of whether or not you are in the 'System V environment'.
# If you want to use the System V libraries, then you need to 
# manually set groupstype='gid_t' and add explicit references to 
# /usr/5lib when Configure prompts you for where to look for libraries.
#
# Check if user is in a bsd or system 5 type environment
if cat -b /dev/null 2>/dev/null
then # bsd
      groupstype='int'
else # sys5
    case "$cc" in
	*gcc*) groupstype='int';; # gcc doesn't do anything special
	*) groupstype='gid_t';; # /usr/5bin/cc pulls in /usr/5lib/ stuff.
    esac
fi

# If you get the message "unresolved symbol '__lib_version' " while
# linking, your system probably has the optional 'acc' compiler (and
# libraries) installed, but you are using the bundled 'cc' compiler with
# the unbundled libraries.  The solution is either to use 'acc' and the
# unbundled libraries (specifically /lib/libm.a), or 'cc' and the bundled
# library.
# 
# Thanks to William Setzer <William_Setzer@ncsu.edu> for this info.
