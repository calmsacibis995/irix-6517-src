#!/bin/sh

load.exp shenzi xfs dist:/test/6.2-mar09h.G
maxpmem.exp 4096
xp_do.exp xp_netoff
sleep 32400
xp_do.exp xp_neton
sleep 32400
xp_do.exp xp_stop
maxpmem.exp 8192
xp_do.exp xp_netoff
sleep 32400
xp_do.exp xp_neton
sleep 32400
xp_do.exp xp_stop

load.exp shenzi efs dist:/test/6.2-mar09h.G
maxpmem.exp 4096
xp_do.exp xp_netoff
sleep 32400
xp_do.exp xp_neton
sleep 32400
xp_do.exp xp_stop
maxpmem.exp 8192
xp_do.exp xp_netoff
sleep 32400
xp_do.exp xp_neton
sleep 32400
xp_do.exp xp_stop

