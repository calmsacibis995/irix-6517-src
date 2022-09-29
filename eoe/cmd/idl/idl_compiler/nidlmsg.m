$ ++
$ ! @OSF_COPYRIGHT@
$ ! COPYRIGHT NOTICE
$ ! Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
$ ! ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
$ ! src directory for the full copyright text.
$ ! 
$ ! HISTORY
$ ! Revision 1.1.5.5  1993/01/03  21:40:56  bbelch
$ ! 	Embedding copyright notice
$ ! 	[1993/01/03  14:37:24  bbelch]
$ !
$ ! Revision 1.1.5.4  1992/12/23  18:50:03  zeliff
$ ! 	Embedding copyright notice
$ ! 	[1992/12/23  01:04:42  zeliff]
$ ! 
$ ! Revision 1.1.5.3  1992/12/03  21:37:31  hinxman
$ ! 	Fix OT5606 - typographical errors
$ ! 	[1992/12/03  18:50:44  hinxman]
$ ! 
$ ! Revision 1.1.5.2  1992/12/03  13:14:01  hinxman
$ ! 	Fix OT 6078 and 6269 - duplicate attributes are errors
$ ! 	[1992/12/03  13:09:43  hinxman]
$ ! 
$ ! Revision 1.1.2.4  1992/06/22  21:07:04  rsalz
$ ! 	Close OT3589; fix typo attriubte --> attribute.
$ ! 	[1992/06/22  21:01:20  rsalz]
$ ! 
$ ! $EndLog$
$ ! 
$ !  Copyright (c) 1989 by
$ !      Hewlett-Packard Company, Palo Alto, Ca. & 
$ !      Digital Equipment Corporation, Maynard, Mass.
$ !
$ !  NAME:
$ !
$ !      nidlmsg.msg
$ !
$ !  FACILITY:
$ !
$ !      Interface Definition Language (IDL) Compiler
$ !
$ !  ABSTRACT:
$ !
$ !  RPC IDL Compiler messages.
$ !
$ !  VERSION: DCE 1.0
$ !
$ --
$ + 
$  These two symbols must have the same integer value.
$ -
$set 1 

1 43

2 Use %1$s for list of command options
$ Explanation:
$ Errors on the command line prevent the IDL compiler from
$ executing.
$ User Action:
$ Invoke the IDL compiler with the indicated command option to
$ get a list of valid command options, then correct the error.

3 Operation %1$s has no binding handle parameter; [auto_handle] assumed
$ Explanation:
$ When an operation uses parametric binding, its first parameter
$ is a handle parameter that determines the location of a
$ server of the interface.  Operations whose first parameter
$ is not of type <kw>(handle_t) and does not have the <kw>([handle])
$ attribute are assumed to use nonparametric binding
$ known as <kw>(auto_handle), whereby a binding is automatically
$ established by the RPC runtime library.
$ User Action:
$ See the documentation on the various binding techniques
$ to determine the best method for your application.

4 File %1$s, line %2$lu: %3$s
$ Explanation:
$ Identifies the filename, source line number, and source
$ text associated with an error.
$ User Action:
$ None.  This is an informational message.

5 Must compile stubs with ANSI C compiler to avoid promotion of float to double in operation %1$s
$ Explanation:
$ The operation contains a <kw>(float) parameter passed by value.
$ Non-ANSI C compilers that do not support function prototypes,
$ automatically promote <kw>(float) to <kw>(double).  Function
$ prototypes within generated C stubs are conditional on the
$ IDL_PROTOTYPES preprocessor symbol.  When you compile a
$ stub with a non-ANSI C compiler, IDL_PROTOTYPES must not be
$ defined (causing the non-prototyped definitions to compile)
$ and thus causing <kw>(float) to <kw>(double) promotion.  This breaks
$ the marshalling logic in the stub, that expects a <kw>(float), not
$ a <kw>(double).
$ User Action:
$ Problems do not occur
$ as long as you compile stubs with an ANSI C compiler.  However,
$ to maximize portability, it is highly recommended that you
$ modify any <kw>(float) parameter passed by value to either a <kw>(double)
$ passed by value or a <kw>(float) passed by reference. This
$ message is informational.

6 Importing IDL file %1$s

7 Creating include file %1$s

8 Legal values are:
$ Explanation:
$ Identifies a list of legal values for a command option.
$ User Action:
$ Re-enter the option using one of the legal values.

9 File %1$s, line %2$lu
$ Explanation:
$ Identifies the filename and source line number
$ associated with an error.
$ User Action:
$ None. This is an informational message.

10 Name %1$s is declared in file %2$s, line %3$lu
$ Explanation:
$ Gives the source file and line
$ number of source text to help you diagnose the
$ previous error.
$ User Action:
$ None. This is an informational message.

11 Name is referenced in file %1$s, line %2$lu
$ Explanation:
$ Gives the source file and line
$ number of source text to help you diagnose the
$ previous error.
$ User Action:
$ None. This is an informational message.

12 %1$s

13 Options table:
$ Explanation:
$ Caption for options table printed in usage message
$ User Action:
$ None. This is an informational message.

14 Processing attribute configuration file %1$s

15 Running C preprocessor %1$s

16 Compiling stub module %1$s

17 Creating stub module %1$s

18 Deleting stub module %1$s

19 Type %1$s has a represent_as data type %2$s
$ Explanation:
$ IDL does not allow a data type that has a <kw>([represent_as]) type
$ to itself be used as a <kw>([represent_as]) type in another type
$ definition.
$ User Action:
$ The message gives
$ information to help you locate the problem.

20 Usage: idl filename [options]
$ Explanation:
$ Lists the format of the <kw>(idl) command
$ that invokes the IDL compiler.
$ User Action:
$ Use the <kw>(-confirm) option to see a list of all
$ to determine those you need.

21 DCE IDL compiler version %1$s
$ Explanation:
$ Identifies the current version of the IDL compiler.
$ User Action:
$ Use this version number when submitting bug reports.

22 Warning: Duplicate Protocol specification in endpoint list; \"%1$s\" ignored
$ Explanation:
$ Each protocol can be listed at most once in the endpoint list.  The specified 
$ endpoint specification duplicates another endpoint specification and is ignored.
$ User Action:
$ Use each protocol at most once in the endpoint list.

23 Warning: Syntax error in endpoint specification \"%1$s\"
$ Explanation:
$ The syntax of the string describing the endpoint must be of the
$ form <v>(protocol:[endpoint]). 
$ Although not in the expected format, the string <v>(endpoint) 
$ is assumed to be the desired endpoint specification
$ and is put into the generated stubs.
$ User Action:
$ Correct the syntax of the <kw>(endpoint) specification. 

24 Warning: Extraneous punctuation character \"%1$s\" ignored 
$ Explanation:
$ A punctuation character was found in a location
$ where it is not allowed in the IDL language syntax.
$ User Action:
$ If it is only an extra puncutation character, 
$ remove it.  If the character appears correct, then the
$ error may be caused by another nearby syntax error.
$ Correct the other syntax errors and recompile the interface.

25 Warning: The [handle] attribute of a parameter in a function pointer declaration is ignored

26 Warning: Identifier: %1$s too long; maximum is %2$lu chars
$ Explanation:
$ The length identifier exceeds the maximum number of characters
$ allowed by IDL for portability.  
$ User Action:
$ Shorten the identifier name to within the allowed limit.
$ Make sure the shortened identifier name is unique.

27 Warning: include filename must not contain a file extension
$ Explanation:
$ The filename specified in an <kw>(include) statement in the
$ Attribute Configuration File (ACF) must not contain a file
$ extension.  In a future IDL version, you will be able to
$ specify a programming language, and the IDL compiler will
$ automatically append the appropriate extension.
$ User Action:
$ Remove the file extension.  The current IDL compiler 
$ always assumes an <kw>(.h) extension.

28 Warning: ACF include statement advised for definition of type %1$s
$ Explanation:
$ An Attribute Configuration File (ACF) declares a type that is
$ not defined in an IDL file.  The type is referenced in the
$ generated stub code, thus its definition needs to be included
$ into the stub code as well.
$ User Action:
$ Place an <kw>(include) statement in the ACF to cause your module
$ that defines the type to be included into the generated
$ header file.

29 Warning: A size specifier is required; long assumed
$ Explanation:
$ A size specifier is required when specifying the <kw>(int) data type.
$ User Action:
$ Place a size specifier (<kw>(short, small, long,)
$ or <kw>(hyper)) before the <kw>(int) keyword.

30 Warning: The attributes [in_line] and [out_of_line] apply only to non-scalar types
$ Explanation:
$ Scalar data types are always marshalled in-line, since there
$ is performance degradation if they are 
$ marshalled out-of-line.
$ User Action:
$ Do not use the <kw>([in_line]) and <kw>([out_of_line]) attributes
$ on scalar types.

31 Warning: Missing pointer class for %1$s; [ptr], [ref], or [unique] required
$ Explanation:
$ A pointer declaration does not have pointer class attribute
$ and no <kw>([pointer_default]) attribute was  specified on the
$ interface.
$ User Action:
$ All pointers must have one of the pointer class attributes:
$ <kw>([ptr]), <kw>([ref]), or <kw>([unique]).  Either add one of these attributes
$ at the location of the pointer declaration, or add the 
$ <kw>([pointer_default]) attribute to the interface to specify the
$ class to be used as the default for the module.

32 Warning: The attributes [max_is,length_is] or [size_is,last_is] used together
$ Explanation:
$ The <kw>([max_is]) attribute specifies the array's upper bound while
$ the <kw>([size_is]) attribute specifies the total number of array
$ elements.  Similarly, <kw>([last_is]) specifies the upper data
$ limit while <kw>([length_is]) specifies the
$ number of valid data elements.
$ User Action:
$ Mixing the attributes can be incorrect.
$ Make sure the declaration is correct and change if necessary.

33 Warning: An attribute is defined multiple times
$ Explanation:
$ An attribute is repeated multiple times in an attribute list.
$ User Action:
$ Remove all but one occurrence of the offending attribute.

34 Warning: Generated name too long at line %1$lu.
$ Explanation:
$ The compiler generated an identifier that is too long
$ for some standard C compiler implementations.
$ User Action:
$ Shorten the name at the specified line.

35 Warning: At least one operation must have the [code] attribute
$ Explanation:
$ All of the operations in the interface are marked so 
$ no code is generated for them in the stub modules.
$ User Action:
$ If you used the ACF <kw>([nocode]) attribute on the interface,
$ be sure to specify <kw>([code]) on at least one operation.
$ To disable stub generation entirely use a command line option.

36 Warning: No endpoint can be found in the endpoint specification \"%1$s\"
$ Explanation:
$ The syntax specified in the <kw>(endpoint) specification string does not contain
$ an <kw>(endpoint) specification.  The entire string is assumed to be the protocol name
$ and the <kw>(endpoint) remains unspecified in the generated stubs.
$ User Action:
$ Correct the syntax of the <kw>(endpoint) specification.

