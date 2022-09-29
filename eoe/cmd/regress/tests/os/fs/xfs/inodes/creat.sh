#!/bin/sh

# source in values for XFSDEV and XFSDIR
. /tmp/.envvars

PATH=.:$PATH

cp creattest ${XFSDIR}
cd ${XFSDIR}

creattest 0 65535
creattest 1024 8000
creattest 2048 8000
creattest 4096 8000
creattest 8192 8000
creattest 16384 8000
creattest 65536 8000
creattest 131072 8000

exit 0
