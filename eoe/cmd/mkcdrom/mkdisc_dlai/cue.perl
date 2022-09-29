#!/usr/bin/perl
$track=1;
$prev_track="";
$time=0;
$cue_n=0; # cuesheet counter
$verbose=0;
# we generate an array for the cuesheet and a data file
# @cue_ca = cntl/address
# @cue_tno = track #
# @cue_index = index #
# @cue_dform = data form
# @cue_time = frame time
$,=" ";
$argc = @ARGV;
$i=0;
while ($i < $argc) {
 if ( @ARGV[$i] eq "-v" ) {
    $verbose = 1;
    @ARGV[$i]="/dev/null";
 }
 if ( @ARGV[$i] eq "-c" ) {
    @ARGV[$i]="/dev/null";
    $i = $i + 1;
    $cuesheet_file = @ARGV[$i];
    @ARGV[$i]="/dev/null";
    }
 if ( @ARGV[$i] eq "-f" ) {
    @ARGV[$i]="/dev/null";
    $i = $i + 1;
    $image_file = @ARGV[$i];
    @ARGV[$i]="/dev/null";
    system("rm -f $image_file");
    }
 $i = $i + 1;
}

while(<>) {
	chop;
	if (/^#/) {
		next;	# ignore all lines beginning with '#'
		}
	@words=split;
	$len = @words;
	if ( $len==0 )
		{
		next;	# empty line
		}
	@fields=split(/:/);
	$nf = @fields;
	$copy_bit=0;
	$pre_emphasis=0;
	$isrc="";
	$gap=0;
	$head=0;
	$tail=0;
	if ( @fields[0] eq "upc" && $nf==2 )
	 {
	 $upccode = @fields[1];
	 if ( $upccode !~ /^[0-9]{13}$/ )
	  {
	  die ("upccode must be 13 digits [",$upccode,"]");
	  }
	 next;
	 }
	if ( @fields[0] eq "verbose" && $nf==2 )
	 {
	 $verbose=@fields[1];
	 next;
	 }
	if ( @fields[0] eq "image" && $nf==2 )
	 {
	 $image_file=@fields[1];
	 system("rm -f $image_file");
	 next;
	 }
	if ( @fields[0] eq "cuesheet" && $nf==2 )
	 {
	 $cuesheet_file = @fields[1];
	 next;
	 }
	if (@fields[0] eq "data" && $nf>1)
	 {
	 $file=@fields[1];
	 ($blocks,$pad)=&statfile($file,2048);
	 if ($nf > 2) {
		$attr=@fields[2];
		($copy_bit, $pre_emphasis) = &decode_attr($attr);
		}
	 if ($pre_emphasis)
	  {
	  warn "pre-emphasis ignored for data track #",$track;
	  }
	 if ($nf > 3) {
		warn("extra fields ignored for data track, track #",$track);
		}
	 $time = &do_data_track($track, $file, $blocks, $pad, $copy_bit, $time);
	 $track++;
	 $prev_track="D";
	 next;
	 }
	if (@fields[0] eq "audio" && $nf>1)
	 {
	 $file=@fields[1];
	 ($blocks,$pad)=&statfile($file,2352);
	 if ($nf > 2) {
		$attr=@fields[2];
		($copy_bit, $pre_emphasis) = &decode_attr($attr);
		}
	 if ($nf > 3) {
		$isrc=@fields[3];
		if ( $isrc ne "" ) {
	 	if ($isrc !~ /^.{5}[0-9]{7}$/)
		 {
		 die ("isrc must have XXXXX9999999 format [".$isrc."]");
		 }
	 	@isrc[$track]=$isrc;
		}
		}
	 if ($nf > 4) {
		$gap=&decode_time(@fields[4]);
		}
	 if ($nf > 5) {
		$head=&decode_time(@fields[5]);
		}
	 if ($nf > 6) {
		$tail=&decode_time(@fields[6]);
		}
	 $time = &do_audio_track($track, $file, $blocks, $pad, $copy_bit, $pre_emphasis, $time,
		$gap, $head, $tail);
	 $track++;
	 $prev_track="A";
	 next;
	 }
	warn("Unknown input at line #", $.);
	next;
	} # foreach input line
 # end of input, generate lead_out and write cue sheet
$time = &lead_out($time);
 if ($verbose) {
  &print_msf("Lead out:",$time);
  print "\n";
  }
