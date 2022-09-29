#!/bin/sh

usage()
{
	echo "usage: $0 [-b] [-i] [-d] -f source -o dest"
}

# be sure we can find dvhfile and makeproto, which are in /etc

PATH=$PATH:/etc

BOOT=0
source=
dest=
INSTALL=0
DEBUG=0

while getopts bdo:if: c
do
	case $c in
	b)	BOOT=1;;
	d)	DEBUG=1;;
	i)	INSTALL=1;;
	f)	source=$OPTARG;;
	o)	dest=$OPTARG;;
	\?)	usage
		exit 1;;
	esac
done

if [ -z "$source" -o -z "$dest" ]
then
	usage
	exit 1
fi

dir=`pwd`

source=`echo $source $dir | nawk '
{
	if (1 == index( $1, "/" ))
		print $1
	else
		print $2 "/" $1
}'`

echo "Source image directory: $source"

dest=`echo $dest $dir | nawk '
{
	if (1 == index( $1, "/" ))
		print $1
	else
		print $2 "/" $1
}'`

echo "Destination cd image directory: $dest"

if [ $BOOT -eq 1 -a $INSTALL -eq 0 ]
then
	echo "Must have -i option for -b option"
	exit 1
fi

tmpdir=${TMPDIR:-/usr/tmp}/_VH_$$
imagedir=$dest/_image_$$

echo "Creating image of: $source"

# Set up a directory that is the image we want on the CD ROM.

if [ ! -d $dest ]
then
	mkdir -p $dest
fi

cd $dest

if [ -s cdrom.efs -o -s cdrom.vh ]
   then echo "Warning: this creates cdrom.efs, and cdrom.vh, existing"
	echo "\tones will be overwritten..."
	sleep 5
fi

if [ $DEBUG -eq 0 ]
then
	trap "rm -rf $imagedir $tmpdir PROTO.$$; trap 0; exit 1" 0 1 2 15
fi

if [ $INSTALL -eq 1 ]
then
	mkdir -p $imagedir
        echo "$RELEASE:  $PRDESC [$ALPHA]" > $imagedir/RELEASE.info
	ln -s $source $imagedir/dist
	touch $source/.iscd
else
	ln -s $source $imagedir
fi

if [ $BOOT -eq 1 -a ! -f $imagedir/dist/sa ]
then echo $0: Must have a file called $source/sa
	exit 2
fi

rm -f cdrom.*  # Clean out old stuff so that disk space calculations succeed


makeproto $imagedir > PROTO.$$
touch cdrom.efs
cwd=`pwd`

inodes=`wc -l PROTO.$$ | nawk '{ print $1 }'`
realinodes="$inodes"

CYLSIZE=32		   # Phillips DuPont defaults to 32 spt disks

# This number seems to get rounded DOWN by mkfs, so we'll boost it.
# More imporantly, it the ncg parameter in the superblock, is adjusted
# based partly on this, and can be made too small if we are "too close",
# which makes valid inodes invalid, if we are doing a lot of files.
# So round it up even further
# (was +1000).  This makes a 'df' on the CD look strange, but that's
# life.

if [ "$inodes" -gt 32767 ]; then
	inodes=`expr $inodes + 6000`
else # "product CD's, small directory images, etc.
	inodes=`expr $inodes + 1000`
fi

# keep cylgroups down so we don't get too many inodes, unless we are
# dealing with a lot of files, in which case, it needs to be larger,
# or the ncg parameter in the superblock will be too small, and valid
# inodes thereby become invalid.

if [ "$inodes" -gt 32767 ]; then
	cfg=`expr $CYLSIZE \* 300`
else # "product CD's, small directory images, etc.
	cfg=`expr $CYLSIZE \* 100`
fi

# slop should be a multiple of the cyl size, in order to keep the
# partition sizes a multiple of the cylsize, for fx and dvhtool
# Give us a whole bunch of room for huge CD's
SLOP=`expr 2 \* $cfg`
# note that du -sL isn't really completely reliable, if the source filesystem
# is xfs, and the files have holes in them...  Good enough for our
# purposes, though, for now.
space=`du -sL $source | sed 's/[ 	].*//'`
space=`expr \( \( $space + $CYLSIZE - 1 \) / $CYLSIZE \) \* $CYLSIZE`

