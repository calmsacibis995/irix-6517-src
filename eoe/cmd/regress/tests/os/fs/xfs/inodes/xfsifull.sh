#!/bin/sh

# source in values for XFSDEV and XFSDIR
. /tmp/.envvars

PATH=.:$PATH

cp creattest ${XFSDIR}
cd ${XFSDIR}

creattest 0 67108864
exit 0
