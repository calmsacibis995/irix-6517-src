#!/bin/csh

#
# MfgDBGProm shell script file
#

if( ("$PRODUCT" != "RACER" ) && \
    ("$PRODUCT" != "IP30" ) ) then
  echo "PRODUCT env variable != (RACER | IP30). Exit"

  goto EndScript
endif


setenv Input ""

BuildInput:

if( ("$Input" != "dprom") &&     \
    ("$Input" != "prom") &&      \
    ("$Input" != "IP30proms") && \
    ("$Input" != "fprom.bin") && \
    ("$Input" != "clean")     && \
    ("$Input" != "clobber")   && \
    ("$Input" != "ide") ) then
  echo "Build [dprom, prom, IP30proms, fprom.bin, clean, clobber, ide] ? " ; setenv Input $<

  goto BuildInput
endif

setenv MakeProm $Input

setenv ObjDir    RACER.O
setenv PonObjDir RACERp.O

# Clean up for making
foreach ObjFile ( ns16550cons.o )
  rm $WORKAREA/stand/arcs/lib/libsk/$ObjDir/$ObjFile >& /dev/null
end

cd $WORKAREA/stand/arcs/lib/libsk
make clobber

cd $WORKAREA/stand/arcs/ide/IP30/pon
make clobber LIBTYPE=p
make clobber LIBTYPE=

# Remove all mains so we wont get errors
foreach ObjFile ( main csu )
  rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.x >& /dev/null
  rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.d >& /dev/null
  rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.o >& /dev/null
  rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.g >& /dev/null
end

if( ("$Input" == "clean") || \
    ("$Input" == "ide" ) ) then
  foreach ObjFile ( bridge.o heart.o xbow.o saio.o sflash.o arcsio.o config.o memory.o dallas.o diag_enet.o )
    if( -e $WORKAREA/stand/arcs/lib/libsk/$ObjDir/$ObjFile ) then
      rm $WORKAREA/stand/arcs/lib/libsk/$ObjDir/$ObjFile >& /dev/null
    endif
  end

  goto EndScript
endif


# Change directory to highest level.
#
cd $WORKAREA/stand/arcs


if( ("$Input" == "clobber") ) then
  make clobber

  goto EndScript
endif

echo "Extra VCDEFS flags ? " ; setenv Input $<
setenv MakeInput $Input

# Set the DuartFlag to MFG_IO6 to use the IO6 duart
# Otherwise set to "" for IOC3 base duart.
setenv DuartFlag -DMFG_IO6

# Make everything with MFG_DBGPROM
#
make "COMPILATION_MODEL_SAOPT=IPA" "VCDEFS=-DMFG_DBGPROM -DVERBOSE $DuartFlag $MakeInput" $MakeProm

# Need to remove some object files, and compile with DEBUG flag
#
foreach ObjFile ( bridge.o heart.o xbow.o saio.o sflash.o arcsio.o config.o memory.o dallas.o diag_enet.o )
  if( -e $WORKAREA/stand/arcs/lib/libsk/$ObjDir/$ObjFile ) then
    rm $WORKAREA/stand/arcs/lib/libsk/$ObjDir/$ObjFile >& /dev/null
  endif
end

# Make selected files with MFG_DBGPROM and DEBUG
#
make "COMPILATION_MODEL_SAOPT=IPA" "VCDEFS=-DMFG_DBGPROM -DDEBUG -DVERBOSE $DuartFlag $MakeInput" $MakeProm


if( -e $WORKAREA/stand/arcs/IP30prom/MfgDBGProm.sh ) then
#
# Shell files inherit environment variables.
#
  cd $WORKAREA/stand/arcs/IP30prom
  make clobber

  $WORKAREA/stand/arcs/IP30prom/MfgDBGProm.sh
endif

if( -e $WORKAREA/stand/arcs/IP30prom/RACER.O/prom ) then
  cp $WORKAREA/stand/arcs/IP30prom/RACER.O/prom $WORKAREA/stand/arcs/IP30prom/MfgDBGProm
endif

EndScript:

if( "$Input" == "ide" ) then

# Need to build cleanly for ide

  cd $WORKAREA/stand/arcs/lib/libsk
  make mfgstuff

  cd  $WORKAREA/stand/arcs/ide/IP30/pon
  make mfgstuff
endif

