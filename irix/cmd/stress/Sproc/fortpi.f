c - 
c -  Approximation to pi - FORTRAN
c -
	EXTERNAL pieceofpie			! declare 'work' procedure
	INTEGER nprocs 				! number of processes
	INTEGER nrects				! number of evaluations of integral
	INTEGER status, m_set_procs 		! return status of m_set_procs
	INTEGER eachpiece			! size of each chunk of work
	DOUBLE PRECISION width			! width of each 'rectangle'
	COMMON totalpi				! GLOBAL approximation of pi
	DOUBLE PRECISION totalpi
	DOUBLE PRECISION pi			! value of pi to compare to
	pi = 3.1415926535897932d0
   	write(*,*) 'Number of rectangles: '	! input number of rectangles
	read(*,*) nrects
	if (nrects.le.0) goto 30
   	write(*,*) 'Number of processes: '	! input number of processes
	read(*,*) nprocs
	if (nprocs.le.0) goto 30
	status = m_set_procs(nprocs)		! set number of processes to spawn
	if (status.ne.0) goto 20
	width = 1.d0 / nrects			! calculate width
	eachpiece = nrects / nprocs		!    and size of each task
	totalpi = 0.0d0
	if (m_fork(pieceofpie,eachpiece,width).ne.0) stop 'd0 100 '
	totalpi = width * totalpi
	call m_kill_procs			! kill idle processes
   20	if (status.ne.0) then
		write(*,*) 'Error in m_set_procs'
	else
		write(*,26) totalpi		! print results
   26	  	format(' Pi:     ',f18.16)
		write(*,27) pi - totalpi
   27	  	format(' Error:  ',f18.16)
	endif
   30   stop
	end
c -	
c -	pieceofpie that each process does
c -
	SUBROUTINE pieceofpie(eachpiece, width)
	INTEGER eachpiece			! size of each task
	DOUBLE PRECISION width			! width of each rectangle approximation
	COMMON totalpi
	DOUBLE PRECISION totalpi
	INTEGER starti, endi, i		! starting and ending index for integral
	DOUBLE PRECISION mypiece, x		! local variables to accumulate result
	EXTERNAL m_get_myid, m_lock, m_unlock 
	INTEGER m_get_myid
 	starti = m_get_myid()
	starti = starti * eachpiece		! define starting index
	endi = starti + eachpiece -1	
	mypiece = 0.0d0
	do 100, i = starti, endi		! calculate my piece of pi
	x = (i-0.5d0) * width
	mypiece = mypiece + 4.0d0 / (1.0d0 + x * x)
  100	continue
	call m_lock
	totalpi = totalpi + mypiece		! add my piece to global total
	call m_unlock
	end
