#
# SED script to extract error messages from ttcp
#

# Purge previous hold area and start accumulating new run
/TTCP-[TR]/{
h
d
}

# Ignore "connection refused" (means ttcp-r didn't start up in time)
/ttcp-t: ERROR - errno=146/{
H
d
}

# Ignore "address already in use" (means a previous ttcp didn't clean up yet)
/ttcp-r: ERROR - errno=125/{
H
d
}

# Ignore "pipe broken" (means ttcp-r croaked - check its log instead)
/ttcp-t: ERROR - errno=32/{
H
d
}

# Error detected: dump it
/ttcp-[tr]: ERROR/{
H
g
p
a\
-------------------------------------------------------
d
}

# Accumulate anything else in hold area
H
d


