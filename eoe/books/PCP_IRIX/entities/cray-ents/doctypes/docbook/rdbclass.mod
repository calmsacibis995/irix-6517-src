<!-- ..................................................................... -->
<!-- DocBook class and mixture redeclaration module for V2.3 ............. -->
<!-- Filename: rdbclass.mod .............................................. -->


<!-- This redeclaration module supports Cray Research, Inc., modifications -->
<!-- to DocBook V2.3. These modifications are listed as V1.1.............. -->




<!-- ..................................................................... -->


<!-- redefinition of list.class to exclude glosslist and add deflist.......-->

<!ENTITY % local.list.class	"">
<!ENTITY % list.class
		"ItemizedList | OrderedList | SegmentedList
		| SimpleList | VariableList | DefList %local.list.class;">

<!-- redefinition of para.class to exclude both formalpara and simpara ... -->
<!-- Reason: document analysis team determined that formalpara and simpara
     are not required for Cray documentation.............................. -->

<!ENTITY % para.class
		"para"	>

<!-- redefinition of cptr.char.class to remove medialabel, symbol
     action, application, classname, database, errorname, errortype,
     hardware, computeroutput, keycode, keysym, parameter, property,
     retrunvalue, structfield, systemitem, token, type ................... --> 
<!-- Reason: These elements do not add value to current Cray documentation
     using the existing delivery mechanisms. The Document Analysis team can
     see no use for end users conducting context searches based on these
     parameters or for triggering presentation differences................ -->

<!ENTITY % cptr.char.class
		"accel | command 
		| filename | function
		| interface | interfacedefinition | keycap
		| literal | msgtext | option | optional
		| replaceable
		| structname 
		| userinput"	>

<!-- redefinition of informal.class to remove informalequation,
informalexample, and informaltable ....................................... -->
<!-- Reason: Cray documentation exclusively uses the formal version of these
     structures so the informal versions are not necessary................ -->

<!ENTITY % informal.class
		"blockquote | graphic"		>

<!-- redefinition of compound.class to remove sidebar .................... -->
<!-- Reason: Cray Research documentation does not utilize sidebars ....... -->

<!ENTITY % compound.class
		"msgset | procedure"		>

<!-- redefinition of genobj.class to remove bridgehead ................... -->
<!-- Reason: Cray documentation does not utilize bridgeheads ............. -->

<!ENTITY % genobj.class
		"anchor | comment | highlights"	>

<!-- redefinition of descobj.class to remove authorblurb and epigraph .... -->
<!-- Reason: Cray documentation does not use authorblurbs or epigraphs ... -->

<!ENTITY % descobj.class
		"abstract"		>

<!-- redefinition of word.char.class to remove:
	* citation
	* citerefentry
	* quote
	* sgmltag
-->

<!ENTITY % word.char.class
		"emphasis | citetitle 
		| glossterm | firstterm |  footnote | trademark"	>

<!-- redfinition of docinfo.char.class to remove:
	* author
	* authorinitials
	* corpauthor
	* othercredit
	* productname
	* productnumber
-->

<!ENTITY % docinfo.char.class
		"modespec | revhistory"		>
 

