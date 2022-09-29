#!/bin/csh

if( "${?MakeProm}" == "0" ) then
setenv Input ""
else 
setenv Input $MakeProm
endif

if( "${?MakeInput}" == "0" ) then
setenv MakeInput ""
endif

if( "${?DuartFlag}" == "0" ) then
setenv DuartFlag ""
endif


if( "$Input" == "IP30proms" ) then
  goto CheckFiles
endif

BuildInput:
if( ("$Input" != "dprom") &&     \
    ("$Input" != "prom") &&      \
    ("$Input" != "fprom.bin") && \
    ("$Input" != "clean") ) then
  echo "Build [dprom, prom, fprom.bin, clean] ? " ; setenv Input $<
  goto BuildInput
endif



setenv ObjDir RACER.O

if( ("$Input" == "clean") ) then
# Remove all mains so we wont get errors
  foreach ObjFile ( main csu )
    rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.x >& /dev/null
    rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.d >& /dev/null
    rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.o >& /dev/null
    rm $WORKAREA/stand/arcs/IP30prom/$ObjDir/${ObjFile}.g >& /dev/null
  end

  goto EndScript
endif



setenv MakeProm $Input

CheckFiles:
cd $WORKAREA/stand/arcs/IP30prom

# Check the existance of the files.
if( !(-e duart_regs.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/ioc3/duart_regs.c          duart_regs.c
endif

if( !(-e pci_sio.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/pci/pci_sio.c              pci_sio.c
endif

if( !(-e utility.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/util/utility.c             utility.c
endif

if( !(-e enet_diags.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/enet/enet_diags.c          enet_diags.c
endif

if( !(-e enet_dma.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/enet/enet_dma.c            enet_dma.c
endif

if( !(-e ide_malloc.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/common/ide_malloc.c                 ide_malloc.c
endif

if( !(-e RadCspaceTest.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/rad/RadCspaceTest.c        RadCspaceTest.c
endif

if( !(-e RadRamTest.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/rad/RadRamTest.c           RadRamTest.c
endif

if( !(-e RadStatusTest.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/rad/RadStatusTest.c        RadStatusTest.c
endif

if( !(-e RadUtils.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/rad/RadUtils.c             RadUtils.c
endif

if( !(-e scsi_regs.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/scsi/scsi_regs.c           scsi_regs.c
endif

if( !(-e gz_errorcodes.c) ) then
  ln -s $WORKAREA/stand/arcs/ide/godzilla/util/gz_errorcodes.c       gz_errorcodes.c
endif



# Remove files so that the make flags can take place.
#
rm $ObjDir/main.x >& /dev/null
rm $ObjDir/main.d >& /dev/null
rm $ObjDir/main.o >& /dev/null
rm $ObjDir/main.g >& /dev/null




if( "$MakeProm" == "IP30proms" ) then
  make -f MfgDBGProm.mk "COMPILATION_MODEL_SAOPT=IPA" "VCDEFS=-DMFG_DBGPROM -DMFG_MAKE -DVERBOSE $DuartFlag $MakeInput -I$WORKAREA/stand/arcs/ide/godzilla/include" fprom.bin
  make -f MfgDBGProm.mk "COMPILATION_MODEL_SAOPT=IPA" "VCDEFS=-DMFG_DBGPROM -DMFG_MAKE -DVERBOSE $DuartFlag $MakeInput -I$WORKAREA/stand/arcs/ide/godzilla/include" prom 
  make -f MfgDBGProm.mk "COMPILATION_MODEL_SAOPT=IPA" "VCDEFS=-DMFG_DBGPROM -DMFG_MAKE -DVERBOSE $DuartFlag $MakeInput -I$WORKAREA/stand/arcs/ide/godzilla/include" dprom 
else
  make -f MfgDBGProm.mk "COMPILATION_MODEL_SAOPT=IPA" "VCDEFS=-DMFG_DBGPROM -DMFG_MAKE -DVERBOSE $DuartFlag $MakeInput -I$WORKAREA/stand/arcs/ide/godzilla/include" $MakeProm
endif

EndScript:

