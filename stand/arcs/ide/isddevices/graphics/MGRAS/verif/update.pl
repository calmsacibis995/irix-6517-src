#!/usr/local/bin/perl

$SRCDIR="/hosts/large/d2/impact/gfx/MGRAS/verif/TE";
$HOMEDIR="/d2/trees/lotus/work/stand/arcs/ide/IP22/graphics/MGRAS/verif";
$CSIMDIR="/d2/trees/future/work/gfx/MGRAS/diags/RE/color_z/opt1";

%VerifTests = ('geometry' => ['opt43','opt110','opt111','opt127','opt130','opt2','opt330','opt350'],
	       'load'     => ['opt8'],
	       'lut4d'    => ['opt100'],
	       'mddma'    => ['opt1'],
	       'tex1d'    => ['opt1'],
	       'tex3d'    => ['opt1']);


%TestList = ('geometry_opt110' => 'debug_8Khigh',
	     'geometry_opt111' => 'debug_8Kwide',
	     'geometry_opt127' => 'debug_detail',
	     'geometry_opt130' => 'debug_mag',
	     'geometry_opt330' => 'line_gl_sc',
	     'load_opt8' => 'load',
	     'lut4d_opt100' => 'lut4d',
	     'mddma_opt1' => 'mddma',
	     'geometry_opt43' => 'opt43',
	     'geometry_opt2' => 'persp1',
	     'geometry_opt350' => 'scis_tri_i',
	     'tex1d_opt1' => 'tex1d',
	     'tex3d_opt1' => 'tex3d');

for $testdir (keys(%VerifTests)) {
  for $test (@{$VerifTests{$testdir}}) {
    $name = $testdir . "_" . $test;
    $diff = 0;
    for ($pp=0; $pp < 2; $pp++) {
      for ($rd=0; $rd < 3; $rd++ ) {
	$cmd="diff $SRCDIR/$testdir/mtest/$test/PP${pp}_RD${rd}.cmou ./$TestList{$name} > /dev/null";
	$ret = system($cmd);
	$diff |= $ret;
      }
    }
    if ($diff) {
      $cmd="cp $SRCDIR/$testdir/mtest/$test/*.cmou ./$TestList{$name}";
      print "$cmd\n";
      system($cmd);
      &NewHeader($TestList{$name});
    }

    $cmd="diff $SRCDIR/$testdir/mtest/$test/csim_in.bin ./$TestList{$name} > /dev/null";
    $diff = system($cmd);
    if ($diff) {
      $cmd="cp $SRCDIR/$testdir/mtest/$test/csim_in.bin ./$TestList{$name}";
      print "$cmd\n";
      system($cmd);
      $cmd="cp $SRCDIR/$testdir/mtest/$test/csim_in.txt ./$TestList{$name}";
      print "$cmd\n";
      system($cmd);
#      system("sum $TestList{$name}/csim_in.txt");
#      system("txt2ide -f $TestList{$name}/csim_in.txt -o $TestList{$name}/tom");
    }

    $cmd="diff $SRCDIR/$testdir/mtest/$test/spec ./$TestList{$name} > /dev/null";
    $diff = system($cmd);
    if ($diff) {
      $cmd="cp $SRCDIR/$testdir/mtest/$test/spec ./$TestList{$name}";
      print "$cmd\n";
      system($cmd);
    }

  }
}

sub NewHeader {
  local($key)=@_;
  my %RunParams = ('debug_8Khigh' => [196, 221, 291, 98, "tex_bordertall_out"],
		   'debug_8Khigh' => [196, 221, 291, 98, "tex_bordertall_out"],
		   'debug_8Kwide' => [220, 196, 98, 292, "tex_borderwide_out"],
		   'debug_detail' => [38, 39, 284, 436, "tex_detail_out"],
		   'debug_mag' => [144, 208, 159, 224, "tex_mag_out"],
		   'line_gl_sc' => [220, 220, 72, 72, "tex_linegl_out"],
		   'load' => [196, 207, 128, 98, "tex_load_out"],
		   'lut4d' => [0, 0, 256, 256, "tex_lut4d_out"],
		   'mddma' => [0, 0, 256, 256, "tex_mddma_out"],
		   'persp1' => [194, 194, 98, 124, "tex_persp_out"],
		   'scis_tri_i' => [195, 195, 100, 100, "tex_scistri_out"],
		   'tex1d' => [94, 65, 482, 426, "tex_1d_out"],
		   'tex3d' => [206, 206, 100, 100, "tex_3d_out"],
		   'opt43' => [236, 236, 40, 40, "tex_poly"]);

  my $cmd, $dir;

#  for $key (keys(%RunParams)) {
    chdir $key;
    chomp($dir = `pwd`); print "$dir\n";
    $cmd="banner $RunParams{$key}[4]";
    print "$cmd\n";
    system($cmd);
    
    $cmd="../viewer/aviewit -x $RunParams{$key}[0] -y $RunParams{$key}[1] -h $RunParams{$key}[2] -w $RunParams{$key}[3] -z -o";
    print "$cmd\n";
    system($cmd);
    
    open(FILEIN,"<out.h");
    open(FILEOUT,">out.h.new");
    while(<FILEIN>) {
      if(/out_array/) {
	s/out/$RunParams{$key}[4]/;
      }
      print FILEOUT;
    }
    close(FILEIN);
    close(FILEOUT);
    
    $cmd="cp out.h.new /d3/trees/lotus/work/stand/arcs/ide/IP22/graphics/MGRAS/mgras_$RunParams{$key}[4].h";
    print "$cmd\n";
    system($cmd);
    
    chdir "..";
#  }

}

__END__
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
