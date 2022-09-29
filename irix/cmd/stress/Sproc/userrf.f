c -*************************************************************************
c -									  *
c - 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
c -									  *
c -  These coded instructions, statements, and computer programs  contain  *
c -  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
c -  are protected by Federal copyright law.  They  may  not be disclosed  *
c -  to  third  parties  or copied or duplicated in any form, in whole or  *
c -  in part, without the prior written consent of Silicon Graphics, Inc.  *
c -									  *
c -*************************************************************************/

#ident "$Revision: 1.12 $"
	PROGRAM userrf

#include "ulocks.h"
#include "sys/schedctl.h"
#include "sys/prctl.h"

c -
c - userrf - test some error handling in usinit & some of the usconfig options
c -


	external slave, slave2
	INTEGER*4 schedctl
	INTEGER*4 verbose

	INTEGER*4 osz, sz, junk
	INTEGER sb(12),out
#if _MIPS_SZPTR == 64
	INTEGER*8 usp
	INTEGER*8 l
	INTEGER*8 sm(6), sd(6)
	INTEGER*8 s
	INTEGER*8 hp(3)
	INTEGER*8 castst
#else
	INTEGER*4 usp
	INTEGER*4 l
	INTEGER*4 sm(6), sd(6)
	INTEGER*4 s
	INTEGER*4 hp(3)
	INTEGER*4 castst
#endif
	INTEGER*4 hints


	sz = 8192
	ASSIGN 999 TO out
	call ussettrace

	hints = schedctl(SETHINTS, 0, 0)
	if (hints .eq. -1) then
		call perror("userrf:schedctl SETHINTS failed")
		goto out
	endif
	write(*,800) hints
800	format('userrf:hints at', Z, '\n')

	if (usconfig(CONF_ARENATYPE, US_SHAREDONLY) .lt. 0) then
		call perror("userrf:usconfig ARENATYPE failed")
		goto out
	endif
	if (usconfig(CONF_LOCKTYPE, US_DEBUG) .lt. 0) then
		call perror("userrf:usconfig LOCKTYPE failed")
		goto out
	endif
	if (usconfig(CONF_INITSIZE, sz) .lt. 0) then
		call perror("userrf:usconfig INITSIZE failed")
		goto out
	endif
   10	usp = usinit("/usr/tmp/userrf.arena")
	if (usp .eq. NULL) then
		call perror("userrf:usinit failed")
		goto out
	endif
	if (usconfig(CONF_GETSIZE, usp) .ne. sz) then
		call perror("userrf:usconfig size failed")
		goto out
	endif
	write(*,900) sz
900	format('userrf:usinit worked for size', I, '\n')

c - since SHAREDONLY - file should be unlinked already */
	if (stat("/usr/tmp/userrf.arena", sb) .ne. 0) then
		write(*,*) "userrf:ERROR:stat of '/usr/tmp/userrf.arena' worked!\n"
		goto out
	endif

c - test m_fork
	junk = m_set_procs(2)
	l = 9
	if (m_fork(slave, 1, l, sb) .ne. 0) then
		call perror("userrf:m_fork failed")
		goto out
	endif
	call m_kill_procs()

c - now test out dump sema and dump lock */
	l = usnewlock(usp)
	if (l .lt. 0) then
		call perror("userrf:usnewlock FAILED")
		goto out
	endif
	call usdumplock(l, 0, "userrf:new lock")
	junk = ussetlock(l)
	call usdumplock(l, 0, "userrf:after lock")
	junk = usunsetlock(l)
	call usdumplock(l, 0, "userrf:after unlock")
	junk = usctllock(l, CL_METERRESET)
	junk = usctllock(l, CL_DEBUGRESET)
	call usdumplock(l, 0, "userrf:after reset")

c - now test out dump sema and dump lock */
	s = usnewsema(usp, 1)
	if (s .lt. 0) then
		call perror("userrf:usnewsema FAILED")
		goto out
	endif
	junk = usconfig(CONF_HISTON, usp)
	junk = usctlsema(s, CS_HISTON)
	junk = usctlsema(s, CS_METERON)
	junk = usctlsema(s, CS_DEBUGON)
	call usdumpsema(s, 0, "userrf:new sema")
	junk = uspsema(s)
	call usdumpsema(s, 0, "userrf:after psema")
	junk = usvsema(s)
	call usdumpsema(s, 0, "userrf:after vsema")
	junk = usctlsema(s, CS_DEBUGFETCH, sd)
	junk = usctlsema(s, CS_METERFETCH, sm)
	write(*,901) sd(1), sd(2), sd(3), sd(4)
901	format('semaphore debug:owner ', I, 'owner pc ', Z, 'last pid ', I, 'last pc ', Z, '\n')

	write(*,902) sm(1), sm(2), sm(3), sm(4), sm(5), sm(6)
902	format('semaphore meter: phits ', I, 'psemas ', I, 'vsemas ', I, 'vnowait ', I, 'nwait ', I, 'maxnwait ', I, '\n')
	junk = usctlsema(s, CS_METERRESET)
	junk = usctlsema(s, CS_DEBUGRESET)
	call usdumpsema(s, 0, "userrf:after reset")

	junk = usconfig(CONF_HISTFETCH, usp, hp)
	write(*,903) hp(1), hp(2), hp(3)
903	format('Hist current ', Z, ' entries', I, ' errors', I, '\n')
	junk = usconfig(CONF_HISTRESET, usp)
	junk = usconfig(CONF_HISTFETCH, usp, hp)
	write(*,904) hp(1), hp(2), hp(3)
904	format('Hist current after reset ', Z, ' entries', I, ' errors', I, '\n')

c - test uscas
	castst = 1
#if _MIPS_SZPTR == 64
	if (uscas(loc(castst), 1_8, 2_8, usp) .ne. 1) then
#else
	if (uscas(castst, 1, 2, usp) .ne. 1) then
#endif
		call perror("uscas failed")
		goto out
	endif
	if (castst .ne. 2) then
		write(*,905) castst
905		format('castst should be 2 is' Z, '\n')
	endif
	write(*,906) 
906	format('userrf:after cas test', '\n')

c - test sproc (mostly checking fortran manifest definitions
	call prctl(PR_SETEXITSIG, 0, 0)
	if (sproc(slave2, PR_SALL, 0) .lt. 0) then
		call perror("sproc failed")
		goto out
	endif
	call wait(0)
	write(*,907) 
907	format('userrf:after sproc/wait', '\n')
	

999	stop
	end

        integer*4 function slave(arg1, arg2, arg3)

	INTEGER*4 myid

	myid = m_get_myid()
c - single thread fortran IO
	call m_lock()
	write(*,700) myid, arg1, arg2, arg3
	call m_unlock()
700	format('userrf:slave tid ', I, 'arg1 ', Z, 'arg2 ', Z, 'arg3 ', Z, '\n')
	end

        integer*4 function slave2(arg1)
	end
