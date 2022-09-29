# adm's sh profile


umask 022

# the absense of '.' in this path is quite necessary.  It closes an 
#	otherwise bad security breach.
PATH=/usr/lib/acct:/usr/bsd:/usr/bin:/bin:/etc:/usr/etc
export PATH

if [ -t 0 ]
then
	stty line 1 erase '^H' kill '^U' intr '^C' echoe 
fi

# list directories in columns
ls()	{ /bin/ls -C $*; }
