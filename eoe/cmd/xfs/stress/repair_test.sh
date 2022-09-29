#!/bin/csh -f
echo -n ====; date
set dev = $1
set test = /test_$dev:t
cd /tmp
rm -f randno$$ randno$$.c
cat > randno$$.c << EOF
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
main(int argc, char **argv)
{
        int             low = atoi(argv[1]);
        int             high = atoi(argv[2]);
        struct timeval  tv;
        int             val;

        gettimeofday(&tv, NULL);
        srandom((uint)tv.tv_sec ^ (uint)tv.tv_usec ^ (uint)getpid());
        val = (random() % (high - low + 1)) + low;
        printf("%d\n", val);
}
EOF
cc -o randno$$ randno$$.c
set blog = `/tmp/randno$$ 9 16`
set dlog = `/tmp/randno$$ $blog 16`
if ( `/tmp/randno$$ 0 9` == "0" ) then
	set nopt = "version=1"
else
	set nopt = "log=$dlog"
endif
@ maxilog = $blog - 1
if ( $maxilog > 11 ) set maxilog = 11
set ilog = `/tmp/randno$$ 8 $maxilog`
echo mkfs -b log=$blog -n $nopt -i log=$ilog $dev
mkfs -b log=$blog -n $nopt -i log=$ilog $dev
echo mkdir -p $test
mkdir -p $test
if ( `/tmp/randno$$ 0 3` == "0" ) then
	set qflag = "-q"
else
	set qflag = ""
endif
echo mount -o logbufs=8 $qflag $dev $test
mount -o logbufs=8 $qflag $dev $test
echo mkdir $test/perm
mkdir $test/perm
echo cd $test/perm
cd $test/perm
set permc = `/tmp/randno$$ 2 32`
switch ( $permc )
case "2":
	set llow = 10
	set lhigh = 16
	breaksw
case "3":
	set llow = 7
	set lhigh = 10
	breaksw
case "4":
	set llow = 5
	set lhigh = 8
	breaksw
case "5":
	set llow = 5
	set lhigh = 7
	breaksw
case "6":
	set llow = 4
	set lhigh = 6
	breaksw
case "7":
case "8":
case "9":
	set llow = 4
	set lhigh = 5
	breaksw
case "10":
	set llow = 3
	set lhigh = 5
	breaksw
case "11":
case "12":
case "13":
case "14":
case "15":
case "16":
case "17":
	set llow = 3
	set lhigh = 4
	breaksw
case "18":
case "19":
case "20":
case "21":
case "22":
case "23":
case "24":
case "25":
case "26":
case "27":
case "28":
case "29":
case "30":
case "31":
	set llow = 3
	set lhigh = 3
	breaksw
case "32":
	set llow = 2
	set lhigh = 3
	breaksw
endsw
set perml = `/tmp/randno$$ $llow $lhigh`
echo /usr/stress/xfs/permname -c $permc -l $perml
/usr/stress/xfs/permname -l $perml -c $permc
echo cd /
cd /
set count = `/tmp/randno$$ 10000 50000`
set isr = `/tmp/randno$$ 0 1`
if ( $isr == 1 ) then
	set rflag = "-r"
else
	set rflag = ""
endif
set seed = `/tmp/randno$$ 0 2147483647`
set attr_setf = `/tmp/randno$$ 0 4`
if ( $attr_setf == 0 ) then
	set attr_removef = 0
else
	set attr_removef = `/tmp/randno$$ 0 2`
endif
if ( $qflag == "-q" ) then
	set chownf = `/tmp/randno$$ 0 2`
else
	set chownf = 0
endif
set creatf = `/tmp/randno$$ 0 8`
set getdentsf = `/tmp/randno$$ 0 2`
set linkf = `/tmp/randno$$ 0 2`
set mkdirf = `/tmp/randno$$ 0 4`
set mknodf = `/tmp/randno$$ 0 4`
set renamef = `/tmp/randno$$ 0 4`
set rmdirf = `/tmp/randno$$ 0 2`
set symlinkf = `/tmp/randno$$ 0 4`
set unlinkf = `/tmp/randno$$ 0 2`
echo /usr/stress/xfs/fsstress -d $test -n $count $rflag -z -f attr_remove=$attr_removef -f attr_set=$attr_setf -f chown=$chownf -f creat=$creatf -f getdents=$getdentsf -f link=$linkf -f mkdir=$mkdirf -f mknod=$mknodf -f rename=$renamef -f rmdir=$rmdirf -f symlink=$symlinkf -f unlink=$unlinkf -s $seed
/usr/stress/xfs/fsstress -d $test -n $count $rflag -z -f attr_remove=$attr_removef -f attr_set=$attr_setf -f chown=$chownf -f creat=$creatf -f getdents=$getdentsf -f link=$linkf -f mkdir=$mkdirf -f mknod=$mknodf -f rename=$renamef -f rmdir=$rmdirf -f symlink=$symlinkf -f unlink=$unlinkf -s $seed
echo umount $test
umount $test
set trash = `/tmp/randno$$ 1 32`
set mode = `/tmp/randno$$ 0 3`
@ nbits = 8 * (1 << $blog)
set max = `/tmp/randno$$ 1 $nbits`
set seed = `/tmp/randno$$ 0 2147483647`
if ( `/tmp/randno$$ 0 1` == 1 ) then
	set tinode = "-t inode"
else
	set tinode = ""
endif
echo xfs_db.debug -c blockget -c \"blocktrash -n $trash -${mode} -y $max -t dir -t btbmapd -t attr -t btbmapa $tinode -s $seed\" -c blockfree -c check $dev
xfs_db.debug -c blockget -c "blocktrash -n $trash -${mode} -y $max -t dir -t btbmapd -t attr -t btbmapa $tinode -s $seed" -c blockfree -c check $dev
echo xfs_repair.debug $dev
xfs_repair.debug $dev
echo xfs_db.debug -c check $dev
xfs_db.debug -c check $dev
if ( $status != 0 ) then
	echo "xfs_check failed"
endif
