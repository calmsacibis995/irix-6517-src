#!/bin/sh

#
#OLDDIR is /usr/etc/boot.O because we expect inst to have already
#placed a symlink to /var/boot in place of /usr/etc/boot.
#
OLDDIR=$rbase/usr/etc/boot.O
NEWDIR=$rbase/var/boot

#
#If /usr/etc/boot.O is not a directory, then
#we probably did this before.  Don't do it again.
#
if [ ! -d $OLDDIR ]
then
	exit 0
fi

#
#We're running from /var/boot, so it had
#better exist.
#
if [ ! -d $NEWDIR ]
then
	echo "tftpd.inst.sh: $NEWDIR should exist!!"
	exit 1
fi

#
#This function assumes that it is executing in the /usr/etc/boot.O.
#It move symlinks from /usr/etc/boot.O to /var/boot.  If the symlink
#starts wih .. and doesn't go back down into boot.O, then it will work.
#If the symlink starts with / and doesn't go back down into boot.O,
#then it will work.  Otherwise, it will probably work, but there are
#no guarantees if the symlink is arbitrarily complex.
#
#This function is only trying to handle the common cases, not the
#esoteric ones.
#
#The parameters of this function are:
#	FILE=$1
#
move_bootdir_link() {
	FILE=$1

	REL_LINK=`ls -l "$FILE" | nawk '{ if (substr($11, 1, 2) == "\.\.") print $11 }'`
	if [ "$REL_LINK"X != X ]
	then
		REL_LINK=`echo $REL_LINK | sed -e 's#\.\.\/##1'`
		if [ -l $rbase/var ]
		then
			ln -s ../../../usr/etc/"$REL_LINK" "$NEWDIR"/"$FILE"
		else
			ln -s ../../usr/etc/"$REL_LINK" "$NEWDIR"/"$FILE"
		fi
		return 0
	fi
	set `ls -l $FILE`
	shift 10
	NONREL_LINK="$1"
	if [ "$NONREL_LINK"X != X ]
	then
		ln -s "$NONREL_LINK" "$NEWDIR"/"$FILE"
		return 0
	fi
	return 1
}

#
#Just make a symlink with the name FILE in the directory DEST_DIR
#that has the same value as SRC_DIR/FILE.  This won't handle
#complicated relative symlinks, but simple ones should still work.
#
#Return 0 on success and 1 on failure.
#The parameters are:
#	FILE=$1
#	SRC_DIR=$2
#	DEST_DIR=$3
#
move_link() {
	FILE=$1
	SRC_DIR=$2
	DEST_DIR=$3

	set `ls -l $SRC_DIR/$FILE` 
	shift 10
	LINK="$1"
	if ln -s "$LINK" "$DEST_DIR"/"$FILE"
	then
		return 0
	else
		echo "$SRC_DIR/$FILE could not be moved to $DEST_DIR."
		echo "You will need to move it manually."
		return 1
	fi
}	

#
#move_dir() attempts to copy directory DIR in SRC_DIR to a new directory
#DIR in DEST_DIR.  Symbolic links are just remade with the same value,
#so relative links that go above /usr/etc/boot.O may no longer work.
#
#The function returns 0 on success and 1 on any failure.
#It is up to the caller to rm -r the directory upon success.
#
#This function assumes that it is in directory SRC_DIR upon entry.
#The parameters to this function are:
#	DIR=$1
#	SRC_DIR=$2
#	DEST_DIR=$3
#We cannot use named variables for the parameters, because this
#shell function is recursive and all shell variables other than
#the numbered ones are global.
#
move_dir() {

	if mkdir "$3"/"$1" >/dev/null 2>&1
	then
		true
	else
		echo "$2/$1 could not be moved to $3."
		echo "You will need to move it manually."
		return 1
	fi
	cd "$2"/"$1"
	RETURN=0
	for FILE in *
	do
		if [ -l "$FILE" ]
		then
			if move_link "$FILE" "$2"/"$1" \
				     "$3"/"$1"
			then
				:
			else
				RETURN=1
			fi
			continue
		fi
		if [ -f "$FILE" ]
		then
			if cp "$FILE" "$3"/"$1" >/dev/null 2>&1
			then
				:	
			else
				echo "$2/$1/$FILE could not be moved"
				echo "to $3/$1.  You will need to move"
				echo "it manually."
				RETURN=1
			fi
			continue
		fi
		if [ -d "$FILE" ]
		then
			if move_dir "$FILE" "$2"/"$1" \
				    "$3"/"$1"
			then
				:
			else
				RETURN=1
			fi
			continue;
		fi
	done
	cd "$2"
	return "$RETURN"
}

#
#For each file in /usr/etc/boot.O:
#	if a file of the same name already exists in /var/boot
#	 then just print out a warning and skip it.
#	if the file is a symlink then call move_bootdir_link()
#	 to create an equivalent symlink in /var/boot.
#	if the file is a file then copy it to /var/boot.
#	if the file is a directory then call move_dir() to
#	 try to copy the directory over to /var/boot.  This
#	 function will handle nested directories and will try
#	 to properly handle symlinks found below.
#
#If all of the files are moved successfully, then remove the
#entire directory /usr/etc/boot.O.
#
cd $OLDDIR
REMOVE=0
for FILE in *
do
	if [ -l "$NEWDIR"/"$FILE" -o -f "$NEWDIR"/"$FILE" -o \
	     -d "$NEWDIR"/"$FILE" ]
	then
		echo "Couldn't move $OLDDIR/$FILE to $NEWDIR."
		echo "The file already exists."
		REMOVE=1
		continue;
	fi
	if [ -l $FILE ]
	then
		if move_bootdir_link "$FILE"
		then
			:
		else
			REMOVE=1
		fi
		continue
	fi
	if [ -f $FILE ]
	then
		if cp -r $FILE $NEWDIR/$FILE >/dev/null 2>&1
		then
			:
		else
			echo "Couldn't cleanly copy $OLDDIR/$FILE"
			echo "to $NEWDIR.  You'll have to do it by hand."
			REMOVE=1
		fi
		continue
	fi
	if [ -d $FILE ]
	then
		if move_dir "$FILE" "$OLDDIR" "$NEWDIR"
		then
			:
		else
			REMOVE=1
		fi
		continue;
	fi
done
cd $rbase
if [ "$REMOVE" = 0 ]
then
	rm -rf $OLDDIR >/dev/null 2>&1
fi
exit 0
		
			
		