37 Warning: Character constant cannot be portable across character sets
$ Explanation:
$ You used an integer value to specify a character. This
$ cannot be portable across different character sets
$ (ASCII and EBCDIC).
$ User Action:
$ Consider your portability requirements.

38 Warning: Semantic checking prevented by other reported errors
$ Explanation:
$ One or more of the reported errors prevented the compiler from
$ completing the semantic checking of the interface.  This 
$ can cause some semantic errors in the interface to go unreported.
$ User Action:
$ Correct the reported errors and compile the interface again.

39 Warning: Old UUID format must be replaced with new format:
$ Explanation:
$ The UUID specified in the interface attribute list is in an
$ old format.
$ User Action:
$ Although the old format is compatible with the new
$ format, we recommend that you switch to the new format.
$ If the input is an NCS Version 1 NIDL source file, process it with 
$ the translator utility (the <kw>(nidl_to_idl) command) that 
$ converts an NCS Version 1 NIDL source file into the 
$ format expected by the IDL compiler.  
$ You can also use the <kw>(uuidgen) command to convert the
$ old format UUID to the new format.
$ When you have the new format for the UUID,
$ compile the translated source file.  

40 Warning: Object file for %1$s placed in current working directory
$ Explanation:
$ A command option specifies a directory other than the current
$ working directory in which to place a generated C source and
$ object file.
$ The IDL compiler does not place the file in the requested
$ directory if you use the <kw>(-cc_cmd) argument on the command line.
$ Also, the IDL compiler does not place the file in the requested
$ directory on systems on which the mechanism it usually uses
$ for the placement does not work.
$ User Action:
$ IDL provides a command option that allows you to pass
$ command options to the C compiler.  Use the appropriate
$ C compiler option to assure that the object files are placed
$ in the correct directory.

41 Warning: The attribute [%1$s] applied on type with [%2$s] set, [%3$s] assumed

42 Warning: The attribute [nocode] does not apply to server stub
$ Explanation:
$ A server for an interface must support all of the routines in
$ that interface, therefore, the <kw>([nocode]) ACF attribute is not
$ valid when you use IDL to generate only server stub code.
$ User Action:
$ This is a warning, so no action is required.  If you want to
$ remove the warning message, create a new ACF for the
$ interface that does not use the <kw>([nocode]) attribute.  Or
$ change the command line used so stub generation is not
$ limited to the server stub.

43 Warning: File %1$s is a system-defined IDL filename
$ Explanation:
$ The file <v>(filename) has the same name as an RPC-supplied 
$ system IDL file.  This can cause unexpected compilation errors,
$ such as missing declarations of IDL predefined types.
$ User Action:
$ Rename <v>(filename) so it does not conflict with any
$ of the system-defined IDL filenames.  

44 Pipes must be defined with a typedef
$ Explanation:
$ The IDL compiler constructs routine names
$ that are referenced by generated stub code
$ from the name of any <kw>(pipe) data type.
$ Therefore the type cannot be anonymous.
$ User Action:
$ Declare the <kw>(pipe) data type with a <kw>(typedef)
$ so the <kw>(pipe) has a name associated with it.

45 Declaration of %1$s cannot contain an anonymous type
$ Explanation:
$ The type specification for the parameter, field or <kw>(union) arm 
$ <v>(name) contains a data type that is an anonymous <kw>(struct) or
$ <kw>(union).  Since such a type is unique and not compatible with any other type,
$ the generated stubs cannot generate code to access the type.
$ User Action:
$ Move the <kw>(struct) or <kw>(union) declaration into a <kw>(typedef)  and modify the 
$ declaration of <v>(name) to use the newly created type name.

46 An arm of a union cannot be or contain a [ref] pointer
$ Explanation:
$ IDL-generated server stub code must allocate storage for
$ objects pointed to by <kw>([ref]) pointers.  IDL does not allow
$ a <kw>([ref]) pointer within a <kw>(union) because the valid
$ arm of the <kw>(union), and therefore the
$ object for which storage is allocated, is not
$ known at compile time.
$ User Action:
$ Change the pointer within the <kw>(union) declaration to a full pointer.

47 Array elements cannot be conformant arrays or conformant structures
$ Explanation:
$ Array elements must be of fixed size.
$ User Action:
$ If the element type is an array, make sure that the array has
$ fixed bounds.  If the element type is a structure, make sure
$ that any arrays in the structure have fixed bounds.

48 Array elements cannot be context handles
$ Explanation:
$ Arrays of context handles are not allowed.
$ User Action:
$ Change the definition of the array so its elements are
$ not context handles.  If you want to declare
$ an array of context handles, you must use alternative means.

49 Array elements cannot be pipes
$ Explanation:
$ Array elements cannot be pipes.
$ User Action:
$ Change the definition of the array so its elements are
$ not of a <kw>(pipe) data type.  If you want to declare
$ an array of pipes, you must use alternative means.

50 Array size information required for %1$s
$ Explanation:
$ The IDL code uses an array with bounds that are not fixed without
$ specifying the appropriate size attributes.
$ User Action:
$ Modify the array declaration to include the appropriate
$ <kw>([max_is]) or <kw>([size_is]) attribute.  If the array
$ is a parameter, an additional parameter is used to
$ specify size information.  If the array is a field
$ in a structure, an additional field is used to
$ specify size information.

51 Arrays can be conformant in the first dimension only
$ Explanation:
$ IDL only allows an array to be conformant in its first dimension.
$ User Action:
$ Modify the array declaration so dimensions other than
$ the first have fixed bounds.

52 An array with a pointer attribute is valid only as a parameter
$ Explanation:
$ An array that is a parameter in an operation can have a pointer
$ attribute because arrays are implicitly passed by reference
$ pointer.  Otherwise, an array cannot have a pointer attribute.
$ User Action:
$ Remove the pointer attribute from the parameter or type
$ definition, or define a new type without a pointer attribute.

53 Use array syntax to declare multidimensional arrays
$ Explanation:
$ An IDL declaration attempts to use mixed pointer (<kw>(*)) and
$ array (<kw>([])) syntax to define a multidimensional array.
$ The interpretation that an asterisk (<kw>(*)) is used to represent an array
$ is due to the presence of one or more of the 
$ <kw>([max_is]), or <kw>([size_is]) attributes.  It is
$ ambiguous whether size attributes apply to the pointer or the
$ array, thus IDL does not allow mixing pointer and array syntax
$ when size attributes are present.
$ User Action:
$ The IDL compiler requires that you make such declarations
$ using only array <kw>([]) syntax.  If you do not want to
$ declare a multidimensional array, you used the 
$ <kw>([max_is]), or <kw>([size_is]) attribute incorrectly.  They apply
$ only to arrays with bounds that are not fixed, and the declared
$ array has fixed bounds.

54 Arrays can be varying in the first dimension only
$ Explanation:
$ IDL only allows an array to be varying in its first dimension.
$ User Action:
$ Remove the <kw>([first_is]), <kw>([last_is]), or <kw>([length_is])
$ attributes that refer to dimensions other than the first.

55 Arrays with [transmit_as] cannot be conformant or varying
$ Explanation:
$ IDL does not allow a conformant or varying array with the
$ <kw>([transmit_as]) attribute.
$ User Action:
$ Change the declaration so the array is of fixed size and
$ has no data limit attributes, or remove the <kw>([transmit_as]) attribute.

56 The attribute is no longer allowed in this context; use translator
$ Explanation:
$ An attribute list is encountered in an invalid context.  
$ In NCS Version 1 NIDL, attributes are allowed in this position.  
$ You may be compiling an NCS Version 1 NIDL source file. 
$ User Action:
$ If the input is an NCS Version 1 NIDL source file, process it with the 
$ <kw>(nidl_to_idl) translator utility that  
$ converts an NCS Version 1 NIDL source file into the format expected by
$ the IDL compiler.  Then compile the translated source file.  
$ Otherwise, move the attributes to a valid location.

57 An attribute variable cannot be an indirect field reference
$ Explanation:
$ An indirect field reference is used in a field attribute;
$ indirection is not allowed in this context.
$ User Action:
$ Remove the indirection.  For example, if the referenced field was
$ <kw>(long *sp;) and the attribute expression was
$ <kw>([size_is(*sp)]), then change the field to
$ <kw>(long s;) and the attribute expression to
$ <kw>([size_is(s)]).

58 Invalid use of tag %1$s
$ Explanation:
$ You used a previously defined tag name in a declaration of a different type.
$ Tag names can be used for a <kw>(struct) or <kw>(union) declaration, but not both.
$ User Action:
$ Either make sure that each use of the tag name specifies the same type, or
$ use different tag names with each type.

59 The attribute [broadcast] is not valid on an operation with pipes
$ Explanation:
$ IDL does not allow pipes to be used in <kw>([broadcast])
$ operations.
$ User Action:
$ Remove the <kw>([broadcast]) attribute from the operation,
$ or remove the <kw>(pipe) parameter from the operation.

60 Case label type does not agree with discriminator type
$ Explanation:
$ The value of a <kw>(case) expression in a discriminated <kw>(union)
$ is not the same data type as the discriminator variable
$ in the <kw>(switch) clause.
$ User Action:
$ Change the discriminator declaration or the <kw>(case) expression
$ so the data types match.

61 Case label must be a constant from the enumeration of the discriminant
$ Explanation:
$ The value of a <kw>(case) expression in a discriminated <kw>(union)
$ is from a different enumeration data type than the enumeration
$ data type of the discriminator variable in the <kw>(switch) clause.
$ User Action:
$ Change the discriminator declaration or the <kw>(case) expression
$ so the enumeration data types match.

62 Structures containing conformant arrays must be passed by reference
$ Explanation:
$ The size of a structure that contains a conformant array is
$ not a compile-time constant.  It is not possible to pass such
$ a structure by value.
$ User Action:
$ Change the parameter declaration to pass the structure by
$ reference by adding an asterisk (<kw>(*)) to the left of the parameter name.

63 The base type of a pipe cannot be a conformant type
$ Explanation:
$ IDL does not allow pipes of any data type of size that is not fixed.
$ User Action:
$ Change your <kw>(pipe) type definition to use a fixed array or
$ some other construct of fixed size.

64 A conformant field must be the last field in the structure
$ Explanation:
$ The IDL compiler restricts arrays with bounds that are not fixed
$ and that occur in structures to only the last field
$ in the structure.
$ User Action:
$ If the structure contains more than one
$ conformant array, remove all but one of them.
$ Move the conformant array so it is the last field in the
$ structure definition.  

