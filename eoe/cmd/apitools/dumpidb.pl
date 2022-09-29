#!/usr/bin/perl -w
## #!/usr/bin/perl

## Change to test ptools

## File = dumpidb.pl
##
## Can get input from any .idb file.
##
## Optional -o paramater to specify the directory for the output files.
## If -o param is not specified, then output dir defaults to '$DefOutDir'.
##
## Optional -d paramater to specify the directory to look for .idb files.
## If -d param is not specified, then output dir defaults to '$DefInDir'.
##
## The database filename is hardcoded in '$Database'. If this file does not 
## exist when this script is run, the output file '$Outfile' is copied to 
## '$Database', and the script ends. If the database exists, it will be 
## updated with only new records, and the current run will be in dumpidb.out. 

## Defaults ###################################################################

## Default hard-coded output directory
$DefOutDir = '.';

## Default hard-coded input directory
$DefInDir = '/hosts/dist/test/kudzu/latest';

## Hard-coded database filename
$Database = 'libraries.db';

## Hard-coded excluded records filename
$Excluded = 'excluded.out';

## Hard-coded output filename
$Outfile = 'dumpidb.out';

## Hard-coded NEW public libraries filename
$NewLibsFileA = 'newlibsa.out';

## Hard-coded NEW private libraries filename
$NewLibsFileB = 'newlibsb.out';

## Hard-coded OLD database filename
$OldDatabase = 'libraries.db.old';

## Parameters #################################################################

use Getopt::Long;
## Get -o optional option string that's assigned to '$OutDir'
unless (GetOptions("o:s" => \$OutDir, "d:s" => \$InDir)) {
        warn "Problem with -o or -d parameter. Using defaults:\n",
		"\t-o -> $DefOutDir\n",
		"\t-d -> $DefInDir\n";
	$OutDir = $DefOutDir;
	$InDir = $DefInDir;

} else {
	## Check if $OutDir was specified (-o) and is writeable.
	if ($OutDir) {
		unless (-w $OutDir) {
        		warn "Warning: problem with \'$OutDir\'. Using ", 
				"default output directory:\n", "\t$DefOutDir\n";
			$OutDir = $DefOutDir;
		}
	} else {
		$OutDir = $DefOutDir;
	}

	## Check if $InDir was specified (-d) and is readable.
	if ($InDir) {
		unless (-r $InDir) {
        		warn "Warning: problem with \'$InDir\'. Using ", 
				"default input directory:\n", "\t$DefInDir\n";
			$InDir = $DefInDir;
		}
	} else {
		$InDir = $DefInDir;
	}
}
	
## begin #####################################################################

