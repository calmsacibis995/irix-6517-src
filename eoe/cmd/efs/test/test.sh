#! /bin/sh
#
# Run a bunch of filesystem tests
#

# Start raw i/o test program
dma /dev/rdsk/ips0d1s1 /dev/dsk/ips0d1s1 &

# Start burnin procedures
sh burnin.sh "$@" &
sh burnin.sh "$@" &
sh burnin.sh "$@" &

# Start switch procedures
sh switch.sh "$@" &
sh switch.sh "$@" &
sh switch.sh "$@" &

# Now just wait...
wait