# write the cuesheet if user requests it
if ($cuesheet_file) {
open(CUE, ">$cuesheet_file");
for ($i = 0; $i < $cue_n; $i++)
 {
 ($m, $s, $f)=&f_to_msf( @cue_time[$i] );
 printf(CUE  "%c%c%c%c%c%c%c%c", @cue_ca[$i], @cue_tno[$i]==0xaa?0xaa:((int(@cue_tno[$i]/10)<<4) | ( @cue_tno[$i] % 10)),
        @cue_index[$i], @cue_dform[$i], 0, $m, $s, $f);
 }
# write upc if necessary
if ($upccode)
 {
 @upc_chars=split(//,$upccode);
 $u1=@upc_chars[0]<<4 + @upc_chars[1];
 $u2=@upc_chars[2]<<4 + @upc_chars[3];
 $u3=@upc_chars[4]<<4 + @upc_chars[5];
 $u4=@upc_chars[6]<<4 + @upc_chars[7];
 $u5=@upc_chars[8]<<4 + @upc_chars[9];
 $u6=@upc_chars[10]<<4 + @upc_chars[11];
 $u7=@upc_chars[12]<<4;
 printf(CUE "%c%c%c%c%c%c%c%c", 2, $u1, $u2, $u3, $u4, $u5, $u6, $u7);
 }
# write any isrc
for ($i = 1; $i < $track; $i++)
 {
 if (@isrc[$i])
  {
  @isrc_chars=split(//,@isrc[$i]);
  printf(CUE "%c%c%c%c%c%c%c%c", 3, (int($i/10)<<4) | ( $i % 10), 0, @isrc_chars[0..4]);
 $u1=@isrc_chars[5]<<4 + @isrc_chars[6];
 $u2=@isrc_chars[7]<<4 + @isrc_chars[8];
 $u3=@isrc_chars[9]<<4 + @isrc_chars[10];
 $u4=@isrc_chars[11]<<4;
  printf(CUE "%c%c%c%c%c%c%c%c", 3, (int($i/10)<<4) | ( $i % 10), 0, 0, $u1,
    $u2, $u3, $u4);
  }
 } 
close(CUE);
}
# end of main program

# functions used
 
sub statfile { # return the number of blocks of size $div a file is, and any pad
 local ($name, $div) = @_;
 local ($num_blks, $pad);
 local ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime,
   $ctime, $blksize, $blocks);
 if ( ! -r $name ) {
	die "Cant read ", $file;
	}
 if ( ! -f $name ) {
	die $name, " is not a file\n";
	}
 ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime,
   $ctime, $blksize, $blocks) = stat($name);
 if ( $size % $div != 0 ) {
	warn ($name, " is not a multiple of ", $div, " bytes - will be padded");
	$num_blks = int($size / $div) + 1;
	$pad = $div - ($size % $div);
	}
 else
	{
	$num_blks = int($size / $div);
	$pad = 0;
	}
 return ($num_blks, $pad);
 }

sub decode_attr { # decode the attribute field into copy_bit and pre_emph
 local ($attr) = @_;
 local ($copy_bit, $pre_emph) = (0,0);
 if ( $attr =~ /c/ ) { $copy_bit = 1; }
 if ( $attr =~ /p/ ) { $pre_emph  =1; }
 return ($copy_bit, $pre_emph);
 }

sub decode_time { # decode time "m.s.f", "s.f", or "s" into #frames
 local ($arg) = @_;
 local (@msf) = split(/\./, $arg);
 local ($num_fields) = @msf;
 if ($num_fields == 1)
	{
	return (@msf[0] * 75);
	}
 elsif ($num_fields == 2)
	{
	return (@msf[0] * 75 + @msf[1]);
	}
 else 
	{
	return (@msf[0] *60*75 + @msf[1]* 75 + @msf[2]);
	}
 }

sub f_to_msf { # return (m,s,f) from the frame #, msf in BCD
 local ($m, $s, $f);
 local ($frame) = @_;
 $m = int ( $frame / (60*75));
 $frame -= $m *(60*75);
 $s = int ( $frame / 75 );
 $f = $frame % 75;
 $m = (int($m/10)<<4) | ( $m % 10);
 $s = (int($s/10)<<4) | ( $s % 10);
 $f = (int($f/10)<<4) | ( $f % 10);
 return($m, $s, $f);
 }


sub f_to_msf_decimal { # return (m,s,f) from the frame #, msf in decimal
 local ($m, $s, $f);
 local ($frame) = @_;
 $m = int ( $frame / (60*75));
 $frame -= $m *(60*75);
 $s = int ( $frame / 75 );
 $f = $frame % 75;
 return($m, $s, $f);
 }



