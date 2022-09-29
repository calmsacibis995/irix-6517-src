CC   tomcatv.f	1.5     (GENIAS)     7/26/91
CC   tomcatv.f	1.6     (SNI)       10/05/94
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
CC                                                                    CC
CC                          T O M C A T V                             CC
CC                                                                    CC
CC     MESHGENERATION WITH THOMPSON SOLVER,  VECTORIZED VERSION       CC
CC                                                                    CC
CC      SET  N = 65, 129, 257      DO  SCALAR AND VECTOR  RUNS        CC
CC                                                                    CC
C Description of the equations and the finite difference approximation C
CC                               in                                   CC
CC W.Gentzsch, K.Neves and H.Yoshihara: Computational Fluid Dynamics: CC
CC  Algorithms and Supercomputers. AGARDograph No. 311, August 1987   CC
CC                                                                    CC
CC Authors:                                                           CC
CC       W.GENTZSCH FH REGENSBURG  AND  GENIAS SOFTWARE GmbH          CC
CC           Erzgebirgstr. 2, 8402 Neutraubling, Germany              CC
CC                                                                    CC
CC                     Formal modifications by                        CC
CC       F.FERSTL GENIAS SOFTWARE GMBH                                CC
CC           Erzgebirgstr. 2, 8402 Neutraubling, Germany              CC
CC                                                                    CC
CC                                                                    CC
CC	Adapted for PAR93 and SPEC                                    CC
CC	By Bodo Parady                                                CC
CC	November 93                                                   CC
CC                                                                    CC
CC      Adapted for SPEC CFP9                                         CC
CC      By Reinhold Weicker                                           CC
CC      Oct. 1994                                                     CC
CC                                                                    CC
CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
C                                                                      C
      INTEGER	NMAX

      PARAMETER (NMAX = 513)

C     NMAX is the size for the array declarations
C
C     The size of the (sub-)array that is actually used is given by
C     the number N which is read from the input file.
C     The relation N <= NMAX must hold.
C
      INTEGER	ITMAX
      REAL*8	EPS, RELFA, REL

      PARAMETER        ( ITMAX = 1000)
      PARAMETER        ( RELFA = 0.98D0 )
      PARAMETER        ( REL   = 2.0D0/RELFA )
      PARAMETER        ( EPS   = 0.5D-8 )
C
      INTEGER	N, ITACT
      INTEGER	I, J, ITER
C
      REAL*8	AA(NMAX,NMAX), DD(NMAX,NMAX), D(NMAX,NMAX)
      REAL*8	X (NMAX,NMAX), Y (NMAX,NMAX)
      REAL*8	RX(NMAX,NMAX), RY(NMAX,NMAX)
      REAL*8	RXM(ITMAX), RYM(ITMAX)
      REAL*8	DLO, DHI, ALPHA
      REAL*8	A, B, C, XX, XY, YX, YY
      REAL*8	PXX, QXX, PYY, QYY, PXY, QXY
      REAL*8	R
      REAL*8	ABX, ABY
C
C     READ INITIAL DATA
C
      OPEN(10,FILE='TOMCATV.MODEL',STATUS='OLD',ERR=999)
      GOTO 1
 999  PRINT *, 'FILE "TOMCATV.MODEL" DOES NOT EXIST; STOP'
      STOP
   1  CONTINUE
      READ( 5, * ) N, ITACT, DLO, DHI, ALPHA

      IF (ITACT .GT. ITMAX .OR. N .GT. NMAX ) THEN
	PRINT *, 'Please recompile:  This version is lacking Storage'
	STOP
      ENDIF
C
C     READ START CONFIGURATION
C
      DO   10    J = 1,N
      DO   10    I = 1,N
           READ(10,600,END=990) X(I,J),Y(I,J)
   10 CONTINUE
      GOTO 2
  990 PRINT *, '"TOMCATV.MODEL INCOSISTANT; STOP'
      STOP
    2 CONTINUE
C
C     START ITERATION LOOP 140
C
*FLIP140
      DO      140      ITER = 1, ITACT
C       
C     Residuals of ITER iteration
C  
        RXM(ITER)  = 0.D0
        RYM(ITER)  = 0.D0
C
        DO     60    J = 2,N-1
