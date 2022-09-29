
      INTEGER n,ntimes
      PARAMETER (n=2 000 000)
      PARAMETER (ntimes=10)
      DOUBLE PRECISION dummy,t
      INTEGER j,k,iflag
      DOUBLE PRECISION second
      EXTERNAL second
      INTEGER*8 c0, c1
      INTEGER   e0, e1
      INTEGER*4 start_counters
      INTEGER*4 read_counters
      INTEGER*4 print_counters
      EXTERNAL start_counters,read_counters,print_counters
     INTRINSIC dble,max,min,nint,sqrt
      DOUBLE PRECISION a(n),b(n),c(n)
      COMMON a,b,c
      DATA dummy/0.0d0/
      DATA e0/0/,e1/27/
      DO 10 j = 1,n
          a(j) = 1.0d0
          b(j) = 2.0D0
          c(j) = 0.0D0
   10 CONTINUE

      DO 70 k = 1,ntimes
          iflag = start_counters( e0, e1 )
          IF (iflag.lt.0) STOP 'error in start-counters'
          t = second(dummy)
c$doacross local(j) shared(a,c)
          DO 30 j = 1,n
              c(j) = a(j)
   30     CONTINUE
          t = second(dummy) - t
          iflag = read_counters( e0, c0, e1, c1 )
          IF (iflag.lt.0) STOP 'error in read-counters'
          iflag = print_counters( e0, c0, e1, c1 )
          IF (iflag.lt.0) STOP 'error in print-counters'
          print *,'dummy output to confuse optimizer : ',c(k)
          print *,'Time = ',t
   70 CONTINUE

      END