65 Conformant array or structure is invalid within a union
$ Explanation:
$ The IDL compiler does not allow an array with bounds  that are not fixed
$ in a discriminated <kw>(union).  Each member of a <kw>(union) must
$ be of fixed size.
$ User Action:
$ It is possible to have a <kw>(union) case that is a full <kw>([ptr])
$ pointer to a conformant array or conformant structure.
$ Change the declaration and
$ associated code accordingly.

66 Interface attributes [auto_handle] and [implicit_handle] cannot occur together
$ Explanation:
$ The <kw>([auto_handle]) and <kw>([implicit_handle]) attributes
$ are two distinct
$ mechanisms for managing the binding between client and server
$ at runtime.  You cannot specify both of these attributes.
$ User Action:
$ See the documentation to determine which binding mechanism
$ to use.  Then remove one of the conflicting attributes
$ from the interface attribute list in the Attribute
$ Configuration File (ACF).

67 The attributes [in_line] and [out_of_line] cannot occur together

68 The attributes [%1$s] and [%2$s] cannot occur together
$ Explanation:
$ The specified attributes are conflicting and only one or the other
$ can be specified.
$ User Action:
$ Remove one of the attributes.

69 The [represent_as] type %1$s conflicts with previously defined %2$s
$ Explanation:
$ The attribute configuration file (ACF) contains conflicting
$ <kw>([represent_as]) clauses for the same data type.
$ User Action:
$ Remove one of the <kw>([represent_as]) clauses for that data type.

70 Constant name: %1$s not found
$ Explanation:
$ The named <kw>(constant) is not defined.
$ User Action:
$ Modify the interface and specify a known constant.

71 Constant type does not agree with constant expression
$ Explanation:
$ You used a constant expression that is inappropriate
$ for the constant type.
$ User Action:
$ Modify the constant definition so the expression
$ matches the type.

72 A hyper constant type is not allowed
$ Explanation:
$ <kw>(hyper) constant is not allowed.
$ User Action:
$ Modify the <kw>(constant) type to one of the supported types:
$ <kw>(long, char, boolean, void *,) or <kw>(char *).

73 Missing \"}\" on interface declaration
$ Explanation:
$ The closing brace on the interface declaration was not found.
$ It may have been omitted, the nesting of other braces may be incorrect, or
$ some other syntax error may cause IDL to overlook a brace.
$ User Action:
$ Add the closing brace if missing, otherwise make sure all other braces
$ are specified in pairs, and there are no other syntax errors.

74 Missing \"]\" on attribute list
$ Explanation:
$ The closing bracket on an attribute list was not found.
$ It may have been omitted, or
$ some other syntax error may cause IDL to overlook the bracket.
$ User Action:
$ Add the closing bracket if it is missing, otherwise make sure
$ there are no other syntax errors.

75 Missing \"]\" on array bound
$ Explanation:
$ The closing bracket on an array bound specification was not found.
$ It may have been omitted, or
$ some other syntax error may cause IDL to overlook the bracket.
$ User Action:
$ Add the closing bracket if it is missing, otherwise make sure
$ there are no other syntax errors.

76 Missing result type on operation declaration
$ Explanation:
$ No data type was found for the result of an operation
$ while processing what appeared to be an
$ operation declaration.  All operations must have an explicit result type.
$ User Action:
$ If the result type of an operation is omitted, explicitly 
$ specify it. If no result is returned from the operation,
$ specify <kw>(void) as the result type.
$ This error can also occur due to a previous syntax error.

77 Missing type on parameter declaration
$ Explanation:
$ No data type is found for the parameter.
$ User Action:
$ If the parameter type is omitted, explicitly specify it.
$ This error can also occur due to a previous syntax error.

78 The base type of a pipe cannot be a [context_handle] type
$ Explanation:
$ A <kw>([context_handle]) type cannot be used as the base type
$ of a <kw>(pipe).
$ User Action:
$ Remove the invalid declaration, or change it so the
$ base type of the <kw>(pipe) is not a <kw>([context_handle]) type.

79 Attribute [context_handle] only applies to void * types
$ Explanation:
$ The attribute <kw>([context_handle]) indicates that the data is a
$ pointer-sized object that serves as the handle, or method
$ of accessing, some object.  To the called operation, the data is often
$ an address of a structure in memory, although it need not be.
$ A context handle is opaque to, and must never be written by,
$ the caller code.  To emphasize this, IDL requires that
$ context handles are defined as type <kw>(void *).
$ User Action:
$ Change the declaration with the <kw>([context_handle]) attribute
$ to data type <kw>(void *).

80 Context handles are not valid as structure fields
$ Explanation:
$ A context handle is not allowed as a field of a structure.
$ User Action:
$ Pass a context handle as a separate parameter to an operation,
$ rather than embedding it as a field of a structure.

81 Context handle is not valid as a member of a union
$ Explanation:
$ A context handle cannot be a member of a <kw>(union).
$ User Action:
$ Pass a context handle as a separate parameter to an operation,
$ rather than embedding it as a member of a <kw>(union).

82 Definition of tag %1$s is not yet complete
$ Explanation:
$ The tag <v>(name) is forward referenced in a context that requires it to be
$ completely defined.  References to <kw>(struct) or <kw>(union) types by means of tag 
$ name before the <kw>(struct) or <kw>(union) is completely defined is only allowed in
$ contexts in which the size is not needed (a pointer to the type or
$ in a <kw>(typedef)).  Although IDL can process such references, the resulting
$ stub cannot be compiled by most C compilers.
$ User Action:
$ Move the declaration of the <kw>(struct) or <kw>(union) prior to this
$ reference. 

83 Duplicate case label value
$ Explanation:
$ A discriminated <kw>(union) contains more than one <kw>(case) label
$ with the same value.  Each <kw>(case) label value in a discriminated
$ <kw>(union) can be used at most once.
$ User Action:
$ Remove one of the conflicting <kw>(case) labels.

84 Unexpected end-of-file
$ Explanation:
$ The end of the source file was encountered before the end of the interface definition.
$ The source file may be incomplete.  
$ User Action:
$ Complete the interface definition.
$ 

85 Unexpected end-of-file near '%2$.*1$s'
$ Explanation:
$ The end of the source file was encountered before the end of the interface definition.
$ The source file may be incomplete.  
$ User Action:
$ Complete the interface definition.

86 Syntax error in attribute list
$ Explanation:
$ The syntax of the attribute list is not correct.  It must contain a list of
$ attributes separated by commas.
$ User Action:
$ Make sure the attribute list contains only valid attribute names and values, that
$ their spellings are correct, and that they form a valid list.

87 Data type error_status_t must be defined in nbase.idl
$ Explanation:
$ You specified an additional <kw>([comm_status])
$ or <kw>([fault_status]) parameter
$ for an operation in the Attribute Configuration File (ACF).
$ This requires that data type <kw>(error_status_t) be defined.
$ User Action:
$ Usually, <kw>(error_status_t) is a predefined type in the
$ RPC-supplied imported interface <kw>(nbase.idl).  Make sure
$ you are accessing the RPC-supplied <kw>(nbase.idl).

88 File is not a directory: %1$s
$ Explanation:
$ A file that is not a directory is specified in a context
$ where a directory is required; for example, as the command
$ line option that specifies an output directory.
$ User Action:
$ Specify a filename that is a directory.

89 File %1$s not found
$ Explanation:
$ A file, specified by the user either in the IDL source or
$ ACF file, does not exist.
$ User Action:
$ Check the filenames you specified.  Determine
$ the location of any imported or included files that are needed to
$ compile the IDL source file specified. If any are not in the
$ directories that the IDL compiler searches by default, you
$ must use a command line option to specify additional
$ directories to search.

90 The [first_is] parameter must have the [in] attribute
$ Explanation:
$ The <kw>([first_is]) attribute specifies a parameter that contains
$ the lower data limit of a varying array.  Since the array has the
$ <kw>([in]) attribute, the lower data limit parameter must also be <kw>([in])
$ so the number of array elements to send from client to
$ server is known.
$ User Action:
$ Change the lower data limit parameter referenced by the
$ <kw>([first_is]) clause to have the <kw>([in]) attribute.

91 A [first_is] variable must be a small, short, or long integer
$ Explanation:
$ The <kw>([first_is]) attribute specifies a field or parameter that contains
$ the lower data limit of a varying array.  Array data limits must be
$ nonhyper integers.
$ User Action:
$ Change the data limit field or parameter referenced by the
$ <kw>([first_is]) clause to integer data type.

92 Floating point constants not supported
$ Explanation:
$ Floating point constants cannot be specified in IDL.
$ User Action:
$ Remove the <kw>(float) constant specification.

93 Function pointers are not valid as elements of conformant arrays

94 Function pointers are allowed only in local interfaces

95 Function pointer parameters cannot be of type handle_t

96 Function pointers in an operation must be [in] parameters

97 The base type of a pipe cannot be a function pointer

98 Function pointers are not valid as structure fields

99 Function pointers are not valid as members of unions

100 Function types are allowed only in local interfaces

101 Array elements cannot be of type handle_t
$ Explanation:
$ Data type <kw>(handle_t) is only meaningful when used as the data
$ type of the first parameter in an operation.  It is used to
$ establish a binding to a server of the interface.
$ User Action:
$ Do not declare arrays of type <kw>(handle_t).

102 A [handle] binding parameter must be [in] or [in,out]
$ Explanation:
$ A parameter of a data type with the <kw>([handle]) attribute
$ as the first parameter
$ in an operation is used to
$ establish a binding to a server of the interface.
$ It must be an <kw>([in]) or <kw>([in,out]) parameter.
$ User Action:
$ Place the <kw>([in]) attribute on the <kw>([handle]) parameter.

103 Pointers to type handle_t are valid only in parameter declarations
$ Explanation:
$ Data type <kw>(handle_t) is only meaningful when used as the data
$ type of the first parameter in an operation.  It is used to
$ establish a binding to a server of the interface.
$ User Action:
$ Do not declare a pointer to data type <kw>(handle_t), except as a parameter.

104 A handle_t as other than the first parameter requires a [transmit_as]
$ Explanation:
$ Data type <kw>(handle_t) is only meaningful when used as the data
$ type of the first parameter in an operation.  A bound handle
$ is used to deliver the call to the correct server of the
$ interface.  A bound <kw>(handle_t) parameter itself
$ is not transmittable.  When the <kw>(handle_t) type is used
$ in other than the first parameter, it must have a
$ <kw>([transmit_as]) clause to convert it to a transmittable type.
$ User Action:
$ Use data type <kw>(handle_t) only as the data
$ type of the first parameter in an operation,
$ unless the defined data type has a <kw>([transmit_as]) clause.

105 The base type of a pipe cannot be handle_t
$ Explanation:
$ Data type <kw>(handle_t) is only meaningful when used as the data
$ type of the first parameter in an operation.  It is used to
$ establish a binding to a server of the interface.
$ User Action:
$ Do not declare pipes whose base type is <kw>(handle_t).

