all: earom.img 

earom.img : mkimg earom.dat
	mkimg -i earom.dat -o earom.img

mkimg: mkimg.c
	cc -g -non_shared -o $@ mkimg.c