if [ $BOOT -eq 1 ]
then
	vhspace="`stat -qs $1/* | nawk '
		{ size += ($1 + 511)/512; }
		END { printf "%d\n", size }'`"
	# round up, + cylsize for unused first cyl
	vhspace=`expr \( \( $vhspace + $CYLSIZE - 1 \) / $CYLSIZE \) \* $CYLSIZE + $CYLSIZE`
else
	# Minimum volume header size is 2 cylinders
	vhspace=`expr $CYLSIZE \* 2`
fi

# add slop for inodes, bitmaps, etc
pspace=`expr $space + $SLOP`

# cdrom.efs + cdrom.vh
needed=`expr $pspace + $vhspace`

dfree=`df -b . | sed -e 1d -e 's/[^ 	]*[ 	]*[^ 	]*[ 	]*[^ 	]*[ 	]*[^ 	]*[ 	]*//' -e 's/[ 	].*$//'`
if [ "$dfree" -lt "$needed" ]
then echo Need $needed blocks, only $dfree avail in this directory
	exit 3
fi

if [ $BOOT -eq 1 ]
then
	dname="$RELEASE $ALPHA CDROM Installation disk"
	mkdir -p $tmpdir
	set -e # abort on any errors from here on out
	cwd=`pwd`
	cd $tmpdir
	mkboottape -f $source/sa -x
	mkdir $imagedir/stand
	cp sash* ide* $imagedir/stand
	# Filenames in vh must be 8 characters or less, so we'll rename
	for i in sash.IP??
	do 
		if [ -f "$i" ]; then
			newname=`echo $i |sed "s/\.//"`
			mv $i $newname
		fi
	done
	cd $cwd
	flist="$tmpdir/*"
else
	dname="`hostname -s` `date +'%D %R'` $source backup"
	flist=
fi

echo "Creating tmp volume header as:\t $dest/cdrom.vh"
echo "  with sgilabel: $dname"
dvhfile -f cdrom.vh -n "$dname" -h 1 -s $CYLSIZE -p 8,0,$vhspace,volhdr -p 7,$vhspace,$pspace,efs $flist


echo "Creating EFS filesystem as:\t cdrom.efs\n"
mkfsout="`(cd $imagedir;  mkfs -t efs $cwd/cdrom.efs $pspace $inodes 1 $CYLSIZE $cfg $CYLSIZE $CYLSIZE $cwd/PROTO.$$)`"

minodes=`echo "$mkfsout"|sed -n '/inodes=/s/.*inodes=//p'`

if [ $minodes -lt $realinodes ]
then 
	echo warning, may need to tune slop more.  Wanted at least $realinodes inodes
	echo but we only got $minodes inodes from mkfs:
else
	echo mkfs output:
fi
echo "$mkfsoutput"

# Note that mkfs may make a file smaller than the partitition.  Since we
# need to generate an image as large as the whole volume we may need to
# tack on some null blocks at the end, or a readcapacity on the drive
# will return too small a value, and due to the hack in the dksc driver,
# we will downsize the efs partition, and as a result we won't be able
# to mount it...
# We therefore add blocks if necessary
# used to use ls -Ls, but that isn't really right, and can add too much
# because of holey files on xfs, etc.
efsbytes=`stat -qs cdrom.efs`
efssize=`expr "$efsbytes" / 512`
if [ $efssize -lt $pspace ] ; then
	extra=`expr $pspace - $efssize`
	echo Adding $extra sectors at end of cdrom.efs to bring up to partition size.
	dd if=/dev/zero bs=1b count=$extra >> cdrom.efs
elif [ $efssize -gt $pspace ] ; then
	echo Warning, filesystem image larger than it should be, will not work
	echo Should be no more than "$pspace" blocks, but is "$efssize" blocks.
fi

echo
echo "\ndvhfile file listing on:\tcdrom.vh"
dvhfile -f cdrom.vh -l

echo Checking the built filesystem
fsck -I cdrom.efs
exit 0