106 A handle_t binding parameter must be an [in] parameter
$ Explanation:
$ A parameter of type <kw>(handle_t) as the first parameter
$ in an operation 
$ establishes a binding to a server of the interface.
$ It must be an <kw>([in]) parameter only.
$ User Action:
$ Remove the <kw>([out]) attribute from the <kw>(handle_t) parameter.

107 Structure fields cannot be of type handle_t
$ Explanation:
$ Data type <kw>(handle_t) is only meaningful when used as the data
$ type of the first parameter in an operation.  It is used to
$ establish a binding to a server of the interface.
$ User Action:
$ Do not declare structure fields of data type <kw>(handle_t).

108 Members of unions cannot be of type handle_t
$ Explanation:
$ Data type <kw>(handle_t) is only meaningful when used as the data
$ type of the first parameter in an operation.  It is used to
$ establish a binding to a server of the interface.
$ User Action:
$ Do not declare <kw>(union) members of data type <kw>(handle_t).

109 A handle_t first parameter must not have [transmit_as] type
$ Explanation:
$ A parameter of type <kw>(handle_t), when used as the data
$ type of the first parameter in an operation, is used to
$ establish a binding to a server of the interface.
$ It is not sent over the wire, therefore it is incorrect
$ for it to have a <kw>([transmit_as]) data type.
$ User Action:
$ Remove the <kw>([transmit_as]) clause from the type's declaration.

110 The attribute [idempotent] is not valid on an operation with pipes
$ Explanation:
$ An operation that uses pipes cannot be idempotent, because the
$ <kw>(pipe) data stream can break at unpredictable times.
$ User Action:
$ Remove the <kw>([idempotent]) attribute from the operation,
$ or remove the <kw>(pipe) parameter from the operation.

111 The attribute [ignore] on array elements is not allowed
$ Explanation:
$ The <kw>([ignore]) attribute is not allowed on array elements
$ User Action:
$ Remove the <kw>([ignore]) attribute from the relevant declaration.

112 The attribute [ignore] is valid only for pointers
$ Explanation:
$ The <kw>([ignore]) attribute is valid only for pointers.
$ User Action:
$ Remove the <kw>([ignore]) attribute from the declaration.

113 The attribute [%1$s] on a field is not allowed
$ Explanation:
$ The specified attribute is not valid on a field.
$ User Action:
$ Remove the attribute.

114 The attribute [%1$s] on a parameter is not allowed
$ Explanation:
$ The specified attribute is not valid on a parameter declaration.
$ User Action:
$ Remove the attribute.

115 The attribute [%1$s] on a type is not allowed
$ Explanation:
$ The specified attribute is not valid on a type declaration.
$ User Action:
$ Remove the attribute.

116 The attribute [%1$s] on an operation is not allowed
$ Explanation:
$ The specified attribute is not valid on an operation.
$ User Action:
$ Remove the attribute.

117 The attribute [%1$s] on an interface is not allowed
$ Explanation:
$ The specified attribute is not valid on a interface declaration.
$ User Action:
$ Remove the attribute.

118 The attribute [%1$s] on a union member is not allowed
$ Explanation:
$ The specified attribute is not valid on a <kw>(union) member declaration.
$ User Action:
$ Remove the attribute.

119 An [implicit_handle] variable must either be of type handle_t or have the handle attribute
$ Explanation:
$ The <kw>([implicit_handle]) variable declared in the ACF must be a
$ handle, either a primitive handle of data type <kw>(handle_t);
$ or a customized handle, a data type with the <kw>([handle]) attribute.
$ User Action:
$ Specify a valid handle data type for the <kw>([implicit_handle]).

120 A nonlocal interface cannot import a local interface
$ Explanation:
$ The <kw>([local]) interface attribute implies that the interface
$ is not part of an RPC application, but used only to generate
$ header files.  This causes IDL to suppress any 
$ errors specific to RPC that the interface uses as
$ part of an RPC application.
$ User Action:
$ Remove the <kw>([local]) attribute from the imported interface
$ definition.  The imported interface does not need a UUID
$ unless the interface defines an operation and you compile it
$ independently.

121 V1 attributes are incompatible with this type
$ Explanation:
$ The version 1 migration attributes are provided for compatibility for
$ applications that were originally written with NCS Version 1 NIDL.
$ These attributes are <kw>([v1_string]), <kw>([v1_array]), <kw>([v1_enum]),
$ and <kw>([v1_struct]).  In later versions of IDL, strings,
$ conformant and varying arrays, enumerations,
$ and structures are enhanced at
$ the network representation level.  A data type with any of
$ the V1 migration attributes cannot contain one of these data types
$ that does not have the corresponding V1 attribute.
$ User Action:
$ Change the data type declaration so all relevant
$ contained types have the corresponding V1 attribute, or remove
$ V1 attributes if compatibility with NCS Version 1 NIDL is unnecessary.

122 Interface attributes [code] and [nocode] cannot occur together
$ Explanation:
$ The <kw>([code]) or <kw>([nocode]) attribute on an interface
$ establishes the default for operations in the interface.
$ You can specify only one of these attributes.
$ User Action:
$ Remove one of the conflicting attributes.

123 Integer division by zero
$ Explanation:
$ The evaluation of an integer constant expression resulted
$ in an integer divided by zero.
$ User Action:
$ Correct the expression so a division by zero does
$ not occur.

124 Interface attributes [in_line] and [out_of_line] cannot occur together
$ Explanation:
$ The <kw>([in_line]) or <kw>([out_of_line]) attribute on an interface
$ establishes the default for types in the interface.
$ You can specify only one of these attributes.
$ User Action:
$ Remove one of the conflicting attributes.

125 ACF interface name %1$s differs from IDL interface name %2$s
$ Explanation:
$ The interface name in an interface's Attribute Configuration
$ File must be identical to the interface name in the IDL source.
$ User Action:
$ Change the ACF to use the correct interface name.

126 Integer value overflows %1$s
$ Explanation:
$ The integer value specified is too large to be contained in
$ the destination of size <v>(size).
$ User Action:
$ Either reduce the size of the integer value, or increase the
$ size of the destination declaration.

127 Integer constant %1$s is invalid
$ Explanation:
$ The integer contains incorrect characters.
$ User Action:
$ Correct the specification of the integer.

128 Interface UUID must be specified
$ Explanation:
$ The interface does not have the <kw>([uuid]) attribute required for
$ the complete definition of the interface.
$ User Action:
$ Either add the <kw>([local]) attribute to the interface, or generate
$ a UUID with the <kw>(uuidgen) utility supplied with IDL and specify
$ the UUID value in the interface <kw>([uuid]) attribute.

129 Invalid array bound type
$ Explanation:
$ Array indices must be integers.
$ User Action:
$ Change the array declaration so the fixed bounds are integer
$ constants.

130 Invalid case label type
$ Explanation:
$ The values in <kw>(case) clauses in discriminated <kw>(union)s must be
$ integer, enumeration, boolean, or character values that match
$ the data type of the <kw>(union) discriminator.
$ User Action:
$ Modify the invalid clause to use a value of the correct
$ data type.

131 Invalid character literal
$ Explanation:
$ The specification of the character literal is invalid.
$ User Action:
$ Character literals can be specified as a single printing
$ character or one of the following escape sequeces:
$ <literal>(\)n, <literal>(\)t, <literal>(\)v, <literal>(\)b, 
$ <literal>(\)r, <literal>(\)f, <literal>(\)a, <literal>(\)<literal>(\), <literal>(\)?, <literal>(\)',
$ <literal>(\)", <literal>(\)<v>(ooo), <literal>(\)x<v>(hh) 
$ (where <v>(ooo) are octal digits, and 
$ <v>(hh) are hexadecimal digits).

132 Invalid octal digit in %1$s
$ Explanation:
$ An octal integer contains a digit that is not valid.
$ Integer constants with a leading zero are interpreted as
$ octal values.  Only the digits 0 through 7 are valid in octal value.
$ User Action:
$ If the integer is not intended to be specified in octal, remove
$ the leading zeros. Otherwise correct the value to contain only
$ octal digits.

133 Could not invoke the C preprocessor
$ Explanation:
$ The IDL compiler is unable to invoke the C preprocessor
$ to preprocess the IDL file before compiling it.
$ User Action:
$ If you do not need the C preprocessor, you can use a command
$ line option to prevent the
$ IDL compiler from calling it.  If you specified
$ the C preprocessor to invoke with a command line option,
$ check to make sure you gave the correct file specification.
$ If you do not specify a C preprocessor, the IDL compiler
$ looks in a default directory for it.  The C
$ preprocessor may not be installed in the expected directory.

134 The attribute [out_of_line] is not allowed on parameters
$ Explanation:
$ IDL does not allow the <kw>([out_of_line])
$ attribute on parameters.
$ User Action:
$ Apply the <kw>([out_of_line]) attribute to the type definition
$ rather than the parameter.  If you do not want all
$ instances of the type <kw>([out_of_line]), define two separate
$ types, one of which has the <kw>([out_of_line]) attribute.

135 Command option -%1$s %2$s is not valid
$ Explanation:
$ The <v>(option-value) specified is not valid for the option
$ with which it is used.
$ User Action:
$ See the documentation for the correct command line syntax.

136 Invalid parameters on command line:
$ Explanation:
$ There are extra parameters on the command line.
$ User Action:
$ You may have attempted to use a value on a option that does
$ not take a value, or attempted to compile more than one
$ interface at once.
$ See the documentation for the proper command line syntax.

137 Pointers to context handles are valid only in parameter declarations
$ Explanation:
$ IDL does not allow pointers to context handles,
$ except in parameter declarations.
$ User Action:
$ Do not declare a pointer to a <kw>(context_handle), except as a parameter.

138 Pointers to pipes are valid only in parameter declarations
$ Explanation:
$ IDL does not allow a pointer to a <kw>(pipe),
$ except in a parameter declaration.
$ User Action:
$ Do not declare a pointer to a <kw>(pipe), except as a parameter.

139 The [last_is] parameter must have the [in] attribute
$ Explanation:
$ The <kw>([last_is]) attribute specifies a parameter that contains
$ the upper data limit of a varying array.  Since the array has the
$ <kw>([in]) attribute, the upper data limit parameter must also be <kw>([in])
$ so the number of array elements to send from client to
$ server is known.
$ User Action:
$ Change the data limit parameter referenced by the
$ <kw>([last_is]) clause to have the <kw>([in]) attribute.

140 The attributes [last_is] and [length_is] cannot occur together
$ Explanation:
$ The <kw>([last_is]) and <kw>([length_is]) attributes both specify a
$ field or parameter used to determine the 
$ data limit of a varying array.
$ You cannot use both.
$ User Action:
$ The <kw>([last_is]) attribute specifies the index of the last valid element.
$ The <kw>([length_is]) attribute specifies the total number of elements in the
$ array. Remove one of these attributes.

