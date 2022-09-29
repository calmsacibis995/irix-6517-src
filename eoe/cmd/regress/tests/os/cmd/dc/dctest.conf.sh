#!/bin/csh -f

if (-x /bin/dc) then
	exit 0;
else
	exit 1;
endif 