#!/bin/sh

killall runtests
killall badifred bounce cbuf flash gsync \
	mode parallel rbuf rectwrite winset worms
killall badifred-N32 bounce-N32 cbuf-N32 flash-N32 gsync-N32 \
	mode-N32 parallel-N32 rbuf-N32 rectwrite-N32 winset-N32 worms-N32

exit 0