141 A [last_is] variable must be a small, short, or long integer
$ Explanation:
$ The <kw>([last_is]) attribute specifies a field or parameter that contains
$ the upper data limit of a varying array.  Array data limits must be
$ nonhyper integers.
$ User Action:
$ Change the upper data limit field or parameter referenced by the
$ <kw>([last_is]) clause to be of integer data type.

142 The lower bound must not be greater than the upper bound
$ Explanation:
$ The syntax of an array requires the lower bound to
$ precede the upper bound.
$ User Action:
$ Modify the array declaration so the lower bound is first.

143 The [length_is] parameter must have the [in] attribute
$ Explanation:
$ The <kw>([length_is]) attribute specifies a parameter used
$ to determine the data length of a varying array.
$ Since the array has the
$ <kw>([in]) attribute, the <kw>(length_is) parameter must also be <kw>([in])
$ so the number of array elements to send from client to
$ server is known.
$ User Action:
$ Change the parameter referenced by the
$ <kw>([length_is]) clause to have the <kw>([in]) attribute.

144 A [length_is] variable must be a small, short, or long integer
$ Explanation:
$ The <kw>([length_is]) attribute specifies a field or parameter that contains
$ the data length of a varying array.  The field or parameter must be an
$ nonhyper integer.
$ User Action:
$ Change the field or parameter referenced by the
$ <kw>([length_is]) clause to be of integer data type.

145 The major version number is too large; the maximum is %1$lu
$ Explanation:
$ The major version number is too large.
$ User Action:
$ Replace the major version number (the portion
$ to the left of the period) with a value
$ less than <v>(number).

146 The attribute [max_is] cannot be applied to dimension %1$lu; upper bound is not dynamic
$ Explanation:
$ A <kw>([max_is]) variable is only valid for array dimensions that
$ have an upper bound that is not fixed.
$ User Action:
$ If the array is multidimensional, you may have specified the
$ <kw>([max_is]) variable for the wrong dimension.  See the
$ documentation for the proper syntax.  If the array has only
$ one dimension, the <kw>([max_is]) clause is invalid, since the
$ array's upper bound is fixed.  You may have meant to use a
$ <kw>([last_is]) clause.  See the documentation for the
$ distinctions between conformant and varying arrays
$ and their attributes.

147 Maximum identifier length for interface name is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the interface name.
$ Therefore, the number of characters allowed
$ in an interface name is
$ less than for other identifiers.
$ User Action:
$ Shorten the interface name.

148 Maximum identifier length for type with [transmit_as] is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the names of any <kw>([transmit_as]) data types.
$ Therefore, the number of characters allowed
$ in such a data type is
$ less than for other identifiers.
$ User Action:
$ Shorten the data type name.

149 Maximum identifier length for [handle] type is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the names of any <kw>([handle]) data types.
$ Therefore, the number of characters allowed
$ in such a data type is
$ less than for other identifiers.
$ User Action:
$ Shorten the data type name.

150 Maximum identifier length for [context_handle] type is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the names of any <kw>([context_handle]) data types.
$ Therefore, the number of characters allowed
$ in such a data type is
$ less than for other identifiers.
$ User Action:
$ Shorten the data type name.

151 Maximum identifier length for pointed-to type is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the names of any pointed-to data types.
$ Therefore, the number of characters allowed
$ in such a data type is
$ less than for other identifiers.
$ User Action:
$ Shorten the data type name.

152 Maximum identifier length for pipe type is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the names of any <kw>(pipe) data types.
$ Therefore, the number of characters allowed
$ in such a data type is
$ less than for other identifiers.
$ User Action:
$ Shorten the data type name.

153 Maximum identifier length for [represent_as] type is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the names of any <kw>([represent_as]) data types.
$ Therefore, the number of characters allowed
$ in such a data type is
$ less than for other identifiers.
$ User Action:
$ Shorten the data type name.

154 Maximum identifier length for [out_of_line] type is %1$lu characters
$ Explanation:
$ The IDL compiler constructs new identifier names
$ that are referenced by generated stub code
$ from the names of any <kw>([out_of_line]) data types.
$ Therefore, the number of characters allowed
$ in such a data type is
$ less than for other identifiers.
$ User Action:
$ Shorten the data type name.

155 A [max_is] parameter must have the [in] attribute
$ Explanation:
$ The <kw>([max_is]) attribute specifies a parameter that contains
$ the upper bound of a conformant array.  This must be an <kw>([in])
$ attribute so the server stub code knows how much
$ space to allocate for the array.
$ User Action:
$ Change the upper bound parameter referenced by the
$ <kw>([max_is]) clause to have the <kw>([in]) attribute.

156 The attribute [max_is] or [size_is] is required
$ Explanation:
$ An array with an upper bound that is not fixed is used as a field of
$ a structure or as a parameter of an operation.  You must
$ specify a field or parameter that determines
$ the allocation of the array at runtime.
$ User Action:
$ Use a <kw>([max_is]) or <kw>([size_is]) attribute to specify
$ the field or parameter that contains the size information.

157 The attributes [max_is] and [size_is] cannot occur together
$ Explanation:
$ The <kw>([max_is]) and <kw>([size_is]) attributes both specify a
$ field or parameter used to determine the 
$ upper bound of a conformant array.
$ You cannot use both.
$ User Action:
$ The <kw>([max_is]) attribute specifies the index of the last possible array element.
$ The <kw>([size_is]) attribute specifies the total number of possible elements in the
$ array. Remove one of the attributes.

158 A [max_is] variable must be a small, short, or long integer
$ Explanation:
$ The <kw>([max_is]) attribute specifies a field or parameter that contains
$ the upper bound of a conformant array.  Array bounds must be
$ nonhyper integers.
$ User Action:
$ Change the upper bound field or parameter referenced by the
$ <kw>([max_is]) clause to be of integer data type.

159 A [maybe] operation cannot have [out] parameters or a function result
$ Explanation:
$ The <kw>([maybe]) attribute specifies that the operation's caller
$ does not require and does not receive a response or fault
$ indication.  Do not use an <kw>([out])
$ parameter or a function result in a <kw>([maybe]) operation.
$ User Action:
$ Remove the <kw>([maybe]) attribute from the operation,
$ or remove all <kw>([out]) parameters from the function and
$ declare its result type as <kw>(void).

160 The attribute [min_is] is required

161 The attribute [min_is] cannot be applied to dimension %1$lu; lower bound is not dynamic

162 A [min_is] parameter must have the [in] attribute

163 The minor version number is too large; the maximum is %1$lu
$ Explanation:
$ The minor version number is too large.
$ User Action:
$ Replace the minor version number (the portion
$ to the right of the period) with a value
$ less than <v>(number).

164 A [min_is] variable must be a small, short, or long integer

165 Name already declared: %1$s
$ Explanation:
$ The name referenced is already declared.
$ User Action:
$ Modify the interface and select a unique name 
$ for the given item.

166 Name is not a constant: %1$s
$ Explanation:
$ The name used to define a new constant is not the name of
$ a <kw>(constant).
$ User Action:
$ Modify the <kw>(constant) definition to use
$ a predefined <kw>(constant) name.

167 Name is not a field: %1$s
$ Explanation:
$ The name referenced in an array attribute is not
$ declared as a field in the structure being defined.
$ User Action:
$ Declare a field in the structure that can be used 
$ in the array attribute.

168 Name not found: %1$s
$ Explanation:
$ The referenced name is not defined.
$ User Action:
$ Modify the interface to define an appropriate type for the
$ specified name.

169 Name is not a parameter: %1$s
$ Explanation:
$ The name referenced in an array attribute is not
$ declared as a parameter in the operation being defined.
$ User Action:
$ Declare a parameter in the operation that can be used 
$ in the array attribute.

170 Name is not a type: %1$s
$ Explanation:
$ The name referenced is not a type definition.
$ User Action:
$ Modify the interface to specify a unique type specification
$ instead of the name referenced.

171 Name %1$s previously declared in file %2$s, line %3$lu
$ Explanation:
$ The name referenced was previously declared at the specified location.
$ User Action:
$ Remove this declaration, if it is redundant, or change the spelling
$ so it does not conflict with the existing name.

172 %1$sNLS message catalog version mismatch in \"%2$s\", Expected: \"%3$lu\", Actual: \"%4$s\"

173 %1$sError messages may be incorrect
$ Explanation:
$ The error messages reported by the IDL compiler are stored in an NLS
$ message catalog.  The catalog <v>(filename) is not the same version
$ as the IDL compiler being invoked.  The error messages reported may
$ therefore be incorrect or cause the compiler to terminate abwrnormally.
$ User Action:
$ Check the definition of the NLS environment variable NLSPATH and verify that
$ it searches the appropriate directories.  The default location for the IDL 
$ compiler message catalog is 
$ <v>(<literal>(<)dceshared>)<kw>(/nls/msg/)<v>(LANG)<kw>(/%N)

174 Non-integer values are not allowed in expressions
$ Explanation:
$ One of the values in the expression is not
$ an integer (or one that can be promoted to integer).
$ User Action:
$ Correct the expression to contain only integer values.

175 The [align] attribute is not yet supported

176 The attribute [in(shape)] is not yet supported

177 Arrays with a nonzero lower bound are not yet supported

178 The attribute [out(shape)] is not yet supported

179 Attribute [unique] is not yet supported

180 Operation attributes [code] and [nocode] cannot occur together
$ Explanation:
$ The <kw>([code]) and <kw>([nocode]) attributes have directly opposite
$ meanings.  Do not use both.
$ User Action:
$ Remove one of the conflicting attributes.

181 Unable to open %1$s for read access
$ Explanation:
$ The IDL compiler is unable to open a file for processing.
$ User Action:
$ Make sure the file exists, and that it has the proper access
$ protection for the IDL compiler.

182 Unable to open %1$s for write access
$ Explanation:
$ The IDL compiler is unable to create a file.
$ User Action:
$ Make sure the directory to contain the file exists,
$ and that it has the proper access
$ protection for the IDL compiler.

183 Operation name %1$s referenced in ACF is not defined
$ Explanation:
$ Any operation referenced in an Attribute Configuration File
$ (ACF) must be defined in the corresponding IDL file.
$ User Action:
$ Check for typographical errors in either file.

