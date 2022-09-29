#!/bin/csh -f

set dcerror = 0

if (`echo "[q]sa[ln0=aln256%Pln256/snlbx]sb340582483272snlbxq" | /bin/dc` != "HELLO") then
	@ dcerror += 1
	$ERROR dc\'s \"P\" function is broken.
endif

if (`echo "10k 2o 2i 01.01pq" | /bin/dc` != 1.01) then
	@ dcerror += 1
	$ERROR dc\'s base conversion is confused.
	$INFO Command is: "echo '10k 2o 2i 01.01pq' | dc."
	$INFO Returned value is: `echo '10k 2o 2i 01.01pq' | /bin/dc`, not 1.01.
endif

exit $dcerror


