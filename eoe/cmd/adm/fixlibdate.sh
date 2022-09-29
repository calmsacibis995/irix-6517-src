#! /bin/ksh

# the purpose of this script is to be sure that for each dir where
# LIB.so and LIB.a both exist, and are both files (the latter so we do
# not mess up builds where $ROOT/usr/lib etc have symlinks in them), 
# that the .so is newer than the .a, so that the librootrules .a.so
# rule does not kick in.  This is of use for the sgi public roots
# (where the attempt to do the update will fail noisily), and for
# internal roots when the .a may be installed later than the .so from
# a different image.

# The special case of the dso's in /lib,/lib32, and /lib64 are
# also handled.

# It expects $ROOT to be set appropriately.

if [ "$ROOT" = "" ]; then
	echo $0: The \$ROOT variable is not set, please set it first.
	exit 1;
fi

if [ -d = "$1" ]; then
	cmd="echo touch"
else
	cmd=touch
fi

# first the special case
for lib in $ROOT/lib $ROOT/lib32 $ROOT/lib64; do
	if [ ! -d $lib ]; then continue; fi
	export lib
	cd $lib && find . -name \*.so\* -type f -print | (while read dso; do
		dso=${dso#./}
		base=${dso%.so}.a
		if [ -f $ROOT/usr/lib/$base -a $ROOT/usr/lib/$base -nt $dso ]
			then $cmd $lib/$dso # use full name on this, for errors
		else
			basev=${dso%.so.[0-9]*}.a
			if [ ${basev%.so.a}.a = $base ]; then continue; fi
			if [ -f $ROOT/usr/lib/$basev -a $ROOT/usr/lib/$basev -nt $dso ]
				then $cmd $lib/$dso
			fi
		fi
		done )
done

# Now handle all the /usr/lib* dirs and subdirs
for lib in $ROOT/usr/lib $ROOT/usr/lib32 $ROOT/usr/lib64; do
	if [ ! -d $lib ]; then continue; fi
	export lib
	cd $lib && find . \( -name \*.so -o -name \*.so.'[0-9]*' \) -type f -print |
		(while read dso; do
			dso=${dso#./}
			base=${dso%.so}.a
			if [ -f $base -a $base -nt $dso ]
				then $cmd $lib/$dso # use full name on this, for errors
			else
				basev=${dso%.so.[0-9]*}.a
				if [ ${basev%.so.a}.a = $base ]; then continue; fi
				if [ -f $basev -a $basev -nt $dso ]; then $cmd $lib/$dso; fi
			fi
		done )
done