184 An [out] conformant array must be a top-level parameter or under a full pointer
$ Explanation:
$ An array with unspecified lower and/or upper bounds requires
$ size information.  If the array is a top-level parameter,
$ the size information must be in additional <kw>([in]) parameters.
$ If the array is contained within a structure, the size
$ information must be in additional fields of the structure,
$ and the parameter containing the structure must be <kw>([in]) or
$ <kw>([in,out]).  In both cases, the size information must be <kw>([in])
$ so the server stub knows how much storage to allocate
$ for the array.  The exception to this latter case is any <kw>([out])
$ conformant array that is pointed to by a full <kw>([ptr]) pointer
$ or that indirectly lies under a full <kw>([ptr]) pointer.  In this
$ case the semantics are that the user-written manager code
$ either allocates and/or manages the
$ storage for the array and any other data below the full <kw>([ptr])
$ pointer or returning a <kw>(NULL) value for the full <kw>([ptr]) pointer.
$ User Action:
$ Change the parameter to be <kw>([in,out]), pass the array as a
$ separate <kw>([out]) parameter with <kw>([in]) size information
$ parameters, or change the data structure and the code that
$ manipulates it so the conformant array is under a
$ full <kw>([ptr]) pointer.

185 Output parameters must be passed by reference
$ Explanation:
$ Parameters with the <kw>([out]) or <kw>([in,out]) attributes must be
$ passed by reference, so the changed value of the
$ parameter can be reflected back to the caller.
$ User Action:
$ Add a <kw>(*) to the parameter declaration to indicate
$ pass-by-reference calling mechanism.

186 An [out,ptr] parameter is not valid
$ Explanation:
$ The <kw>([ptr]) parameter attribute implies that the value of the
$ pointer may be <kw>(NULL).  It is invalid on an <kw>([out]) only
$ parameter, since the possible NULL-ness is not shipped
$ to the server (for example, in the <kw>([in]) direction).
$ User Action:
$ If the pointer cannot be <kw>(NULL)
$ remove the <kw>([ptr]) attribute.
$ Otherwise, make the parameter an <kw>([in,out,ptr]) parameter.

187 An [out] parameter or operation result cannot contain [unique] pointers

188 An operation result cannot be a pipe
$ Explanation:
$ Pipes are not allowed as operation results.
$ User Action:
$ Change the operation so the <kw>(pipe) is an extra <kw>([out])
$ parameter instead of the operation result.

189 Output parameters require an explicit top-level *
$ Explanation:
$ Parameters with the <kw>([out]) or <kw>([in,out]) attributes must be
$ passed by reference, so the changed value of the
$ parameter can be reflected back to the caller.
$ IDL does not allow a <kw>(*) in a type definition to serve as
$ a passing mechanism <kw>(*).
$ User Action:
$ Change the declaration of the parameter so it
$ contains an explicit <kw>(*).

190 An [out,unique] parameter is not valid

191 The base type of a pipe cannot be a pipe type
$ Explanation:
$ A <kw>(pipe) type cannot be used in the definition of
$ another <kw>(pipe) type.
$ User Action:
$ Remove the invalid declaration, or change it so the
$ base type of the <kw>(pipe) is not another <kw>(pipe) type.

192 Pipes are not valid as structure fields
$ Explanation:
$ Pipes are not allowed as fields of structures.
$ User Action:
$ Pass a <kw>(pipe) as a separate parameter to an operation,
$ rather than embedding it as a field of a structure.

193 Pipes are not valid as members of unions
$ Explanation:
$ Pipes are not allowed as members of <kw>(union)s.
$ User Action:
$ Pass a <kw>(pipe) as a separate parameter to an operation,
$ rather than embedding it as a member of a <kw>(union).

194 A pipe cannot have a [transmit_as] type
$ Explanation:
$ A pipe is a specialized object handled by RPC that,
$ by itself, is not a transmittable object.
$ Thus, a <kw>([transmit_as]) clause is invalid on a <kw>(pipe) definition.
$ User Action:
$ Remove the <kw>([transmit_as]) attribute.

195 A parameter with [%1$s] cannot be passed by value
$ Explanation:
$ The <v>([attribute-name]) attribute can only be applied
$ to a parameter if that parameter has an explicit <kw>(*).
$ User Action:
$ Modify the parameter signature to have a top-level <kw>(*).

196 A parameter must have either or both the [in] and [out] attributes
$ Explanation:
$ The direction of a parameter must be declared.  The
$ <kw>([in]) attribute tells IDL that the parameter is passed from
$ client to server.  The
$ <kw>([out]) attribute tells IDL that the parameter is passed from
$ server to client.
$ User Action:
$ Add the <kw>([in]), <kw>([out]), or <kw>([in,out]) attribute to the parameter.

197 Parameter attributes [in_line] and [out_of_line] cannot occur together

198 Parameter name %1$s referenced in ACF operation %2$s is not defined
$ Explanation:
$ Any parameter referenced within an operation declaration in an
$ Attribute Configuration File (ACF) must be defined in the
$ corresponding operation definition in the IDL file.
$ User Action:
$ Check for typographical errors in either file.

199 The attribute [ptr] is invalid on a binding handle parameter
$ Explanation:
$ The first parameter in an operation is a <kw>(handle) parameter,
$ either of type <kw>(handle_t) or a type with the <kw>([handle]) attribute.
$ It cannot have the <kw>([ptr]) attribute, since it cannot be <kw>(NULL).
$ User Action:
$ Remove the <kw>([ptr]) attribute from the parameter.

200 The attribute [ptr] is valid only for pointer or array types
$ Explanation:
$ The <kw>([ptr]) attribute modifies the default behavior of a
$ pointer or array data type.  It is not valid on other
$ data types.
$ User Action:
$ You may have missed a <kw>(*) in your declaration.
$ If not, remove the <kw>([ptr]) attribute.

201 The base type of a pipe cannot be or contain a pointer
$ Explanation:
$ IDL does not allow the base type of a <kw>(pipe) to be a pointer
$ or any data type that contains pointers.
$ User Action:
$ Change your <kw>(pipe) type definition so the base type of
$ the <kw>(pipe) is not a pointer and does not contain any pointers.

202 Incorrect syntax for pointer to conformant array 
$ Explanation:
$ An IDL declaration attempts to use mixed pointer <kw>(*) and
$ array <kw>([]) syntax to declare a pointer to an array that has
$ bounds that are not fixed.  It is
$ ambiguous whether size attributes apply to the pointer or the
$ array, thus IDL does not allow mixing pointer and array syntax
$ when size attributes are present.
$ User Action:
$ If the declaration is a parameter declaration and the
$ top-level <kw>(*) is a pointer to an array, that <kw>(*) is
$ unnecessary.  This is because arrays are implicitly passed
$ by reference, as in C.  Remove the top-level <kw>(*).
$ If the declaration is a field of a structure, change the
$ declaration to use arrayified pointer syntax.  A field that is
$ a pointer to a type and also has a conformant array attribute
$ represents a pointer to an array of that type.
$ If the declaration is an [out] parameter, see the description
$ of the message "An [out] conformant array must be a top-level
$ parameter or under a full pointer."

203 Full pointers to context handles are not allowed
$ Explanation:
$ IDL does not allow full pointers to
$ to context handles.
$ User Action:
$ If the declaration has the <kw>([ptr]) attribute, either explicitly
$ or by default, change it to a <kw>([ref]) pointer.  If the
$ declaration is an operation result, it cannot be a <kw>([ref])
$ pointer, thus an operation result of type pointer to
$ context handle is not possible.

204 The attribute [ptr] is not valid on pipe parameters
$ Explanation:
$ IDL does not allow the <kw>([ptr]) attribute
$ on <kw>(pipe) parameters.
$ User Action:
$ Remove the <kw>([ptr]) attribute from the parameter.

205 Pointers to [v1_enum] types are not allowed
$ Explanation:
$ An enumeration type with the <kw>([v1_enum]) attribute follows
$ NCS Version 1 NIDL semantics, and does not allow pointers to
$ enumerations except as a reference passing mechanism on a
$ parameter.
$ User Action:
$ If compatibility with NCS Version 1 NIDL is not necessary,
$ remove the <kw>([v1_enum]) attribute.

206 Pointers to varying arrays are not allowed
$ Explanation:
$ An IDL declaration attempts to use mixed pointer <kw>(*) and
$ array <kw>([]) syntax to declare a pointer to an array that has
$ varying bounds.  Usually, it is
$ ambiguous whether size attributes apply to the pointer or the
$ array, thus IDL does not allow mixing pointer and array syntax
$ when size attributes are present.
$ User Action:
$ If the declaration is a parameter declaration and the
$ top-level <kw>(*) is a pointer to an array, that <kw>(*) is
$ unnecessary.  This is because arrays are implicitly passed
$ by reference, as in C.  Remove the top-level <kw>(*).
$ If the declaration is more complex, 
$ investigate alternative ways to achieve 
$ results, such as using a pointer to a
$ structure that has the required information.

207 void * must be used in conjunction with the [context_handle] attribute
$ Explanation:
$ The only valid use of a <kw>(void *) data type in an interface
$ definition is on an item with the <kw>([context_handle]) attribute.
$ User Action:
$ Change the data type in the declaration, or add the
$ <kw>([context_handle]) attribute.

208 The attribute [ref] is valid only for pointer or array types
$ Explanation:
$ The <kw>([ref]) attribute modifies the default behavior of a
$ pointer or array data type.  It is not valid on other
$ data types.
$ User Action:
$ You may have missed a <kw>(*) in your declaration.
$ If not, remove the <kw>([ref]) attribute.

209 A [ref] function result is not valid
$ Explanation:
$ Unlike <kw>([out]) parameters, there is never any pre-existing
$ storage in the caller for pointer valued function results.
$ The pointer always indicates new storage.  This is the
$ capability provided by full pointers only, not
$ <kw>([ref]) pointers.
$ User Action:
$ Remove the <kw>([ref]) attribute from the type definition
$ of the data type of the function result.

210 Rename of %1$s to %2$s failed
$ Explanation:
$ The call to rename <v>(filename1) to <v>(filename2) returned a failing status.
$ User Action:
$ Make sure the filenames and paths are valid, you have privilege
$ to perform the rename operation, and there is enough free space on the
$ destination device.

211 Types with [represent_as] cannot be nested
$ Explanation:
$ IDL does not allow a data type that has a <kw>([represent_as]) type
$ to itself be used as a <kw>([represent_as]) type in another type
$ definition.
$ User Action:
$ Do not nest <kw>([represent_as]) types.

212 Too many scoping levels
$ Explanation:
$ The input source is too complicated to be parsed.  This
$ occurs because of deeply nested <kw>(struct) or <kw>(union) declarations.
$ User Action:
$ Simplify the input source by using <kw>(typedef)s to represent the
$ nested <kw>(struct) or <kw>(union) declarations, and building the more
$ complicated <kw>(struct) or <kw>(union) declarations by referencing the
$ named types instead of in-line specification of the <kw>(struct)
$ or <kw>(union) declaration.

213 Size attributes can only be applied to array types
$ Explanation:
$ Array bound attributes are only valid when used on array types or pointers
$ used as an array.
$ User Action:
$ Remove the array bound attribute or correct the type to be
$ an array.

