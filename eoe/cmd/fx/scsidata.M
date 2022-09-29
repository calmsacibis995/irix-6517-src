#
# menu data.  this file determines the content of the menu tree.
# there are some coupling between this file and nameitem() in input.c
#
# Commented out items are not implemented yet. This is mainly bad block
# manipulation.
# See Menucvt.awk

# provide driver name
DRIVER	scsi
# establish funcvals
FUNC	dumpbuf
	dumpbuf [T BO N]
	print contents of the buffer
	T = data type {bytes, shorts, longs}
	BO = buffer offset in bytes
	N = how many data to print
FUNC	editbuf
	editbuf [T BO D ...]
	edit contents of the buffer
	T = data type {bytes, words, longs}
	BO = buffer offset in bytes
	D = new data
FUNC	cmpbuf
	cmpbuf [BO BO N]
	compare bytes
	BO = buffer offset in bytes
	N = number of bytes to compare
FUNC	fillbuf
	fillbuf [BO STR N]
	fill the buffer with copies of a string
	BO = buffer offset in bytes
	STR = source string
	N = number of bytes to fill
FUNC	number
	number [N]
	print number in decimal, octal, and hex
FUNC	readbuf
	readbuf [BO N]
	read data into buffer
	BO = buffer offset in bytes
	N = number of bytes to read
FUNC	seek
	seek [BN]
	set the readbuf / writebuf offset
	BN = block number

FUNC	writebuf
	writebuf [BO N]
	write data from buffer to disk
	BO = buffer offset in bytes
	N = number of bytes to write

FUNC	showcapacity
	showcapacity
	Show the drive capacity returned by the drive

FUNC	showpages
	showpages [-cmds]
	show supported mode select pages and lengths
	[-c] show current page data (default if no options used)
	[-m] show modifiable page data
	[-d] show mfg's default page data
	[-s] show saved page data

FUNC	setpage
	setpage
	Do a modeselect on an arbitrary page; bytes not 
	entered are 0; all values are masked against
	changeable values.  The result becomes the current
	and saved values, if saved data is supported
	Be careful, no sanity checks are done

FUNC	format
	format (low level)
	NOTE:  This is almost a bad idea with SCSI hard disks!

	Format the disk.  This is a single SCSI operation.
	This means you will see no progress indication during the
	formatting process.  Many drives do not even blink
	the drive busy light during this process.  It will take
	several hours or longer for most drives.

FUNC	addbb
	addbb [BN]
	add to the badblock list
	BN = logical badblock number
FUNC	showbb
	showbb [-lbcfmg]
	print badblock list (default is logical block, grown only).
	Not all drives support all options, and may return default forms
	If the list is longer than 8192 entries, only the first 8192
	are reported
	[-l] report by as logical block format (default)
	[-b] report by as bytes from index
	[-c] report by as cyl/head/sector
	[-f] report full defect list
	[-m] report manufacturer's defect list
	[-g] report grown (added) defect list (default)

FUNC	butterfly
	butterfly [-a] [MOD BN N P]
	butterfly exercise
	-a = no questions asked
	MOD = {rd-only, wr-only, seek, wr-compare, ro-compare}
	BN = starting block number
	N = number of blocks
	P = number of passes
FUNC	complete
	complete [-a]
	complete read / write test
	-a = no questions asked
FUNC	errlog
	errlog [-il]
	print error log
	-i = clear log after printing
	-l = show block numbers
FUNC	random
	random [-a] [MOD BN N P]
	random test
	-a = no questions asked
	MOD = {rd-only, wr-only, seek, wr-compare, ro-compare}
	BN = starting block number
	N = number of blocks
	P = number of passes
FUNC	sequential
	sequential [-a] [MOD BN N P]
	sequential test
	-a = no questions asked
	MOD = {rd-only, wr-only, seek, wr-compare, ro-compare}
	BN = starting block number
	N = number of blocks
	P = number of passes
FUNC	stop_on_error
	stop_on_error
	toggle whether to stop on errors
FUNC	miscompares
	miscompares
	toggle whether to display differences on miscompares
FUNC	settestpat
	settestpat
	set data values (.. when done).   The pattern repeats
	based the bytes entered to fill the entire test buffer
	used for the write and write-cmp tests.
FUNC	showtestpat
	showtestpat
	print data test pattern that is repeated for write tests
FUNC	sync
	sync
	update label information in driver and on disk.

FUNC	auto
	auto
	automated setup of disk
	set sgi id info, exercise as complete -a, create default
	partition layout, and write volume header when done.

FUNC	exit
	exit
	exit from fx
FUNC	set set
	set
	set parts of the label
FUNC	show show
	show
	print parts of the label
FUNC	create create
	create
	reset parts of the label to defaults

FUNC	exercise ex
	exercise
	run drive exercises
FUNC	label lb
	label
	change items affecting disk label

FUNC	repartition repart
	repartition
	change disk partitions in a safe manner

FUNC	sroot_select
	rootdrive
	Repartition drive as a single filesystem system drive.
	Makes root and swap partitions with root as whole disk
	except for swap. Choice of xfs or efs partition type for
	root partition.

FUNC	root_select
	usrrootdrive
	Repartition drive as an split root,usr system drive.
	Makes root,swap,usr, and, optionally, xfslog partitions,
	with ~16 MB root, swap, and rest for usr.
	Choice of xfs or efs partition type for root and usr
	partitions.