C
          DO     50    I = 2,N-1
            XX = X(I+1,J)-X(I-1,J)
            YX = Y(I+1,J)-Y(I-1,J)
            XY = X(I,J+1)-X(I,J-1)
            YY = Y(I,J+1)-Y(I,J-1)
            A  = 0.25D0  * (XY*XY+YY*YY)
            B  = 0.25D0  * (XX*XX+YX*YX)
            C  = 0.125D0 * (XX*XY+YX*YY)
            AA(I,J) = -B
            DD(I,J) = B+B+A*REL
            PXX = X(I+1,J)-2.D0*X(I,J)+X(I-1,J)
            QXX = Y(I+1,J)-2.D0*Y(I,J)+Y(I-1,J)
            PYY = X(I,J+1)-2.D0*X(I,J)+X(I,J-1)
            QYY = Y(I,J+1)-2.D0*Y(I,J)+Y(I,J-1)
            PXY = X(I+1,J+1)-X(I+1,J-1)-X(I-1,J+1)+X(I-1,J-1)
            QXY = Y(I+1,J+1)-Y(I+1,J-1)-Y(I-1,J+1)+Y(I-1,J-1)
C
C     CALCULATE RESIDUALS ( EQUAL TO RIGHT HAND SIDES OF EQUS.)
C
            RX(I,J)   = A*PXX+B*PYY-C*PXY
            RY(I,J)   = A*QXX+B*QYY-C*QXY
C
   50     CONTINUE
   60   CONTINUE
C
C     DETERMINE MAXIMUM VALUES RXM, RYM OF RESIDUALS
C
        DO     80    J = 2,N-1
          DO     80    I = 2,N-1
            RXM(ITER) = MAX(RXM(ITER), ABS(RX(I,J)))
            RYM(ITER) = MAX(RYM(ITER), ABS(RY(I,J)))
   80   CONTINUE
C
C     SOLVE TRIDIAGONAL SYSTEMS (AA,DD,AA) IN PARALLEL, LU DECOMPOSITION
C
        DO     90    I = 2,N-1
          D(I,2) = 1.D0/DD(I,2)
   90   CONTINUE
        DO    100     J = 3,N-1
          DO    100     I = 2,N-1
            R       = AA(I,J)*D(I,J-1)
            D (I,J) = 1.D0/(DD(I,J)-AA(I,J-1)*R)
            RX(I,J) = RX(I,J) - RX(I,J-1)*R  
            RY(I,J) = RY(I,J) - RY(I,J-1)*R
  100   CONTINUE
        DO    110     I = 2,N-1
          RX(I,N-1) = RX(I,N-1)*D(I,N-1)
          RY(I,N-1) = RY(I,N-1)*D(I,N-1)
  110   CONTINUE
        DO    120     J = N-2,2,-1
          DO    120     I = 2,N-1
            RX(I,J) = (RX(I,J)-AA(I,J)*RX(I,J+1))*D(I,J)
            RY(I,J) = (RY(I,J)-AA(I,J)*RY(I,J+1))*D(I,J)
  120   CONTINUE 
C
C     ADD CORRECTIONS OF ITER ITERATION
C
        DO    130     J = 2,N-1
          DO    130     I = 2,N-1
            X(I,J) = X(I,J)+RX(I,J)
            Y(I,J) = Y(I,J)+RY(I,J)
  130   CONTINUE
C
        ABX  = ABS(RXM(ITER))
        ABY  = ABS(RYM(ITER))
        IF (ABX.LE.EPS.AND.ABY.LE.EPS)  GOTO  150
  140 CONTINUE
C
C     END OF ITERATION LOOP 140
C
  150 CONTINUE 
C
C     OUTPUT OF CONVERGENCE BEHAVIOR
C
      WRITE (6,1100)
      WRITE (6,1200)
      DO     160     I = 1, ITER-1
        WRITE (6,1300)   I, RXM(I), RYM(I)
  160 CONTINUE
C
  600 FORMAT(D20.14,D20.14)
 1100 FORMAT(///,'     2-D ITERATION BEHAVIOR')
 1200 FORMAT(/,'   IT    X-RES      Y-RES',/)
 1300 FORMAT(1X,I4,E11.4,E11.4)
C 
      STOP
      END