214 The attribute [size_is] cannot be applied to dimension %1$lu; upper bound is not dynamic
$ Explanation:
$ A <kw>(size_is) variable is only valid for array dimensions that
$ have an upper bound that is not fixed.
$ User Action:
$ If the array is multidimensional, you may have specified the
$ <kw>(size_is) variable for the wrong dimension.  See the
$ documentation for the proper syntax.  If the array has only
$ one dimension, the <kw>(size_is) clause is invalid, since the
$ array's upper data limit is fixed.  You may have meant to use a
$ <kw>([length_is]) clause.  See the documentation for the
$ distinctions between conformant and varying arrays
$ and their attributes.

215 A [size_is] parameter must have the [in] attribute
$ Explanation:
$ The <kw>([size_is]) attribute specifies a parameter that contains
$ size information of a conformant array.  This must be an <kw>([in])
$ attribute so the server stub code knows how much
$ space to allocate for the array.
$ User Action:
$ Change the size parameter referenced by the
$ <kw>([size_is]) clause to have the <kw>([in]) attribute.

216 The size attributes do not match the array dimension
$ Explanation:
$ There are too many attribute references for the number of 
$ dimensions defined for the array.
$ User Action:
$ Modify the array bound attribute, specifying one reference
$ for each dimension of the array.

217 The array size attribute variable %1$s cannot have the [ptr] or [unique] attributes
$ Explanation:
$ An array size attribute clause in the source IDL specifies
$ its value by dereferencing a pointer parameter.
$ This is invalid if the pointer variable has either of the
$ <kw>([ptr]) or <kw>([unique]) attributes, since both allow the pointer
$ to be <kw>(NULL).  If the pointer is <kw>(NULL), there is no
$ way to determine the size of the array.
$ User Action:
$ Either redeclare the size variable as a scalar rather
$ than a pointer, or change the size variable to a <kw>([ref])
$ pointer.

218 A [size_is] variable must be a small, short, or long integer
$ Explanation:
$ The <kw>([size_is]) attribute specifies a field or parameter that contains
$ size information of a conformant array.  Array bounds, and
$ thus array size, must be nonhyper integers.
$ User Action:
$ Change the size information field or parameter referenced by the
$ <kw>([size_is]) clause to be of integer data type.

219 A size attribute variable must not have a represent_as type
$ Explanation:
$ The parameter or field referenced in a <kw>([max_is]),
$ <kw>([size_is]), <kw>([first_is]), <kw>([last_is]), or <kw>([length_is]) clause must
$ not be of a data type that is declared to have a
$ <kw>([represent_as]) type.
$ User Action:
$ Change the referenced parameter or field data type,
$ or the data type definition itself.

220 A size attribute variable must not have a transmit_as type
$ Explanation:
$ The parameter or field referenced in a <kw>([max_is]),
$ <kw>([size_is]), <kw>([first_is]), <kw>([last_is]), or <kw>([length_is]) clause must
$ not be of a data type that is declared to have a
$ <kw>([transmit_as]) type.
$ User Action:
$ Change the referenced parameter or field data type,
$ or the data type definition itself.

221 The attribute [v1_array] must be in array, not pointer, syntax
$ Explanation:
$ The <kw>([v1_array]) attribute is for compatibility with
$ NCS Version 1 arrays.  NCS Version 1 NIDL did not allow pointers to represent arrays.
$ User Action:
$ Define the array using array syntax.

222 A conformant [v1_array] must also be varying
$ Explanation:
$ An array with the <kw>([v1_array]) attribute that has a 
$ conformant upper bound that is not fixed must also be varying (it must
$ have a <kw>([last_is]) or <kw>([length_is]) attribute in 
$ addition to the <kw>([max_is]) or <kw>([size_is])
$ attribute).
$ User Action:
$ If compatibility with NCS Version 1 NIDL is not necessary, remove the
$ <kw>([v1_array]) attribute.
$ Otherwise, add a <kw>([last_is]) or <kw>([length_is]) 
$ attribute to the declaration.

223 The attribute [v1_array] is invalid for an array with more than 65535 elements
$ Explanation:
$ An array with the <kw>([v1_array]) attribute is limited to a total
$ of 65535 or fewer elements.  The declared array has too many
$ elements to be <kw>([v1_array]).
$ User Action:
$ Remove the <kw>([v1_array]) attribute from the array declaration.

224 The attribute [v1_array] cannot occur with the [first_is] attribute
$ Explanation:
$ With the <kw>([v1_array]) attribute, the only valid array attributes are
$ <kw>([max_is]), <kw>([last_is]), and <kw>([length_is]).
$ User Action:
$ If compatibility with NCS Version 1 NIDL is necessary, the lower bound
$ of the array must be fixed; remove the <kw>([first_is])
$ attribute and change the array declaration.
$ Otherwise, remove the <kw>([v1_array]) attribute.

225 A [v1_array] can be conformant or varying in first dimension only
$ Explanation:
$ An array with the <kw>([v1_array]) attribute is limited to
$ a conformant or varying upper data limit
$ in the first dimension only.
$ User Action:
$ Remove the <kw>([v1_array]) attribute from the array declaration.

226 The attribute [v1_array] cannot occur with the [first_is] or [min_is] attributes
$ Explanation:
$ A <kw>([v1_array]) specifies that the array be handled in a manner
$ compatible with NCS Version 1 NIDL. <kw>([min_is]) and <kw>([first_is])
$ were not supported in NCS Version 1 NIDL.
$ User Action:
$ <kw>([min_is]) is not supported in IDL; remove
$ it. If <kw>([first_is]) is present, either remove it or remove 
$ <kw>([v1_array]).

227 Arrays of strings are not allowed

228 The attribute [string] cannot be applied to a [v1_array]
$ Explanation:
$ A string that is compatible with the NCS Version 1 NIDL <kw>(string0) data
$ type must have the <kw>([v1_array]) and <kw>([v1_string]) attributes.
$ The <kw>([string]) attribute only applies
$ without the <kw>([v1_array])
$ attribute.  A <kw>([v1_array]) is for compatibility with NCS Version 1 NIDL.
$ User Action:
$ If compatibility with NCS Version 1 NIDL is required,
$ place the <kw>([v1_string]) attribute on the declaration.
$ Otherwise, remove the <kw>([v1_array]) attribute from the
$ array declaration.

229 The attribute [string] is valid only for one-dimensional array of char, byte type, or byte structure
$ Explanation:
$ The <kw>([string]) attribute flags that an array is a
$ string.  Strings must be arrays whose base type is char,
$ byte, or a structure whose fields are all scalar byte fields.
$ User Action:
$ Remove the <kw>([string]) attribute, or change the data type
$ so it is a valid string.

230 Structures with [transmit_as] or [represent_as] cannot be conformant
$ Explanation:
$ IDL does not allow a conformant structure with the
$ <kw>([transmit_as]) or <kw>([represent_as]) attribute.
$ User Action:
$ Change the declaration so the structure fields are of
$ fixed size and have none of the conformant array attributes
$ <kw>([max_is]) or <kw>([size_is]),
$ or remove the <kw>([transmit_as]) or <kw>([represent_as]) attribute.

