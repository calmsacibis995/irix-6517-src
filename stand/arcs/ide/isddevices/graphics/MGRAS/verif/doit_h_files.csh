#!/bin/csh

setenv SRCDIR /hosts/large/d2/impact/gfx/MGRAS/verif/TE
setenv HOMEDIR /d2/trees/lotus/work/stand/arcs/ide/IP22/graphics/MGRAS/verif
setenv CSIMDIR /d2/trees/future/work/gfx/MGRAS/diags/RE/color_z/opt1

cp $SRCDIR/geometry/mtest/opt43/*.cmou ./opt43
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

goto cmou
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

cmou:
foreach dir (debug_8Khigh debug_8Kwide debug_detail debug_mag line_gl_sc load lut4d mddma persp1 scis_tri_i tex1d tex3d opt43 )
#foreach dir (debug_detail opt43)

	cd $dir

	switch ($dir)
		case debug_8Khigh:	
			set xval = 196; set yval = 221; set hval = 291; set wval = 98
			setenv name tex_bordertall_out
			breaksw
		case debug_8Kwide:
			set xval = 220; set yval = 196; set hval = 98; set wval = 292
			setenv name tex_borderwide_out
			breaksw
		case debug_detail:
			set xval = 38; set yval = 39; set hval = 284; set wval = 436
			setenv name tex_detail_out
			breaksw
		case debug_mag:
			set xval = 144; set yval = 208; set hval = 159; set wval = 224
			setenv name tex_mag_out
			breaksw
		case line_gl_sc:
			set xval = 220; set yval = 220; set hval = 72; set wval = 72 
			setenv name tex_linegl_out
			breaksw
		case load:
			set xval = 196; set yval = 207; set hval = 128; set wval = 98
			setenv name tex_load_out
			breaksw
		case lut4d:
			set xval = 0; set yval = 0; set hval = 256; set wval = 256 
			setenv name tex_lut4d_out
			breaksw
		case mddma:
			set xval = 0; set yval = 0; set hval = 256; set wval = 256 
			setenv name tex_mddma_out
			breaksw
		case persp1:
			set xval = 194; set yval = 194; set hval = 98; set wval = 124 
			setenv name tex_persp_out
			breaksw
		case scis_tri_i:
			set xval = 195; set yval = 195; set hval = 100; set wval = 100 
			setenv name tex_scistri_out
			breaksw
		case tex1d:
			set xval = 94; set yval = 65; set hval = 482; set wval = 426 
			setenv name tex_1d_out
			breaksw
		case tex3d:
			set xval = 206; set yval = 206; set hval = 100; set wval = 100 
			setenv name tex_3d_out
			breaksw
		case opt43:
			set xval = 0; set yval = 0; set hval = 40; set wval = 40			setenv name tex_poly
			breaksw
	endsw

# run aviewit
	banner $name
	aviewit -x $xval -y $yval -h $hval -w $wval -z -o

# modify the out.h file
	sed 's/out_array/'$name'_array/g' out.h >! out.new.h
	echo "};\n" >> out.new.h

# copy out.new.h to the right place
	cp out.new.h /d3/trees/lotus/work/stand/arcs/ide/IP22/graphics/MGRAS/mgras_$name.h

# return 
	cd ..
end
done:
echo DONE
