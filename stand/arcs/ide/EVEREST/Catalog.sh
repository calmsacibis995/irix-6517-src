#!/bin/sh
# Collection of all the catalog structures in different subdirectories.
# Creates catalog_t *Global_Catalog[] having the porinters to 
# catalog structures in each directory.
#
#ident "arcs/ide/EVEREST/Catalog.sh $Revision: 1.5 $"

FILEPFX=Catalog_
STRUPFX=_catalog
STRUNAME=catalog_t

FILENAME=${PRODUCT}_catalog.c
rm -f ${FILENAME}
echo "#include <sys/types.h>\n#include <everr_hints.h>\n" >> ${FILENAME}

STRU=


##################### distinguish between IP19/ IP21 ####################
#if [ $PRODUCT = BB_EVEREST64_RS ||$PRODUCT = BB_EVEREST ]; then
#	X=`find . -type d -print | egrep -v "r4k|IP19"` 
#else
#	X=`find . -type d -print` 
#fi

echo product = $PRODUCT
if [ $PRODUCT = EVEREST32 ]; then
	X=`find . -type d -print | egrep -v "fpu"` 
else
	X=`find . -type d -print | egrep -v "r4k|IP19"` 
fi

for i in $X
do

    D=`basename $i`
    F=$i/${FILEPFX}${D}.c
    if [ -f $F ] -a [ "`grep -i ${D}${STRUPFX} $F > /dev/null 2>&1 `" = 0 ]
    then
	    STRU=${STRU}${D}${STRUPFX}" "
	    echo "extern ${STRUNAME} ${D}${STRUPFX};" >> ${FILENAME}
    fi
done

echo "\n\n\n\ncatalog_t	*Global_Catalog[] = { " >> ${FILENAME}

for i in ${STRU}
do
    echo "\t\t&${i}," >> ${FILENAME}
done
echo "\t\t0\n};" >> ${FILENAME}

