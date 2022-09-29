#!/usr/bin/perl

$line_num = 0;
$entry_num = 0;
$rule_num = 0;
$size_cond_table = 0;
$size_act_table = 0;
$first_condition = 1;
$first_action = 1;

$header_file = "$ARGV[1]/sn0_fru_analysis.h";
$c_file = "$ARGV[1]/sn0_fru_tables.c";
$cond_file = "cond_file.c";
$action_file = "action_file.c";
$rules_file =  "rules_file.c";

unlink $c_file,$cond_file,$action_file,$rules_file;

open(TEXT_FILE,$ARGV[0]) || &fail("unable to open file $ARGV[0]\n");
open(HEADER_FILE,$header_file) || &fail("unable to open file $header_file\n");
open(C_FILE,"> $c_file") || &fail("unable to open file $c_file\n");
open(COND_FILE,"> $cond_file") || &fail("unable to open file $cond_file\n");
open(ACTION_FILE,"> $action_file") || &fail("unable to open file $action_file\n");
open(RULES_FILE,"> $rules_file") || &fail("unable to open file $rules_file\n");

print COND_FILE "\n\n/* table of conditions */\nkf_cond_t kf_cond_tab[] = {\n";
print ACTION_FILE "\n\n/* table of action sets */\nkf_action_set_t kf_action_set_tab[] = {\n";
print RULES_FILE "\n\n/* table of rules */\nkf_rule_t kf_rule_tab[] = {\n";

print C_FILE "/*\n     This file was created by parsing the file ",
    "$ARGV[0]. Do not\n    edit this file manually, all changes ",
    "should be made to the text file.\n*/\n\n",
    "#include \"sn0_fru_analysis.h\"\n\n",
# KF_CREATE_BIT_MASK is a C macro used because perl does not have 64 bit
# values, it is used below instead of the perl shift operator.
    "#define KF_CREATE_BIT_MASK(_val)\t\t(0x1ull << _val)\n\n";

&get_header_values;

NEW_ENTRY:
while($new_line = &next_line) {
    local(%actions);
    
    if ($new_line =~ /^section:\s*(\w+)/) {
	# create a new section
	if ($rule_num > 0) {
	    # add sentinal rule
	    $cur_condition = -1;
	    $cur_action = -1;
	    &add_rule;
	}
	$section_name = $1;
	print C_FILE "const int KF_$1_RULE_INDEX\t\t\t= $rule_num;\n";
    }
    elsif (($new_line =~ /^register:\s*(\w+)/) && ($section_name ne "")) {
	$register = ("KF_" . $1 . "_INDEX");
	$register_offset = $register_offset{$register};
	if ($register_offset eq "") {&fail("line $line_num: invalid register name: $register\n")};
	
	$new_line = &next_line;
	
# get and check entry's condition syntax
	$new_line  =~ /^condition:\s*(.*)/ || &fail("line $line_num: expected keyword condition:\n");
	@condition = split(' ',$1);
	$operator = shift(@condition);
	if (($operator ne "AND") && ($operator ne "OR")) {
	    &fail("line $line_num: illegal operator value: $operator\n");
	}
	
# print out c code corresponding to this entry's condition
	if ($first_condition == 0) {print COND_FILE ",\n";}
	print COND_FILE "\n/* condition for $register op: $operator bit(s): @condition */\n";
	$first_condition = 1;
	$cur_condition = $size_cond_table;
	foreach $i (0..$#condition) {
	    $condition[$i] =~ /^(\D*)(\d+)/;
	    if ($first_condition == 1) {
# KF_CREATE_BIT_MASK is a C macro used because perl does not have 64 bit values
		print COND_FILE "{KC_LITERAL,{$register_offset, KF_CREATE_BIT_MASK($2)}}";
		$first_condition = 0;
	    }
	    else {
# KF_CREATE_BIT_MASK is a C macro used because perl does not have 64 bit values
		print COND_FILE ",\n{KC_LITERAL,{$register_offset, KF_CREATE_BIT_MASK($2)}}";
	    }
	    $size_cond_table++;
	    if ($1 eq "!") {
		print COND_FILE ",\n{KC_OPERAND,KF_NOT}";
		$size_cond_table++;
	    }
	    elsif ($1 ne "") {
		&fail("Syntax error, expected !, but got %1\n"); 
	    }
	}
	foreach $i (1..$#condition) {
	    print COND_FILE ",\n{KC_OPERAND,KF_$operator}";
	    $size_cond_table++;
	}
	print COND_FILE ",\n{KF_LIST_SENTINEL,{0,0}}";
	$size_cond_table++;
	
	
	$new_line = &next_line;
	
	$new_line =~ /^actions:/ || &fail("line $line_num: expected key word action:\n");
	
	$new_line = &next_line;
	
	while(!($new_line =~ /^\s*end\s*\n/)) {
	    if ($new_line =~ /\s*(\w+)[,]\s*(\d\d\d?)[%]/) {
		if ($confidence_offset{"KF_$1_CONF_INDEX"} eq "") { 
		    &fail("line $line_num: invalid hardware name: $1\n");
		}
		elsif ($2 > 100) { 
		    &fail("line $line_num: invalid confidence level: $2\n");
		}
		$actions{$1} = $2;
		$new_line = &next_line;
	    }
	    elsif ($new_line =~ /^\s*duplicate\n/) {
		&add_rule;
		next NEW_ENTRY;
	    }
	    else {
		&fail("line $line_num: illegal action syntax\n");
	    }
	}
	if ($first_action == 0) {print ACTION_FILE ",\n";}
	print ACTION_FILE "\n/* action set for $register op: $operator bit(s): @condition */\n";
	$first_action = 1;
	$cur_action = $size_act_table;
	foreach $key (keys(%actions)) {
	    if ($first_action == 1) {
		print ACTION_FILE "{KAS_ACTION,KF_",$key,"_CONF_INDEX,$actions{$key}}";
		$first_action = 0;
	    }
	    else {
		print ACTION_FILE ",\n{KAS_ACTION,KF_",$key,"_CONF_INDEX,$actions{$key}}";
	    }
	    $size_act_table++;
	}
	print ACTION_FILE ",\n{KF_LIST_SENTINEL,0}";
	$size_act_table++;
	
	&add_rule;
    }
    else {
	if ($section_name eq "") {
	    &fail("line $line_num: expected value for \"section:\"\n");
	}
	else {
	    &fail("line $line_num: expected value for \"register:\"\n");
	}
    }
}