FUNC	option_select
	optiondrive
	Repartition drive for use as an option (non-root) drive
	The entire drive except for the volume header area, and,
	optionally, the ~4MB xfslog partition is allocated to
	partition 7. Choice of xfs or efs partition type for
	option partition.

FUNC	pt_resize
	resize
	Resize a partition; adjust other partitions to fit.
	Only root, swap, and usr partitions may be resized
	with this menu.  Ensures that none of the standard
	partitions will overlap other standard partitions

FUNC	pt_expert
	expert [-b|-m]
	Repartition any partition.  Same as /label/set/part,
	and accepts the same flags (-b - partition by blocks,
	and -m - partition by megabytes (2^20 bytes))

FUNC	debug db
	debug
	debug functions

FUNC	badblock bb
	badblock
	operate on badblock list

FUNC	set_bi
	bootinfo
	set boot info (root, swap, and bootfile)
FUNC	scsi_setparam
	parameters
	set non-geometry parameters (no reformat needed)
FUNC	scsi_setgeom
	geometry
	set geometry parameters (needs reformatting before use)
FUNC	scsi_defparm
	manufacturer_params
	set all parameters to drive manufacturer's defaults.
	These may not be the same as the way Silicon Graphics
	sets the parameters prior to shipment
FUNC	set_pt
	partitions [-b|-m]
	set partition sizes
	-b - partition by blocks
	-m - partition by megabytes (2^20 bytes)
FUNC	set_sgiinfo
	sgiinfo
	set sgi id info (serial number and name)

FUNC	show_bi
	bootinfo
	print boot info (root, swap, and bootfile)
FUNC	show_dt
	directory
	print volume directory
FUNC	scsi_showparam
	parameters [ -cmds ]
	show non-geometry drive parameters
	-c - current values (default if no options used)
	-d - drive manufactures default values
	-s - saved values (usually same as current)
	-m - modifiable values (values that can be modified)
FUNC	scsi_showgeom
	geometry [ -cmds ]
	show drive geometry parameters
	-c - current values (default if no options used)
	-d - drive manufactures default values
	-s - saved values (usually same as current)
	-m - modifiable values (values that can be modified)
FUNC	show_pt
	partitions
	print partition table
FUNC	show_sgiinfo
	sgiinfo
	print sgi id info (serial number and name)
FUNC	show_all
	all
	print the whole label:
	boot info, volume directory,
	drive parameters, geometry, partitions,
	and sgi id info.

FUNC	create_pt
	partitions
	create default partitions
FUNC	create_sgiinfo
	sgiinfo
	create default sgi id info (serial number and name)
FUNC	create_bi
	bootinfo
	create default boot info (root, swap, and bootfile)
FUNC	create_dt
	directory
	create default volume directory (clean)
FUNC	create_all
	all
	create the whole volume label
	creates default partitions, sgi id info, boot info,
	an empty volume directory, and fills in parameters
	and geometry information from current values.

# menu definitions. Items tagged ITEM always appear, those tagged
# XITEM appear & are selectable only in 'expert' mode. Note that
# because of the way the code handles menus, all normal ITEMs must
# appear before any XITEMs.

MENU	set set
XITEM	scsi_setparam parameters
XITEM	scsi_setgeom geometry
XITEM	set_pt partitions
XITEM	scsi_defparm manufacturer_params
XITEM	set_sgiinfo sgiinfo
XITEM	set_bi bootinfo

MENU	show show
ITEM	scsi_showparam parameters
ITEM	scsi_showgeom geometry
ITEM	show_pt partitions
ITEM	show_sgiinfo sgiinfo
ITEM	show_bi bootinfo
ITEM	show_dt directory
ITEM	show_all all

# note that there is deliberately nothing here for parameters or
# geometry, since they can't be 'created' for scsi.   They can
# only be done via set, where there are numerous warnings, etc.
# this also helps prevent people from getting drive manufactures
# defaults, rather than what SGI ships from the factory.
MENU	create create
XITEM	create_pt partitions
XITEM	create_sgiinfo sgiinfo
XITEM	create_bi bootinfo
XITEM	create_dt directory
XITEM	create_all all

MENU	db debug
ITEM	cmpbuf
ITEM	dumpbuf
ITEM	editbuf
ITEM	fillbuf
ITEM	number
ITEM	readbuf
ITEM	seek
XITEM	writebuf
XITEM	showcapacity
XITEM	showpages
XITEM	setpage
XITEM	format

MENU	bb badblock
ITEM	addbb
ITEM	showbb

MENU	ex exercise
ITEM	butterfly
ITEM	errlog
ITEM	random
ITEM	sequential
ITEM	stop_on_error
ITEM	miscompares
XITEM	settestpat
XITEM	showtestpat
XITEM	complete

MENU	lb label
ITEM	show
XITEM	sync
XITEM	set
XITEM	create

MENU	repart repartition show_pt_cap
ITEM	sroot_select rootdrive
ITEM	root_select usrrootdrive
ITEM	option_select optiondrive
ITEM	pt_resize resize
# this item is the same as on the label/set/partition, and is here only
# if in expert mode, simply because even 'experts' may tend to come here
# first, since 'label' isn't terribly intuitive
XITEM	pt_expert expert

MENU	fx fx
ITEM	exit
ITEM	badblock
ITEM	debug
ITEM	exercise
ITEM	label
ITEM	repartition
XITEM	auto

FUNC	dot
	.
	dummy for nameitem
FUNC	dotdot
	..
	quit current function or menu
FUNC	help
	? [ITEM ...]
	print help for ITEM

