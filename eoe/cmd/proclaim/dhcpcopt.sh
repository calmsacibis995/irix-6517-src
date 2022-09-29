#! /sbin/sh

IS_ON=/sbin/chkconfig
ROUTE=/usr/etc/route

if $IS_ON verbose ; then
    ECHO=echo
    VERBOSE=-v
else            # For a quiet startup and shutdown
    ECHO=:
    VERBOSE=
fi

LEASE_INFO_FILE=/var/adm/proclaim.lease_info_ascii
IFACE=$1

# For every option that will be processed a TAG must be defined
# The value of the TAG is the description in the 
# /var/adm/proclaim.lease_info_ascii file
# Specific processing can be skipped by commenting the corresponding tag
# Currently the following Options will be processed
GATEWAY_TAG=Routers


# Processing
if [ ! -f "${LEASE_INFO_FILE}.${IFACE}" ]; then
    $ECHO "File ${LEASE_INFO_FILE}.${IFACE} not found."
    exit 1
fi

eval `cat ${LEASE_INFO_FILE}.${IFACE} | sed 's/^\(.*\): \(.*\)$/\1="\2"/'`
# At this point each of the available options are variables
# For example, Routers="192.26.82.1"
#
# Routers processing 
# If there is more than one router just use the first one
#
if [ -n "${GATEWAY_TAG}" ] ; then
    GATEWAYS=`eval echo "\\\$${GATEWAY_TAG}"`
    if [ -z "${GATEWAYS}" ]; then
	$ECHO "No Gateways defined."
    else
	( set -- ${GATEWAYS} ; ${ROUTE} add default ${1} > /dev/null 2>&1 )
    fi
fi    

exit 0
