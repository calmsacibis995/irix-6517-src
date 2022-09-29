#! /sbin/sh
# bin -> usr/bin
if ls -ld $rbase/bin 2>/dev/null | grep "^d" > /dev/null 2>&1
then
  cd $rbase/bin
  for name in `ls $rbase/bin`
  do
    if ls -ld $rbase/bin/$name | grep "^l" > /dev/null 2>&1
    then
#	move links

	if ls -ld $rbase/usr/bin/$name > /dev/null 2>&1
	then
#		delete duplicate name 

		rm -f $rbase/bin/$name > /dev/null 2>&1
	else
#		name does not exist in usr/bin

		LINKVAL=`ls -ld "$rbase/bin/$name" | nawk '{ if (substr($11,1,2) == "\.\.") print $11 }'`
		if [ "$LINKVAL"X != X ]
		then
			if ln -s ../"$LINKVAL" "$rbase"/usr/bin/"$name"
			then
				rm $rbase/bin/$name > /dev/null 2>&1
			fi
		else
			mv $rbase/bin/$name $rbase/usr/bin/$name > /dev/null 2>&1
		fi
	fi
    else
#	/bin/$name is not a link.
   	if ls -ld $rbase/bin/$name | grep "^d" > /dev/null 2>&1
	then 
#		dir then move it
		mkdir -p $rbase/usr/bin/$name > /dev/null 2>&1	
		for i in `find $name -type d -print ` ; do
			mkdir -p $rbase/usr/bin/$i > /dev/null 2>&1
		done
		for i in `find $name ! -type d -print `; do
			if ls -ld $rbase/usr/bin/$i > /dev/null 2>&1
			then :
			else
				mv $rbase/bin/$i $rbase/usr/bin/$i > /dev/null 2>&1
			fi
		done	
		rmdir $rbase/bin/$name > /dev/null 2>&1 
	else
#		ordinary file
		if ls -ld $rbase/usr/bin/$name > /dev/null 2>&1
		then :
		else
			mv $rbase/bin/$name $rbase/usr/bin/$name > /dev/null 2>&1
		fi	
	fi
    fi
  done
fi
rm -f $rbase/.bin.mv.sh > /dev/null 2>&1
exit 0