@IDBFiles = <$InDir/*.idb>;
## Make sure there is at least one .idb file to process.
if ($#IDBFiles eq -1) {
	die "Error: There are no .idb files in directory \'$InDir\'.\n";
}

$j = $k = $l_count = $f_count = $ex_count = 0;
for $i_ref (@IDBFiles) {
	unless (open(IDBfile, $i_ref)) {
		print "Error: Can't open $i_ref: $!\n";
		next;
	}

	## Need this for date_found below, and only need to get once. Keep
	## stored as long int, and not a string (using localtime() ).
	if ($i_ref eq $IDBFiles[0]) {
		@stat_info = lstat IDBfile;
		#Just gotta know the 10th element of stat is mtime.
		$date_found = $stat_info[9];
	}

	##Pull out just the fields needed.
	while (<IDBfile>) {
		($ftype, $mode, $owner, $group, $dstpath, $src, $image, 
			@junk) = split;

		## Seems to be 2 formats for .idb files. Either $image is
                ## the 6th field or the very last of an unknown number of
                ## fields. The 7th field is usually 'symval(<xxxx>)' or  
                ## 'sum(<xxxx>)'. Search for '('.
                if (grep /\(/, $image) {
                        $image = $junk[$#junk];
                }

		## Separate path and library
		@full_path = split(/\//, $dstpath);
		$name = $full_path[$#full_path];
		$libpath = $full_path[0]; 
		for ($i_path = 1; $i_path < scalar(@full_path)-1; $i_path++) {
			$libpath = join ('/', $libpath, $full_path[$i_path]);
		}
		## Include libraries, type 'f' or 'l', and ends in '.so' or '.a'
		## or '.so.[1-9]'.
		if (($name =~ /.*\.so$|.*\.so\.[1-9]$|.*\.a$/i) and 
			($ftype =~ /f|l/))
		{
                        $First[$j++] = {
                        	dstpath => $dstpath,
                                idbfile => $i_ref,
				ftype	=> $ftype,
                                srcpath => $src,
                                image   => $image,
				name	=> $name,
				libpath	=> $libpath
                        };
			if ($ftype eq 'l') {$l_count++;};
			if ($ftype eq 'f') {$f_count++;};
		} else { 
			$Excluded[$k++] = {
                                dstpath => $dstpath,
                                idbfile => $i_ref,
				ftype	=> $ftype,
                                srcpath => $src,
                                image   => $image,
				name	=> $name,
				libpath	=> $libpath
                        };
			$ex_count++;
		};
	};
	close IDBfile;
};

open TMP1FILE, ">$OutDir/tmp1.out";
print TMP1FILE "First cut, libraries based ftype, lib prefix and suffix:\n\n";
for $i_ref (@First) {
        print TMP1FILE ">> ";
        for $elems (sort keys %$i_ref) {
                print TMP1FILE "$elems: $i_ref->{$elems}, ";
        }
        print TMP1FILE "\n";
}
close TMP1FILE;

open EXCLUDED, ">$OutDir/$Excluded";
print EXCLUDED "Liraries exluded from database:\n\n";
for $i_ref (@Excluded) {
        print EXCLUDED ">> ";
        for $elems (sort keys %$i_ref) {
                print EXCLUDED "$elems: $i_ref->{$elems}, ";
        }
        print EXCLUDED "\n";
}
close EXCLUDED;

$i = 0;
## Create new fields for each record 
for $i_ref (@First) {
	## Create 'src_dir:' (there's probably an easier way!)

	@work_path = split(/\//, $i_ref->{srcpath}); 
	## exclude last one
	#$src_dir = "";  ## Again, REAL important!
	## This is all pretty kludgy, and it just keeps getting worse!!!
	if (grep /work/, @work_path) {
		$skip = 1;
		$First_time = 1;
		for ($i_path = 0; $i_path < scalar(@work_path) - 1; $i_path++) {
			unless ($skip) {
				if ($First_time) {
					$src_dir = $work_path[$i_path];
					$First_time = 0;
				} else {
					$src_dir = join ('/', $src_dir, 
						$work_path[$i_path]);
				};
			}
			if ($work_path[$i_path] eq 'work') {
				$skip = 0;
				$First_time = 1;
			}
		}
	} else {
		$src_dir = $i_ref->{srcpath};
	};

	## create 'type:'
	if (grep /^lib/, $i_ref->{name}) {
		if ($i_ref->{libpath} eq 'lib') {
			$type = 'o32';
		} elsif ($i_ref->{libpath} eq 'usr/lib') {
			$type = 'o32';
		} elsif ($i_ref->{libpath} eq 'lib32') {
			$type = 'n32';
		} elsif ($i_ref->{libpath} eq 'usr/lib32') {
			$type = 'n32';
		} elsif ($i_ref->{libpath} eq 'lib64') {
			$type = 'n64';
		} elsif ($i_ref->{libpath} eq 'usr/lib64') {
			$type = 'n64';
		} else {
			$type = '-';
		};
	} else {
		$type = '-';
	}

	## 'date_found:' was already created above

	## Save it
	$Second[$i++] = {
		## Only fields required
		name 		=> $i_ref->{name},
		type		=> $type,
		src_dir		=> $src_dir,
		image   	=> $i_ref->{image},
		date_found	=> $date_found,
		## Extra stuff
		dstpath 	=> $i_ref->{dstpath},
		idbfile 	=> $i_ref->{idbfile},
		ftype		=> $i_ref->{ftype},
		srcpath 	=> $i_ref->{srcpath},
		libpath		=> $i_ref->{libpath}
	};
}

open TMP2FILE, ">$OutDir/tmp2.out";
print TMP2FILE "Remaining records with their new fields:\n\n";
for $i_ref (@Second) {
        print TMP2FILE "name: $i_ref->{name}, "; 
        print TMP2FILE "type: $i_ref->{type}, ";
        print TMP2FILE "src_dir: $i_ref->{src_dir}, ";
        print TMP2FILE "image: $i_ref->{image}, ";
	print TMP2FILE "date_found: $i_ref->{date_found}, ";
	## Don't need these!
        print TMP2FILE "dstpath: $i_ref->{dstpath}, "; 
        print TMP2FILE "ftype: $i_ref->{ftype}, "; 
        print TMP2FILE "idbfile: $i_ref->{idbfile}, ";
        print TMP2FILE "srcpath: $i_ref->{srcpath}, ";
        print TMP2FILE "libpath: $i_ref->{libpath}\n";
}
close TMP2FILE;

## Make array of just the libnames of type o32, n32, and n64, and their index.
## Then make an array of libnames of type '-' and their index. Put libraries 
## that are found in @ThirdA into @ThirdC to be discarded.

for $i_ref (@Second) {
	unless ($i_ref->{type} eq '-') {
		push @Publics, $i_ref->{name}; 
	}
}
$i = $j = $k = $l = $dup_count = 0;
for $i_ref (@Second) {
	if ($i_ref->{type} eq '-') {
		$found = 0;
		for $j_ref (@Publics) {
			if ($j_ref eq $i_ref->{name}) {
				$found = 1;	
				last;
			}
		}
		if ($found eq 0) {
			$ThirdB[$j++] = "$i_ref->{name} $i"; 
		} else {
			$ThirdC[$k++] = "$i_ref->{name}"; 
			$dup_count++;
		};
	} else {
		$ThirdA[$l++] = "$i_ref->{name} $i"; 
	};
	$i++;
};

open TMP2AFILE, ">$OutDir/tmp2a.out";
print TMP2AFILE "Output of ThirdA:\n\n";
for $i_ref (@ThirdA) {
	print TMP2AFILE "$i_ref\n";
}
close TMP2AFILE;


open TMP2BFILE, ">$OutDir/tmp2b.out";
print TMP2BFILE "Output of ThirdB:\n\n";
for $i_ref (@ThirdB) {
	print TMP2BFILE "$i_ref\n";
}
close TMP2BFILE;

open TMP2CFILE, ">$OutDir/tmp2c.out";
print TMP2CFILE "Output of ThirdC:\n\n";
for $i_ref (@ThirdC) {
	print TMP2CFILE "$i_ref\n";
}
close TMP2CFILE;

## Sort both sets of libraries, making each record one line of text
$i = 0;
for $i_ref (sort @ThirdA) {
	($Junk, $SecondIndex) = split / /, $i_ref;
	$j_ref = $Second[$SecondIndex];
	$Fourth[$i++] = join ' ', 
		$j_ref->{name},
		$j_ref->{type},
		$j_ref->{src_dir},  
		$j_ref->{image}, 
		$j_ref->{date_found},
		## Don't need these!
        	$j_ref->{dstpath},
        	$j_ref->{ftype},
        	$j_ref->{idbfile},
        	$j_ref->{srcpath},
        	$j_ref->{libpath};
	$pub_count++;
}
## DON'T RESET '$i'!!! These are 'type: -'
for $i_ref (sort @ThirdB) {
	($Junk, $SecondIndex) = split / /, $i_ref;
	$j_ref = $Second[$SecondIndex];
	$Fourth[$i++] = join ' ', 
		$j_ref->{name},
		$j_ref->{type},
		$j_ref->{src_dir},  
		$j_ref->{image}, 
		$j_ref->{date_found},
		## Don't need these!
        	$j_ref->{dstpath},
        	$j_ref->{ftype},
        	$j_ref->{idbfile},
        	$j_ref->{srcpath},
        	$j_ref->{libpath};
	$priv_count++;
}

open TMP3FILE, ">$OutDir/tmp3.out";
print TMP3FILE "Sorted by library:\n\n";
for $i_ref (@Fourth) {
	print TMP3FILE "$i_ref\n";
}
close TMP3FILE;

## Split up each field again within each record. Collapse the libraries so 
## so that there is now one record for each library. src_dir, type, and image
## can be a list, so there may be more than one for each library. 
## Also must add owner, exports, and public.
$i = $lib_count = $RestOfLibsStart = 0;
for $i_ref (@Fourth) {
	($name,$type,$src_dir,$image,$date_found,@junk) = split / /, $i_ref;
	if ($type eq '-') {
		$type = ' ';
	};
	## First time through
	unless ($i) {
		## Can't combine this with the next 'elsif', because $last_name
		## hasn't been assigned yet.
	
		## Assume public libs first, no need to check 'type'
		$upub_count = 1;
		$Fifth[$i++] = {
			name		=> $name,
			occurs		=> 1,
			type		=> $type,
			src_dir		=> $src_dir,
			image		=> $image,
			date_found	=> scalar localtime($date_found),
			owner		=> '  **NEW**',
			exports		=> '**NEW**',
			updated		=> '**NEW**'
		};
		$last_name	= $name;
		$last_type	= $type;
		$last_src_dir	= $src_dir;
		$last_image	= $image;
		$lib_count++;
	## library name changes
	} elsif ($name ne $last_name) {
		## increment unique lib count and capture number of first one
		if ($type eq ' ') {
			$upriv_count++; 
			unless ($RestOfLibsStart) {
				$RestOfLibsStart = $i - 1;
			}
		} else {
			$upub_count++;
		}
		$Fifth[$i++] = {
			name		=> $name,
			occurs		=> 1,
			type		=> $type,
			src_dir		=> $src_dir,
			image		=> $image,
			date_found	=> scalar localtime($date_found),
			owner		=> '  **NEW**',
			exports		=> '**NEW**',
			updated		=> '**NEW**'
		};
		$last_name	= $name;
		$last_type	= $type;
		$last_src_dir	= $src_dir;
		$last_image	= $image;
		$lib_count++;
	## Same library, but the type, src_dir, and image need to be checked. 
	## date_found is generated here, and is unique. type and image are 
	## not, and src_dir probably is (unique).
	} else {
		$Fifth[$i-1]{occurs}++;
		## Check each one. Regular expressions don't like the '++' 
		## as in 'c++_dev_sw.lib'. Use that magical '\Q'!
		if (grep !/\Q$type/, $last_type) {
			$last_type = "$last_type, $type";
			## Already incremented $i
			$Fifth[$i-1]{type} = "$last_type";
		}
		if (grep !/\Q$src_dir/, $last_src_dir) {
			$last_src_dir = "$last_src_dir, $src_dir";
			$Fifth[$i-1]{src_dir} = "$last_src_dir";
		}
		if (grep !/\Q$image/, $last_image) {
			$last_image = "$last_image, $image";
			$Fifth[$i-1]{image} = "$last_image";
		}
	}
}

## Output this week's database
open OUTFILE, ">$OutDir/$Outfile";
print OUTFILE "File = libraries.db\t\t\t";
print OUTFILE "Legend:\n";
print OUTFILE "\n";
print OUTFILE "Current files excluded:\t$ex_count\t\t";
print OUTFILE "name - library name\n";
print OUTFILE "Current type 'f' files:\t$f_count\t\t";
print OUTFILE "occurences - no. times lib in .idb file\n";
print OUTFILE "Current type 'l' files:\t$l_count\t\t";
print OUTFILE "type - type of library\n";
print OUTFILE "\t\t\t\t\t";
print OUTFILE "src_dir - src directory from .idb file\n";
print OUTFILE "Current public libs:\t$pub_count\t\t"; 
print OUTFILE "images - installed image from .idb file\n";
print OUTFILE "Current duplicate libs:\t$dup_count\t\t"; 
print OUTFILE "date found - mtime of first .idb file\n";
print OUTFILE "Current private libs:\t$priv_count\t\t"; 
print OUTFILE "owner - person who owns library\n";
print OUTFILE "\t\t\t\t\t";
print OUTFILE "exports - lib has exports file (y/n)\n";
print OUTFILE "Total unique public libs:  $upub_count\t\t"; 
print OUTFILE "updated - lib has been updated (y/n)\n";
print OUTFILE "Total unique private libs: $upriv_count\n"; 
print OUTFILE "Grand total unique libs:   $lib_count\n";
print OUTFILE "\n";
print OUTFILE '=' x 79, "\n";
$Counter = 0;
for $i_ref (@Fifth) {
	print OUTFILE "name: $i_ref->{name}\n";
	print OUTFILE "occurences: $i_ref->{occurs}\n";
	print OUTFILE "type: $i_ref->{type}\n";
	print OUTFILE "src_dir: $i_ref->{src_dir}\n";
	print OUTFILE "images: $i_ref->{image}\n";
	print OUTFILE "date_found: $i_ref->{date_found}\n";
	## Values, or at least a blank space after the ':' are required! 
	print OUTFILE "owner: $i_ref->{owner}\n";
	print OUTFILE "exports: $i_ref->{exports}\n";
	print OUTFILE "updated: $i_ref->{updated}\n";
	print OUTFILE "\n";
	($Counter == $RestOfLibsStart) ? print OUTFILE '=' x 79, "\n" :
		print OUTFILE '-' x 79, "\n";
	$Counter++;
}
close OUTFILE;

## Remove this!
#unlink "$OutDir/$Database";

## Check if '$Database' exists, if not create it from this week's db and exit.
unless (-e "$OutDir/$Database") {
	use File::Copy;
	unless (copy "$OutDir/$Outfile", "$OutDir/$Database") {
		die "Error: can't copy \'$OutDir/$Outfile\' to ", 
			"\'$OutDir/$Database\'.\n";
	}
	exit;
} 

## Rename the existing database (you know it exists) deleting any previous database.
if (-e "$OutDir/$OldDatabase") {
	unlink "$OutDir/$OldDatabase";
}
unless (rename "$OutDir/$Database", "$OutDir/$OldDatabase") {
	die "Error: can't rename \'$OutDir/$Database\' to ", 
			"\'$OutDir/$OldDatabase\'.\n";
}

## Open up the old (now renamed) database and read in the whole thing 
unless (open(DATABASE, "$OutDir/$OldDatabase")) {
	die "Error: Can't open $OutDir/$OldDatabase: $!\n";
}
## Get rid of header, look for '='
while ($OneLine = <DATABASE>) {
	last if $OneLine =~ /^=/;
}
## Eat up the public libraries
$j = 0;
while ($OneLine = <DATABASE>) {
	## Looks like this 'while' mess could be made into a loop, huh?
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{name})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{occurs})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{type})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{src_dir})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{image})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{date_found})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{owner})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{exports})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseA[$j]{updated})	= split / /, $OneLine, 2;
	## Throw away last line and check for '=' on next line
	$OneLine = <DATABASE>;
	$OneLine = <DATABASE>;
	last if $OneLine =~ /^=/;
	$j++;
}
## Eat up the rest of the libraries
$j = 0;
while ($OneLine = <DATABASE>) {
	## Looks like this 'while' mess could be made into a loop, huh?
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{name})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{occurs})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{type})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{src_dir})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{image})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{date_found})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{owner})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{exports})	= split / /, $OneLine, 2;
	$OneLine = <DATABASE>;
	chop $OneLine;
	($junk, $TheDataBaseB[$j]{updated})	= split / /, $OneLine, 2;
	## Throw away last line two lines
	$OneLine = <DATABASE>;
	$OneLine = <DATABASE>;
	$j++;
}
close DATABASE;

## Print out the existing database of public libs
open TMP4AFILE, ">$OutDir/tmp4a.out";
print TMP4AFILE "The exising database:\n\n";
for $i_ref (@TheDataBaseA) {
	print TMP4AFILE '-' x 79, "\n";
	print TMP4AFILE "name: $i_ref->{name}\n";
	print TMP4AFILE "occurences: $i_ref->{occurs}\n";
	print TMP4AFILE "type: $i_ref->{type}\n";
	print TMP4AFILE "src_dir: $i_ref->{src_dir}\n";
	print TMP4AFILE "images: $i_ref->{image}\n";
	print TMP4AFILE "date_found: $i_ref->{date_found}\n";
	print TMP4AFILE "owner: $i_ref->{owner}\n";
	print TMP4AFILE "exports: $i_ref->{exports}\n";
	print TMP4AFILE "updated: $i_ref->{updated}\n";
	print TMP4AFILE "\n";
}
close TMP4BFILE;
## Print out the existing database of private libs
open TMP4BFILE, ">$OutDir/tmp4b.out";
print TMP4BFILE "The exising database:\n\n";
for $i_ref (@TheDataBaseB) {
	print TMP4BFILE '-' x 79, "\n";
	print TMP4BFILE "name: $i_ref->{name}\n";
	print TMP4BFILE "occurences: $i_ref->{occurs}\n";
	print TMP4BFILE "type: $i_ref->{type}\n";
	print TMP4BFILE "src_dir: $i_ref->{src_dir}\n";
	print TMP4BFILE "images: $i_ref->{image}\n";
	print TMP4BFILE "date_found: $i_ref->{date_found}\n";
	print TMP4BFILE "owner: $i_ref->{owner}\n";
	print TMP4BFILE "exports: $i_ref->{exports}\n";
	print TMP4BFILE "updated: $i_ref->{updated}\n";
	print TMP4BFILE "\n";
}
close TMP4BFILE;

## Make an array of the libnames from the existing databases with their indicies
$i = $j = 0;
for $i_ref (@TheDataBaseA) {
	$LibNamesA[$i] = "$i_ref->{name} $i"; 
	$AllNamesA[$i] = "$i_ref->{name}";
	$i++;
}
for $j_ref (@TheDataBaseB) {
	$LibNamesB[$j] = "$j_ref->{name} $j"; 
	$AllNamesB[$j] = "$j_ref->{name}";
	$j++;
}

## Check the lib names in the new list '@Fifth' with the lib names in 
## '@AllNames?' that come from '@TheDataBase?' 
open NEWLIBSA, ">$OutDir/$NewLibsFileA";
$new_upub_count = 0;
print NEWLIBSA "New public libraries not found in the existing database:\n\n";
for $i_ref (@Fifth) {
	if ($i_ref->{type} ne ' ') {
		$found_it = 0;
		for $j_ref (@AllNamesA) {
			if ($j_ref eq $i_ref->{name}) {
				$found_it = 1;
			}
		}
		unless ($found_it) {
			## Add the lib to the end of @LibNamesA 
			$new_index = $#LibNamesA + 1;
			push @LibNamesA, "$i_ref->{name} $new_index";

			## Add whole record to end of the existing database
			push @TheDataBaseA, $i_ref; 
		
			## Output new libraries
			print NEWLIBSA '-' x 79, "\n";
			print NEWLIBSA "name: $i_ref->{name}\n";
			print NEWLIBSA "occurences: $i_ref->{occurs}\n";
			print NEWLIBSA "type: $i_ref->{type}\n";
			print NEWLIBSA "src_dir: $i_ref->{src_dir}\n";
			print NEWLIBSA "images: $i_ref->{image}\n";
			print NEWLIBSA "date_found: $i_ref->{date_found}\n"; 
			print NEWLIBSA "owner: $i_ref->{owner}\n";
			print NEWLIBSA "exports: $i_ref->{exports}\n";
			print NEWLIBSA "updated: $i_ref->{updated}\n";
			print NEWLIBSA "\n";

			$new_upub_count++;
		}
	}
}
close NEWLIBSA;
open NEWLIBSB, ">$OutDir/$NewLibsFileB";
$new_upriv_count = 0;
print NEWLIBSB "New private libraries not found in the existing database:\n\n";
for $i_ref (@Fifth) {
	if ($i_ref->{type} eq ' ') {
		$found_it = 0;
		for $j_ref (@AllNamesB) {
			if ($j_ref eq $i_ref->{name}) {
				$found_it = 1;
			}
		}
		unless ($found_it) {
			## Add the lib to the end of @LibNamesB 
			$new_index = $#LibNamesB + 1;
			push @LibNamesB, "$i_ref->{name} $new_index";

			## Add whole record to end of the existing database
			push @TheDataBaseB, $i_ref; 
		
			## Output new libraries
			print NEWLIBSB '-' x 79, "\n";
			print NEWLIBSB "name: $i_ref->{name}\n";
			print NEWLIBSB "occurences: $i_ref->{occurs}\n";
			print NEWLIBSB "type: $i_ref->{type}\n";
			print NEWLIBSB "src_dir: $i_ref->{src_dir}\n";
			print NEWLIBSB "images: $i_ref->{image}\n";
			print NEWLIBSB "date_found: $i_ref->{date_found}\n"; 
			print NEWLIBSB "owner: $i_ref->{owner}\n";
			print NEWLIBSB "exports: $i_ref->{exports}\n";
			print NEWLIBSB "updated: $i_ref->{updated}\n";
			print NEWLIBSB "\n";

			$new_upriv_count++;
		}
	}
}
close NEWLIBSB;

open TMP5AFILE, ">$OutDir/tmp5a.out";
print TMP5AFILE "Existing public libraries with new libraries at the bottom:\n\n";
for $i (@LibNamesA) {
	print TMP5AFILE "$i\n";
}
close TMP5AFILE;
open TMP5BFILE, ">$OutDir/tmp5b.out";
print TMP5BFILE "Existing private libraries with new libraries at the bottom:\n\n";
for $i (@LibNamesB) {
	print TMP5BFILE "$i\n";
}
close TMP5BFILE;

$tot_upub_count = $#LibNamesA + 1; 
$tot_upriv_count = $#LibNamesB + 1;
$tot_lib_count = $tot_upub_count + $tot_upriv_count;

## Ouput sorted new database
open OUTFILE2, ">$OutDir/$Database";
## Same as above
print OUTFILE2 "File = libraries.db\t\t\t";
print OUTFILE2 "Legend:\n";
print OUTFILE2 "\n";
print OUTFILE2 "Current files excluded:\t$ex_count\t\t";
print OUTFILE2 "name - library name\n";
print OUTFILE2 "Current type 'f' files:\t$f_count\t\t";
print OUTFILE2 "occurences - no. times lib in .idb file\n";
print OUTFILE2 "Current type 'l' files:\t$l_count\t\t";
print OUTFILE2 "type - type of library\n";
print OUTFILE2 "\t\t\t\t\t";
print OUTFILE2 "src_dir - src directory from .idb file\n";
print OUTFILE2 "Current public libs:\t$pub_count\t\t"; 
print OUTFILE2 "images - installed image from .idb file\n";
print OUTFILE2 "Current duplicate libs:\t$dup_count\t\t"; 
print OUTFILE2 "date found - mtime of first .idb file\n";
print OUTFILE2 "Current private libs:\t$priv_count\t\t"; 
print OUTFILE2 "owner - person who owns library\n";
print OUTFILE2 "\t\t\t\t\t";
print OUTFILE2 "exports - lib has exports file (y/n)\n";
print OUTFILE2 "Total unique public libs:  $tot_upub_count\t\t"; 
print OUTFILE2 "updated - lib has been updated (y/n)\n";
print OUTFILE2 "Total unique private libs: $tot_upriv_count\n"; 
print OUTFILE2 "Grand total unique libs:   $tot_lib_count\n";
print OUTFILE2 "\n";
## Different than above
print OUTFILE2 "New public libs added:\t$new_upub_count\n";
print OUTFILE2 "New private libs added:\t$new_upriv_count\n";
print OUTFILE2 "\n";

print OUTFILE2 '=' x 79, "\n";
$Counter = 0;
for $i_ref (sort @LibNamesA) {
	($Junk, $Sorted) = split / /, $i_ref;
	$j_ref = $TheDataBaseA[$Sorted];
	print OUTFILE2 "name: $j_ref->{name}\n";
	print OUTFILE2 "occurences: $j_ref->{occurs}\n";
	print OUTFILE2 "type: $j_ref->{type}\n";
	print OUTFILE2 "src_dir: $j_ref->{src_dir}\n";
	print OUTFILE2 "images: $j_ref->{image}\n";
	print OUTFILE2 "date_found: $j_ref->{date_found}\n";
	print OUTFILE2 "owner: $j_ref->{owner}\n";
	print OUTFILE2 "exports: $j_ref->{exports}\n";
	print OUTFILE2 "updated: $j_ref->{updated}\n";
	print OUTFILE2 "\n";
	($Counter == $#LibNamesA) ? print OUTFILE2 '=' x 79, "\n" :
		print OUTFILE2 '-' x 79, "\n";
	$Counter++;
}
for $i_ref (sort @LibNamesB) {
	($Junk, $Sorted) = split / /, $i_ref;
	$j_ref = $TheDataBaseB[$Sorted];
	print OUTFILE2 "name: $j_ref->{name}\n";
	print OUTFILE2 "occurences: $j_ref->{occurs}\n";
	print OUTFILE2 "type: $j_ref->{type}\n";
	print OUTFILE2 "src_dir: $j_ref->{src_dir}\n";
	print OUTFILE2 "images: $j_ref->{image}\n";
	print OUTFILE2 "date_found: $j_ref->{date_found}\n";
	print OUTFILE2 "owner: $j_ref->{owner}\n";
	print OUTFILE2 "exports: $j_ref->{exports}\n";
	print OUTFILE2 "updated: $j_ref->{updated}\n";
	print OUTFILE2 "\n";
	print OUTFILE2 '-' x 79, "\n";
}
close OUTFILE2;
