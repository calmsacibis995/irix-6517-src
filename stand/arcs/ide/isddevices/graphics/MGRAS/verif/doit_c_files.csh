#!/bin/csh

setenv SRCDIR /hosts/large/d2/impact/gfx/MGRAS/verif/TE
setenv HOMEDIR /d2/trees/lotus/work/stand/arcs/ide/IP22/graphics/MGRAS/verif
setenv CSIMDIR /d2/trees/future/work/gfx/MGRAS/diags/RE/color_z/opt1


cp $SRCDIR/geometry/mtest/opt43/*.cmou ./opt43
goto done

cp $SRCDIR/geometry/mtest/opt110/*.cmou ./debug_8Khigh
cp $SRCDIR/geometry/mtest/opt111/*.cmou ./debug_8Kwide
cp $SRCDIR/geometry/mtest/opt127/*.cmou ./debug_detail
cp $SRCDIR/geometry/mtest/opt130/*.cmou ./debug_mag
cp $SRCDIR/geometry/mtest/opt2/*.cmou ./persp1
cp $SRCDIR/geometry/mtest/opt330/*.cmou ./line_gl_sc
cp $SRCDIR/geometry/mtest/opt350/*.cmou ./scis_tri_i
cp $SRCDIR/load/mtest/opt8/*.cmou ./load
cp $SRCDIR/lut4d/mtest/opt100/*.cmou ./lut4d
cp $SRCDIR/mddma/mtest/opt1/*.cmou ./mddma
cp $SRCDIR/tex1d/mtest/opt1/*.cmou ./tex1d
cp $SRCDIR/tex3d/mtest/opt1/*.cmou ./tex3d

copy_csim:

cp $SRCDIR/geometry/mtest/opt43/csim_in.bin ./opt43
cp $SRCDIR/geometry/mtest/opt110/csim_in.bin ./debug_8Khigh
cp $SRCDIR/geometry/mtest/opt111/csim_in.bin ./debug_8Kwide
cp $SRCDIR/geometry/mtest/opt127/csim_in.bin ./debug_detail
cp $SRCDIR/geometry/mtest/opt130/csim_in.bin ./debug_mag
cp $SRCDIR/geometry/mtest/opt2/csim_in.bin ./persp1
cp $SRCDIR/geometry/mtest/opt330/csim_in.bin ./line_gl_sc
cp $SRCDIR/geometry/mtest/opt350/csim_in.bin ./scis_tri_i
cp $SRCDIR/load/mtest/opt8/csim_in.bin ./load
cp $SRCDIR/lut4d/mtest/opt100/csim_in.bin ./lut4d
cp $SRCDIR/mddma/mtest/opt1/csim_in.bin ./mddma
cp $SRCDIR/tex1d/mtest/opt1/csim_in.bin ./tex1d
cp $SRCDIR/tex3d/mtest/opt1/csim_in.bin ./tex3d

spec:
cp $SRCDIR/geometry/mtest/opt43/spec ./opt43
cp $SRCDIR/geometry/mtest/opt110/spec ./debug_8Khigh
cp $SRCDIR/geometry/mtest/opt111/spec ./debug_8Kwide
cp $SRCDIR/geometry/mtest/opt127/spec ./debug_detail
cp $SRCDIR/geometry/mtest/opt130/spec ./debug_mag
cp $SRCDIR/geometry/mtest/opt2/spec ./persp1
cp $SRCDIR/geometry/mtest/opt330/spec ./line_gl_sc
cp $SRCDIR/geometry/mtest/opt350/spec ./scis_tri_i
cp $SRCDIR/load/mtest/opt8/spec ./load
cp $SRCDIR/lut4d/mtest/opt100/spec ./lut4d
cp $SRCDIR/mddma/mtest/opt1/spec ./mddma
cp $SRCDIR/tex1d/mtest/opt1/spec ./tex1d
cp $SRCDIR/tex3d/mtest/opt1/spec ./tex3d

csim:
foreach dir (debug_8Khigh debug_8Kwide debug_detail debug_mag line_gl_sc load lut4d mddma persp1 scis_tri_i tex1d tex3d opt43)
	cp ./$dir/csim_in.bin $CSIMDIR/csim_in.bin
#	cd $CSIMDIR
#	make view
	echo "$dir - cd to csimdir and make view, hit CR when done: "
	set answ = $<	
	cp $CSIMDIR/csim_in.txt $HOMEDIR/$dir/csim_in.txt
	cd $HOMEDIR/$dir
	sum $HOMEDIR/$dir/csim_in.txt
	txt2ide -f csim_in.txt -o tom
	cd ..
end	

done:
