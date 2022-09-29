@ECHO OFF
REM
REM Set environment variables for building FFSC software
REM Copy this file to "ffscvars.bat" and edit to taste.
REM It may be helpful to CALL the resulting script from
REM autoexec.bat.
REM

REM
REM Machine names
REM   PCHOST is the hostname of the this PC
REM   UNIXHOST is the hostname of the user's unix workstation
REM
SET PCHOST=EVILEMPIRE-PC.ENGR.SGI.COM
SET UNIXHOST=AWESOME.ENGR.SGI.COM

REM
REM WIND_BASE should be the top-level directory that Tornado was
REM installed in
REM
SET WIND_BASE=C:\TORNADO

REM
REM WIND_UNC is the name of the WIND_BASE directory in "UNC" format,
REM i.e. "\\<host>\<share>\<dir>\<subdir>..."
REM For example:   \\mypc\shareddir\top
REM UNC format is required for source directories, since GNUmake's VPATH
REM variable gets confused with normal MS-DOS paths that involve a drive
REM letter.
REM
SET WIND_UNC=C:\TORNADO

REM
REM MW_UNC is the UNC-format name of the top-level MetaWindows directory.
REM
SET MW_UNC=C:\METAWNDO

REM
REM UNIX_UNC is the UNC-format name of a remote (presumably unix) directory
REM containing source code. Typically, this would be a ptools workarea.
REM 
SET UNIX_UNC=D:


REM
REM These are the directories containing various parts of the FFSC
REM software:
REM   VX_SRC    - VxWorks source code
REM   VX_LIB    - VxWorks libraries
REM   MW_HDR    - MetaWindows headers
REM   MW_LIB    - MetaWindows libraries
REM   SGI_SRC   - SGI-produced source code
REM   GUI_SRC   - SGI/MetaGraphics/Ease-produced gui source code
REM   CDI_SRC   - CDI-produced source code
REM
REM Theoretically, all but VX_LIB could be somewhere in UNIX_UNC, but
REM that could get slow because of network delays. Power users may wish
REM to copy infrequently changing directories (VX_SRC, VX_LIB, CDI_SRC)
REM to a local disk to speed up compiles. Windows 95 briefcases can be
REM helpful for syncing up any changes that may occur. These should all
REM be in UNC format.
REM
SET VX_SRC=%UNIX_UNC%\WIND
SET VX_LIB=%WIND_UNC%\TARGET\LIB
SET MW_HDR=%MW_UNC%\H
SET MW_LIB=%MW_UNC%\LIB
SET SGI_SRC=%UNIX_UNC%\SGI
SET GUI_SRC=%UNIX_UNC%\GUI
SET CDI_SRC=%UNIX_UNC%\CDI

REM
REM BUILD_DIR is the top-level directory in which all object & depend files
REM will be written. If it is on the local machine, it should be in MS-DOS
REM format, other UNC works.
REM
SET BUILD_DIR=%UNIX_UNC%

REM
REM WIND_REGISTRY is the TCP/IP hostname of the Wind River registry daemon
REM
SET WIND_REGISTRY=evilempire-pc.engr.sgi.com

REM
REM These variables are required by Tornado and should not be changed
REM
SET GCC_EXEC_PREFIX=%WIND_BASE%\HOST\X86-WIN32\LIB\GCC-LIB\
SET WIND_HOST_TYPE=x86-win32
set VCOPTS=-DNCLR16