sub do_data_track { # drop down a data track
 local ($track, $file, $blocks, $pad, $copy_bit, $time) = @_;
 local ($start_time);
 # if the track is first, we initialize the lead-in and gap
  if ($track == 1) {
   @cue_ca[$cue_n]=	0x40 + ($copy_bit<<5) + 1;	# lead-in
   $cue_n++;
   @cue_ca[$cue_n]=	@cue_ca[$cue_n-1];	# gap
   @cue_tno[$cue_n]=	1;
   @cue_dform[$cue_n]=	0x10;
   $cue_n++;
   $time = $time + 2*75;	# 2 second gap
   }
 # if the track is not the first, we die, no support yet for data track != 1
  if ($track != 1) {
    die "No support yet for data track != 1\n";
    }
 # now place the data
 @cue_ca[$cue_n]=	0x40 + ($copy_bit<<5) + 1;    # data track
 @cue_tno[$cue_n]=	$track;
 @cue_index[$cue_n]=	1;
 @cue_dform[$cue_n]=	0x11;
 @cue_time[$cue_n]=	$time;
 $cue_n++;
 # copy the file to the output if user specified an output image
 if ( $image_file ) {
 $exitcode = system("cat $file >> $image_file") >> 8;
 if ($exitcode) { die "Error writing track #",$track,"\n"; }
 # do we need to pad?
 if ($pad > 0) {
  open(OUTPUT, ">>$image_file");
  print OUTPUT "\0"x$pad;
  close OUTPUT;
  }
 }
 # the time is incremented
 $start_time = $time;
 $time += $blocks;
 if ($verbose) {
  print "data track", $track, $file;
  &print_msf("size:",$blocks+$pad);
  &print_msf("start:",$start_time); 
  &print_msf("end:",$time);
  print "\n";
  }
 return $time;
 }


sub do_audio_track { # lay down an audio track
local ($track, $file, $blocks, $pad, $copy_bit, $pre_emphasis, $time,
		$gap, $head, $tail) = @_;
local ($size_to_copy);
local ($start_time);
 # if the track is first, we initialize the lead-in and gap
  if ($track == 1) {
   @cue_ca[$cue_n]=	($pre_emphasis<<4)+($copy_bit<<5)+1;	# lead-in
   $cue_n++;
   @cue_ca[$cue_n]=	@cue_ca[$cue_n-1];	# gap
   @cue_tno[$cue_n]=	1;
   @cue_dform[$cue_n]=	0x00;
   $cue_n++;
   $time = $time + 2*75 ;	# 2 second gap
   }
 # ? ? ? ?
 # ? ? ? ? if we are following a data track, add 2 seconds to data track
 # ? ? ? ?
 $time = $time + $gap;	# add the gap
 # now place the audio
 @cue_ca[$cue_n]=	($pre_emphasis<<4)+($copy_bit<<5)+1;	# audio track
 @cue_tno[$cue_n]=	$track;
 @cue_index[$cue_n]=	1;
 @cue_dform[$cue_n]=	0x01;
 @cue_time[$cue_n]=	$time;
 $cue_n++;
 # copy the file to the output if user specified an output image
 # crop if necessary
 if ( $image_file ) {
 $size_to_copy=$blocks-$head-$tail;
 $exitcode = system("dd if=$file ibs=2352 obs=2352 skip=$head count=$size_to_copy >> $image_file") >> 8;
 if ($exitcode) { die "Error writing track #",$track,"\n"; }
 # do we need to pad?
 if ($pad > 0 && $tail == 0 ) {
  open(OUTPUT, ">>$image_file");
  print OUTPUT "\0"x$pad;
  close OUTPUT;
  }
 }
 # the time is incremented
 $start_time = $time;
 $time += $blocks-$head-$tail;
 if ($verbose) {
  print "audio track", $track, $file;
  &print_msf("size:",$blocks-$head-$tail);
  &print_msf("start:",$start_time);
  &print_msf("end:",$time);
  print "\n";
  }
 return $time;
 }

sub lead_out { # generate lead out
 local($time) = @_;
 @cue_ca[$cue_n]=	@cue_ca[$cue_n-1];	# lead_out ca same as last trk
 @cue_tno[$cue_n]=	0xaa;
 @cue_index[$cue_n]=	1;
 @cue_time[$cue_n]=	$time;
 $cue_n++;
 return $time;
 }

sub print_msf { # print the m.s.f
 local ($m, $s, $f);
 local($title, $time) = @_;
 ($m, $s, $f) = &f_to_msf_decimal($time);
 printf( " %s %d.%d.%d", $title, $m, $s, $f);
 }
