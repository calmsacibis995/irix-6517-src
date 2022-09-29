all: r4000-earom.img earom-100.img earom-150.img earom-100-4M.img earom-150-4M.img earom-200.1M.dat

earom-100.img : earom-100.dat mkimg
	./mkimg -r -i earom-100.dat -o earom-100.img

earom-150.img : earom-150.dat mkimg
	./mkimg -r -i earom-150.dat -o earom-150.img

earom-200.img : earom-200.dat mkimg
	./mkimg -r -i earom-200.dat -o earom-200.img

earom-100-4M.img : earom-100-4M.dat mkimg
	./mkimg -r -i earom-100-4M.dat -o earom-100-4M.img

earom-150-4M.img: earom-150-4M.dat mkimg
	./mkimg -r -i earom-150-4M.dat -o earom-150-4M.img

earom-200-4M.img : earom-200-4M.dat mkimg
	./mkimg -r -i earom-200-4M.dat -o earom-200-4M.img

r4000-earom.img : r4000-earom.dat mkimg
	./mkimg -r -i r4000-earom.dat -o r4000-earom.img

r4400-earom.img : earom.dat mkimg
	./mkimg -r -i earom.dat -o r4400-earom.img

mkimg: mkimg.c
	cc -g -o $@ mkimg.c
