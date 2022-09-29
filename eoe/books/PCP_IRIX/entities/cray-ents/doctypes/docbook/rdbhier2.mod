<!-- ..................................................................... -->
<!-- DocBook information pool redeclaration module for V2.3 .............. -->
<!-- Filename: rdbpool.mod ............................................... -->


<!-- This redeclaration module supports Cray Research, Inc., modifications -->
<!-- to DocBook V2.3. These modifications are listed as V1.1.............. -->




<!-- ..................................................................... -->

<!-- This redeclaration module contains the definitions for the following
     information units:

	=> sect1.content
	=> element sect1 to section


     In the DTD driver file referring to this redeclaration module, use
     an entity declaration that uses the public identifier shown below:

     <!ENTITY % rdbhier.	PUBLIC	
      "-//Davenport//ELEMENTS Document DocBook Hierarchy
      Redeclarations//EN"

-->

<!-- ..................................................................... -->

<!-- Redefine sect1.content
     Remove sect1 element from sect1.content and replace it with
     section	-->

<!ENTITY % bookcomponent.content
	"((%divcomponent.mix;)+, (section*|(%refentry.class;)*))
	|(section+|(%refentry.class;)+)">

<!-- Redefine sect1 element to section	-->
<!-- comment out for now
<!ELEMENT section	- -	((%sect.title.content;), 
	(((%divcomponent.mix;)+, ((%refentry.class;)* | section*))
	| (%refentry.class;)+ | section+))	+(%ubiq.mix;)>

<!ATTLIST section
		%common.attrib;
		%label.attrib;
		%cray.attrib;	>
-->

