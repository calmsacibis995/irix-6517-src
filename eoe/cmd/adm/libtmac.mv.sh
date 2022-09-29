#!/sbin/sh
# set -xv
# This shell script creates /usr/share/lib/tmac and then
# moves the contents of /usr/lib/tmac to /usr/share/lib/tmac. 
# inst will then create a symlink from /usr/lib/tmac
# to /usr/share/lib/tmac.
#
# This script will not do the move if if /usr/lib/tmac is 
# already a symlink.
#
# Note:
#   This script is installed and executed out of /etc; very
#   early during the install so that software that install
#   into the old location would wind up in the right place.
if [ ! -l $rbase/usr/lib/tmac ]
then
#   Create /usr/share/lib/tmac.  It's ok if the directory 
#   already exists; we are ignoring errors.
    mkdir -p $rbase/usr/share/lib/tmac > /dev/null 2>&1
    if [ -d $rbase/usr/lib/tmac -o -l $rbase/usr/lib/tmac ]
    then
        cd $rbase/usr/lib/tmac
#       Now move any files and subdirectories
        for i in `find . -type d -print`; do
            mkdir -p $rbase/usr/share/lib/tmac/$i > /dev/null 2>&1
        done
        for i in `find . ! -type d -print`; do
            mv $i $rbase/usr/share/lib/tmac/$i
        done
#       Now delete the source directory
        cd $rbase/etc
        rm -rf $rbase/usr/lib/tmac > /dev/null 2>&1
    fi
fi
rm $rbase/etc/.libtmac.mv.sh > /dev/null 2>&1
exit 0
