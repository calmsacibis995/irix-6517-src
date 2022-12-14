# ************* Makefile for System Controller Firmware ***********
#
#  **********************************************

# ifdef defines:
#	SCLR_FIX:	Interrupts are disabled when the system controller 
#				issues an SCLR.  The problem is the system controller
#				detect an SCLR interrupt when it is generated by the system
#				controller, not just when it is monitored from the back
#				plane.
#

SRC=..\src
H=..\hdr\ 

#xlink -c68hc11 -xsm -Fdebug -f linklst
#xlink -c68hc11 -xsm -Fdebug -f linklst
#C_OPT= -P -g -mb -s -r -e -C -DBRD_TRN -DLAB  
C_OPT= -P -g -mb -s -e -C -DCLK_CHK -DLAB  

ASM_OPT= X S 

OBJS=cstartup.obj main.obj exec.obj executil.obj execsubs.obj irq.obj util.obj dsp_proc.obj display.obj exec_asm.obj l09.obj sys_mon.obj sequence.obj async.obj cpuproc.obj

syscntl.a07:    $(OBJS) makefile
	xlink -c68hc11 -xsm -Fmotorola -f linklst -o 2_03L.a07

cstartup.obj:   $(SRC)\cstartup.s07  makefile linklst.xcl
	a6801 $(SRC)\cstartup.s07,, cstartup.obj $(ASM_OPT)

main.obj:   $(SRC)\main.c $(H)\proto.h  makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -o main.obj $(SRC)\main.c

exec.obj:   $(SRC)\exec.c $(H)\exec.h $(H)\proto.h  makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -o exec.obj $(SRC)\exec.c

executil.obj:   $(SRC)\executil.c $(H)\exec.h $(H)\proto.h  makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -o executil.obj $(SRC)\executil.c

execsubs.obj:   $(SRC)\execsubs.c $(H)\exec.h $(H)\proto.h  makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -o execsubs.obj $(SRC)\execsubs.c

irq.obj:   $(SRC)\irq.c $(H)\proto.h $(H)\sp.h  makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o irq.obj $(SRC)\irq.c

util.obj:   $(SRC)\util.c $(H)\proto.h  makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o util.obj $(SRC)\util.c

dsp_proc.obj:   $(SRC)\dsp_proc.c $(H)\*.h  makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o dsp_proc.obj $(SRC)\dsp_proc.c

display.obj:   $(SRC)\display.c $(H)\proto.h $(H)\sys_hw.h makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o display.obj $(SRC)\display.c

exec_asm.obj:   $(SRC)\exec_asm.s07  makefile linklst.xcl
	a6801 $(SRC)\exec_asm.s07,, exec_asm.obj $(ASM_OPT)

l09.obj:   $(SRC)\l09.s07  makefile linklst.xcl
	a6801 $(SRC)\l09.s07,, l09.obj $(ASM_OPT)

sys_mon.obj:   $(SRC)\sys_mon.c $(H)\proto.h  $(H)\sp.h makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o sys_mon.obj $(SRC)\sys_mon.c

sequence.obj:   $(SRC)\sequence.c $(H)\proto.h $(H)\sys_hw.h $(H)\sp.h makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o sequence.obj $(SRC)\sequence.c

async.obj:   $(SRC)\async.c $(H)\proto.h $(H)\async.h makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o async.obj $(SRC)\async.c

cpuproc.obj:   $(SRC)\cpuproc.c $(H)\proto.h $(H)\async.h $(H)\exec.h $(H)\sp.h makefile linklst.xcl
	    c-6811 -I$(H) $(C_OPT) -RRCODE -o cpuproc.obj $(SRC)\cpuproc.c