print COND_FILE "\n};\n";
print ACTION_FILE "\n};\n";

# add last sentinal rule
$cur_condition = -1;
$cur_action = -1;
&add_rule;
print RULES_FILE "\n};\n";


close(COND_FILE);
close(ACTION_FILE);
close(RULES_FILE);

open(COND_FILE,$cond_file);
while(<COND_FILE>) {
    print C_FILE $_;
}
open(ACTION_FILE,$action_file);
while(<ACTION_FILE>) {
    print C_FILE $_;
}
open(RULES_FILE,$rules_file);
while(<RULES_FILE>) {
    print C_FILE $_;
}

unlink $cond_file,$action_file,$rules_file;

# End of program, subroutine definitions follow.

#=============================================================================
#
# This routine prints out the C code for the rule table entry for the
# given condition and action set.
#
#=============================================================================
sub add_rule {
    if ($rule_num == 0) {
	print RULES_FILE "{$cur_condition,$cur_action}";
    }
    else {
	print RULES_FILE ",\n{$cur_condition,$cur_action}";
    }
    $rule_num++;
}


#=============================================================================
#
# This routine skips all commented and blank lines and returns the next valid
# line of the text file we are parsing.
#
#=============================================================================
sub next_line {
    local($new_line);

    $new_line = <TEXT_FILE>;
    $line_num += 1;

    while (($new_line =~ /^[\#\n]/) || ($new_line =~ /^\s+\n/)) {
	$new_line = <TEXT_FILE>;
	$line_num += 1;
    }

    $new_line;
}



#==============================================================================
#
# This routine reads in the FRU header file so we can check for valid register
# names, hardware components and confidence levels in the test file.
#
#==============================================================================
sub get_header_values {
    local($new_line);

# read in register address indices
    while (!($new_line =~ /START OF REGISTER ADDRESS TABLE INDICIES/)) {
	$new_line = <HEADER_FILE> || &fail("Unexpected end of header file reached\n");
    }
    $new_line = <HEADER_FILE> || &fail("Unexpected end of header file reached\n");
    while (!($new_line =~ /END OF REGISTER ADDRESS TABLE INDICIES/)) {
	if ($new_line =~ /^\s*\#define\s+(\w*)\s+(\d+)/) {
	    $register_offset{$1} = $2;
	}
	$new_line = <HEADER_FILE> || &fail("Unexpected end of header file reached\n");
    }

# read in confidence level address indicies
    $new_line = <HEADER_FILE> || &fail("Unexpected end of header file reached\n");
    while (!($new_line =~ /START OF CONFIDENCE LEVEL ADDRESS TABLE INDICIES/)) {
	$new_line = <HEADER_FILE>;
    }
    $new_line = <HEADER_FILE> || &fail("Unexpected end of header file reached\n");
    while (!($new_line =~ /END OF CONFIDENCE LEVEL ADDRESS TABLE INDICIES/)) {
	if ($new_line =~ /^\s*\#define\s+(\w*)\s+(\d+)/) {
	    $confidence_offset{$1} = $2;
	}
	$new_line = <HEADER_FILE> || &fail("Unexpected end of header file reached\n");
    }
}


#=============================================================================
#
# This routine does any clean up we need when parsing fails.
#
#=============================================================================
sub fail {
    # parsing failed so remove all the files we've created... 
    unlink $cond_file,$action_file,$rules_file,$c_file;

    die "SN0 FRU PARSER ERR: $_[0]";
}
