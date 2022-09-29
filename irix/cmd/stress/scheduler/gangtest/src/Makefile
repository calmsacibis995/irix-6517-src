SHELL = /usr/bin/sh
CINCS	= -I../include
SHDLIBC	=
LDLIBS	= -lm -lmpc 
CFLAGS	= $(CINCS) -g 
TARGET_DIR = ../bin
TARGETS	= gtest driver ginput 

default:     $(TARGETS) 

gtest:		knrTest.C knrGangMems.C knrTestSystem.C knrTimer.C knrGang.C
	CC $(CFLAGS) -o gtest knrTest.C knrGang.C knrGangMems.C knrTestSystem.C knrTimer.C $(LDLIBS)

driver: 	driver.C 
	CC $(CFLAGS) -o driver driver.C

ginput:		ginput.C knrTestSystem.C knrTimer.C
	CC $(CFLAGS) -o ginput ginput.C knrTestSystem.C knrTimer.C

install:
	if (test -d ../bin) then (test) else (mkdir $(TARGET_DIR)) fi
	mv $(TARGETS) $(TARGET_DIR)         
	
clean:
	rm *.o
	rm $(TARGET_DIR)/*