231 Unterminated string literal
$ Explanation:
$ There is a missing closing double quote (<kw>(")) on the string literal.
$ User Action:
$ Make sure the the closing quote for the string literal is on
$ the same source line as the starting quote.  Also verify that
$ any double quote characters internal to the string are 
$ preceded by the escape character (<literal>(\)).

232 The [v1_string] attribute can only be applied to a [v1_array]
$ Explanation:
$ A <kw>([v1_string]) is compatible with the NCS Version 1 NIDL <kw>(string0) data
$ type.  It can be applied only to an array compatible with NCS Version 1. For example,
$ an array with the <kw>([v1_array]) attribute.
$ User Action:
$ If compatibility with NCS Version 1 NIDL is required,
$ place the <kw>([v1_array]) attribute on the array.
$ Otherwise, change the <kw>([v1_string]) attribute to <kw>([string]),
$ that is a generalization of <kw>([v1_string]).

233 A [v1_string] must be an array of char with fixed bounds
$ Explanation:
$ A <kw>([v1_string]) is compatible with the NCS Version 1 NIDL <kw>(string0) data
$ type.  It must be a single dimensioned array of <kw>(char) with
$ a zero lower bound and a fixed upper bound.
$ User Action:
$ If compatibility with NCS Version 1 NIDL is required,
$ change the declaration accordingly.
$ Otherwise, see the documentation regarding the <kw>([string])
$ attribute.

234 A [string] array cannot have varying array attributes
$ Explanation:
$ The <kw>([string]) attribute says that the length of a string is
$ an intrinsic property of the string itself; for example, C
$ strings are zero-terminated.
$ User Action:
$ Remove any <kw>([first_is]), <kw>([last_is]), or <kw>([length_is]) clauses
$ from the string declaration.

235 The attribute [%1$s] can occur at most once per operation
$ Explanation:
$ A <kw>([comm_status]) or <kw>([fault_status]) parameter or operation
$ result is used to return an error code if a certain type of
$ error occurs during execution of an operation.  Do not use
$ either attribute more than once per operation.
$ User Action:
$ Remove the redundant usages of the attribute
$ from the interface's Attribute Configuration File (ACF).

236 A parameter with [%1$s] must be an [out] parameter
$ Explanation:
$ A <kw>([comm_status]) or <kw>([fault_status]) parameter is used to return
$ an error code if a certain type of error occurs during
$ execution of an operation.  Thus, it must be an <kw>([out])
$ parameter.
$ User Action:
$ Place the <kw>([out]) attribute on the parameter.

237 An operation with [%1$s] must return a value of type error_status_t
$ Explanation:
$ The <kw>([comm_status]) or <kw>([fault_status]) attribute on an operation
$ signifies that the return value of the operation is used to
$ return an error code if a certain type of error occurs during
$ execution of an operation.  Thus, the operation must be
$ declared to deliver a result of data type <kw>(error_status_t).
$ User Action:
$ Declare the operation result to be of data type <kw>(error_status_t).

238 A parameter with [%1$s] must be of type error_status_t
$ Explanation:
$ A <kw>([comm_status]) or <kw>([fault_status]) parameter or function result
$ is used to return an error code if a certain type of error
$ occurs during execution of an operation.  The data type of the
$ variable must be <kw>(error_status_t).
$ User Action:
$ Declare the variable to be of data type <kw>(error_status_t).

239 Syntax error
$ Explanation:
$ The source input is incorrect and no interpretation
$ can be made.
$ User Action:
$ Examine the source input on and near the source listed
$ and correct any errors.

240 Syntax error near \"%2$.*1$s\"
$ Explanation:
$ The source input is incorrect and no interpretation
$ can be made for <v>(source-text).
$ User Action:
$ Examine the source input on and near the source listed
$ and correct any errors.

241 Syntax error in UUID format
$ Explanation:
$ The UUID specified in the <kw>([uuid]) attribute of an interface
$ is not in a valid format.
$ User Action:
$ Make sure the UUID is transcribed correctly and 
$ contains the required punctuation.  

242 System error message: %1$s
$ Explanation:
$ Identifies the cause of the
$ previous error.
$ User Action:
$ See the system documentation for an explanation
$ of the error message.

243 Too many elements in %1$s
$ Explanation:
$ The <kw>(enum) <v>(item) contains more than 32767 elements.
$ User Action:
$ Reduce the number of elements included in the
$ <kw>(enum).

244 Too many endpoint specifications; Maximum is %1$lu
$ Explanation:
$ There are more endpoint specifications than IDL can process.
$ User Action:
$ Remove some of the endpoint specifications listed in the <kw>([endpoint]) attribute.
$ Specify at most <v>(number) endpoint specifications for an interface.

245 Type name: %1$s not found
$ Explanation:
$ The type <v>(name) is not declared at this point in the 
$ interface declaration.
$ User Action:
$ Correct the spelling of <v>(name) if incorrect, and make sure
$ that a definition of <v>(name) precedes the reference.

246 Type attributes [in_line] and [out_of_line] cannot occur together
$ Explanation:
$ The <kw>([in_line]) and <kw>([out_of_line]) attributes are 
$ opposite in meaning.
$ You can specify at most one of these attributes on a type.
$ User Action:
$ Remove one of the conflicting attributes.

247 Type name %1$s referenced in ACF is not defined
$ Explanation:
$ The context in which a type name is used in the Attribute
$ Configuration File (ACF) requires that the type be defined
$ in the corresponding IDL file.
$ User Action:
$ See if the referenced type is defined in some other interface.
$ If so, the ACF reference belongs in the ACF for that interface.
$ Check for typographical errors in both the IDL and ACF files.  Check that
$ you have not omitted a type definition in the IDL file.

248 Unbalanced parentheses
$ Explanation:
$ The number of left parentheses and right parentheses are not equal.
$ User Action:
$ Make sure the number of left parentheses equals the number of right
$ parentheses and that each parenthesis is in the correct place.

249 Unbalanced brackets
$ Explanation:
$ The number of left brackets and right brackets are not equal.
$ User Action:
$ Make sure the number of left brackets equals the number of right
$ brackets and that each bracket is in the correct place.

250 Unbalanced braces
$ Explanation:
$ The number of left braces and right braces are not equal.
$ User Action:
$ Make sure the number of left braces equals the number of right
$ braces and that each brace is in the correct place.

251 A union discriminator type must be small, short, long, char, boolean, or enum
$ Explanation:
$ The discriminator of a <kw>(union), that determines which case of
$ the <kw>(union) is used, is restricted to integer, character,
$ enumeration, and boolean data types.  Also, <kw>(hyper int)
$ discriminators are not allowed.
$ User Action:
$ Change the discriminator to one of the valid data types.
$ Make sure the data type of the <kw>(constant) values in the <kw>(case)
$ clauses within the <kw>(union) agree with the discriminator data type.

252 The attribute [unique] is invalid on a binding handle parameter

253 The attribute [unique] is valid only for pointer or array types

254 The attribute [unique] is not valid on context handles

255 A [unique] function result is not valid

256 Unknown attribute [%1$s]
$ Explanation:
$ The attribute specified is not a valid IDL attribute.
$ User Action:
$ Check for spelling errors or misplaced syntax.

257 Obsolete feature encountered; Use translator
$ Explanation:
$ A feature of NCS Version 1 NIDL was encountered that is not supported in
$ the IDL compiler.  You may be compiling an NCS Version 1 NIDL source file.
$ User Action:
$ If the input is a NCS Version 1 NIDL source file, process it with 
$ the translator utility (the <kw>(nidl_to_idl) command) that 
$ converts an NCS Version 1 NIDL source file into the format expected by
$ the IDL compiler.  Then compile the translated source file.  Otherwise, remove
$ the reference to the obsolete feature.

258 The attribute [uuid] is invalid when the [local] attribute is specified
$ Explanation:
$ The <kw>([local]) interface attribute flags that an interface is not
$ for RPC use, but is only a local header generation
$ mechanism.  The <kw>([uuid]) attribute is only used for
$ RPC interfaces.
$ User Action:
$ Remove one of the conflicting attributes.

259 Variable declarations are not supported; declaration of %1$s ignored
$ Explanation:
$ IDL does not support the declaration of variables.  The types
$ of data that can be declared in IDL are; constants, types, and
$ functions.  The declaration of <v>(variable) is not a function
$ and is incorrect.
$ User Action:
$ If intended as a function declaration, 
$ correct the declaration so the top-level declaration
$ is a function. Otherwise, remove the declaration.

260 void is valid only in an operation or a context handle pointer declaration
$ Explanation:
$ The <kw>(void) keyword is only valid as the return value of an
$ operation, or as part of a <kw>(void *) declaration.  Note also
$ that in RPC interfaces, <kw>(void *) is only valid if the
$ <kw>([context_handle]) attribute is also specified on the data item.
$ User Action:
$ You may have missed a <kw>(*) in your declaration.
$ If not, change the <kw>(void) reference to some valid data type.

261 A type used in a [transmit_as] clause cannot have a [represent_as] type
$ Explanation:
$ The type used in a <kw>([transmit_as]) clause specifies an alternative
$ type that a base IDL type is translated to or from and before or after
$ transmission over the network.
$ The type used in a <kw>([represent_as]) clause is an
$ alternative type that a base IDL type is translated from or to at
$ the application code-to-stub code interface.
$ Going from base IDL type to transmittable type and then to
$ a <kw>([represent_as]) type (that may not be transmittable) is
$ invalid.
$ User Action:
$ You may have meant to put a <kw>([represent_as]) clause on the base IDL
$ type, not its transmittable type.

262 A [transmit_as] or [represent_as] type cannot be a conformant array
$ Explanation:
$ A type specified in a <kw>([transmit_as]) clause or a type with
$ the <kw>([transmit_as]) or <kw>([represent_as]) attribute cannot be a
$ conformant array, since there is nowhere to attach the size
$ information attribute that is needed to transmit the data.
$ User Action:
$ Define a structure that contains a conformant array and
$ a size information field instead of using a conformant array
$ directly.

263 The base type of a pipe cannot be a [transmit_as] type
$ Explanation:
$ IDL does not allow the base type of
$ a <kw>(pipe) to have the <kw>([transmit_as]) attribute.
$ User Action:
$ Declare the base type of the <kw>(pipe) to be the data type
$ in the <kw>([transmit_as]) clause.

264 A translated transmittable type cannot contain [%1$s] pointers
$ Explanation:
$ A transmittable type that is derived by calling a translation
$ routine, for example,  a <kw>(from_local) translation of a <kw>([represent_as])
$ type or a <kw>(to_xmit) translation of a <kw>([transmit_as]) type,
$ cannot contain pointers of the indicated type.
$ User Action:
$ Define a transmittable type that does not contain invalid pointers.
$ If using <kw>([represent_as]), reconsider using it.

265 A type with [transmit_as] cannot have other type attributes
$ Explanation:
$ A data type with the <kw>([transmit_as]) attribute
$ (the presented type) has an
$ associated transmittable type, and you must write routines
$ to convert the presented type to or from the transmittable type.
$ Only the transmittable type is relevant to the RPC mechanism,
$ thus it is incorrect for the presented type
$ to have additional RPC-specific attributes.
$ User Action:
$ Remove any type attributes other than <kw>([transmit_as]).

266 Command option -bug %1$lu conflicts with -no_bug %2$lu
$ Explanation:
$ The command line options conflict with each other.
$ User Action:
$ Remove one of the conflicting options.

267 Unterminated comment, end-of-file encountered
$ Explanation:
$ During the processing of a multiline comment,
$ the end of the source 
$ file was reached before the termination characters for 
$ the comment.
$ User Action:
$ Make sure that all multiline comments are correctly terminated.

268 Compilation aborted
$ Explanation:
$ Previous errors caused the compilation to abort.
$ User Action:
$ Correct the errors and recompile.

269 Internal IDL compiler error: Module %1$s, Line %2$lu
$ Explanation:
$ An internal IDL compiler error was found.
$ User Action:
$ Submit a Software Performance Report and notify 
$ your software support representative so the appropriate
$ action can be taken.

270 Command option -bug %1$lu is not known
$ Explanation:
$ The documentation defines all the valid integers that can
$ accompany the <kw>(-bug) and <kw>(-no_bug) options.  You have chosen
$ an invalid value.
$ User Action:
$ See the documentation for the correct number.

271 Command option -no_bug %1$lu is not known
$ Explanation:
$ The documentation defines all the valid integers that can
$ accompany the <kw>(-bug) and <kw>(-no_bug) options.  You have chosen
$ an invalid value.
$ User Action:
$ See the documentation for the correct number.

272 Reached maximum of %1$lu warnings; exiting
$ Explanation:
$ The IDL compiler has a predefined maximum number of warnings
$ it produces before aborting the compilation.  This feature
$ suppresses what may be the same warning
$ over and over.
$ User Action:
$ Fix the warnings you get, or recompile the IDL file or
$ specify the <kw>(-no_warn) option.

273 Command option %1$s does not take a value
$ Explanation:
$ You specified a value with a command line option that
$ does not take a value.
$ User Action:
$ If the value is meant to be the source file, it must
$ be separated from the command option with a space.

274 Out of memory
$ Explanation:
$ The system-defined limit of memory available to the compiler was exceeded.
$ User Action:
$ Either have the system-defined limit of memory raised, or
$ simplify the source file being compiled by breaking it into multiple modules or 
$ eliminating unnecessary imports.

275 Length of source filename and stub suffix exceed 8 chars

276 A source IDL filename is required
$ Explanation:
$ The command line does not contain a source IDL file to compile.
$ User Action:
$ You must specify a source IDL file.  You may have erroneously
$ specified the source IDL file when
$ actually you gave a value to a command option.

277 Unknown command line option: %1$s
$ Explanation:
$ You entered an invalid command line option.
$ User Action:
$ Check the command line for typographical errors.
$ See the documentation for proper command line syntax.

278 A structure field cannot be of a presented type for which the transmitted type is conformant
$ Explanation:
$ IDL does not allow a structure field whose type has a
$ <kw>([transmit_as]) attribute where the transmissible type
$ is conformant.
$ User Action:
$ Use a valid IDL construct.

279 A type with the [represent_as] attribute cannot be conformant
$ Explanation:
$ IDL does not allow a conformant network type to have a local
$ represented type.
$ User Action:
$ Use a valid IDL construct.

280 Attribute cannot be used more than once
$ Explanation:
$ An attribute is repeated multiple times in an attribute list.
$ User Action:
$ Remove all but one occurrence of the offending attribute.

281 Operation with [bb] attribute must have exactly 2 parameters
$ Explanation:
