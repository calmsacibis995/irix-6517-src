#! /bin/sh
#Tag 0x00000f00
#ident "$Revision $"

# miniroot script - used by other mr*rc scripts for common settings

PATH=/sbin:/usr/bin:/usr/bsd:/usr/sbin:/etc:/usr/etc:

# possibly temporary hack to get ~/.swmgrrc to eval to something useful:
HOME=/root; export HOME

# sane terminal modes, erase, kill, intr because documented that way
stty sane tab3 erase '^H' kill '^U' intr '^C' 2>/dev/null

# Scripts catch but no-op SIGINT (from ^C) so that they
# are not disrupted by user hitting ^C, but still allow
# any subshells to default SIGINT.  Ditto SIGQUIT (3), I guess.

trap : 2 3

umask 022
