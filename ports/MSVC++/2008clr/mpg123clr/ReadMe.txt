========================================================================
    DYNAMIC LINK LIBRARY : mpg123clr Project Overview
========================================================================

Project Installation:

The project is part of a Visual Studio 2008 Solution.

The Visual Studio solution file is 2008clr.sln located in a folder/directory named 2008clr.

This folder/directory should be located in the mpg123/ports/MSVC++ folder to maintain file relationships
with required mpg123 library files. i.e. at mpg123/ports/MSVC++/2008clr.

The solution includes the following projects:

	mpg123clr			The mpg123 CLR project. This creates the CLR compatible DLL file.
	scanclr				Demonstration project for comparison with scan.c example.
	feedseekclr			Demonstration project for comparison with feedseek.c example
	replacereaderclr	Demonstration project, how to implement ReplaceReader in CLR (proof of concept)

=========================================================================
Compiling:
Configuration Manager:

mpg123clr requires linking to a libmpg123 static library, this library must be compiled prior to
compiling the clr projects. mpg123clr uses relative folder position during linking; therefore either:-

	ensure that the 2008clr solution is correctly located within the ports\msvc++ folder, or
	change the "linker->input->additional dependancies" to suit your installation configuration

NOTE: x64 Active Solution Platform configurations are not supplied with this solution but have been tested.
(Effectively all that is required is to change all "include" references to account for the "x64" output locations) 

NOTE: The "unsafe" context of the ReplaceReaderClr project is only required to allow the use of callbacks. 
mpg123clr is written to avoid the use of "unsafe" for the majority of normal operations.


==========================================================================
Using mpg123clr:

Mpg123clr is a CLR ready dll. Simply add a reference to the mpg123clr.dll file in your application project.

You may also add a "using" directive in your program files. C# e.g. using mpg123clr;

The mpg123clr project is configured to create an additional Intellisense documentation file (mpg123clr.xml)
to assist usage. If you manually relocate the .dll please also copy the .xml file to ensure correct Intellisense 
operation.

===========================================================================
Developer Notes:

 1) Documentation <remarks> do not appear in CLR Intellisense environment (at time of writing). 
    So deferred in favour of <para> within <summary>.
 2) During development "off_t" parameters conflicted with intellisense (although size_t worked just fine)
	Since many CLR functions that refer to off_t features (e.g. positioners) actually expect 64bit long
	(or long long in C++) parameters, I've implemented off_t as a long long - with one notable exception. 
	This fixed intellisense and accommodates the CLR 64bit long implimentation positioners: And then intellisense 
	started	working correctly and made this a moot point - at least as far as intellisense is concerned.
 3) off_t notable exception - SeekDelegate
	Usually 2008clr functions are called by the callee, long long (clr 64bit long) can be marshaled and passed
	to libmpg123. The seek delegate is called directly by the libmpg123 seek functions - there isn't an opportunity
	to marshal the offending parameters. A library compiled in 32 bit will expect the SeekDelegate to have a 32bit
	position parameter but in 64bit compilation the same parameter will be 64bit. Therefore the need to be correct
	overrides the need for consistency with CLR 64bit positioners.
	SIDE NOTE: In early tests the SeekDelegate was defined with a long long parameter - libmpg123 called it oblivious
	to the fact that the type was incorrect. The only impact was that the 32bit position and 32bit origin (whence)
	were compacted to a single 64bit parameter. This is fixed, no longer an issue!!!
4)	PosixSeek and PosixRead ARE ONLY INCLUDED AS PROOF OF CONCEPT - to prove that the callbacks into CLR would
	function correctly with libmpg123 cdecl calls. It saved making a separate dll for the posix elements.
	REPEAT AFTER ME: Not for normal consumption....
5)	The __clrcall decoration of mpg123clr functions is not strictly necessary for Visual Studio 2005 and later,
	but has been left in for compatability with earlier versions. See MS documentation of "double thunking"
6)	Many functions use pin_ptr to pin the return variables prior to calling the libmpg123 function, this was
	considered as an alternative to avoid the use of local variables with attendant overhead of 
	allocate/use/copy/destroy etc. In practice pin_ptr has significant overhead, empirical tests showed very 
	little performance difference between either method.
	
=============================================================================
Tested/Untested

I haven't used mpg123clr in a production project yet, although one is about to start. Therefore EVERYTHING should
be treated as though unproven. UPDATE: the scanclr, feedseekclr and replacereaderclr examples have provided a basic
functional test.

The majority of functions have been tried in test projects with the following exceptions.

64bit compilation.		mpg123clr compiles correctly in 64bit x64 platform mode, but hasn't been tested.

ICY(icy_meta);			Never had genuine ICY data to test with.
Icy2Utf8(icy_text);		Never tried. Please provide sample material.
enc_from_id3(encbyte);	Works on libmpg123 enums, never had real world text samples.
store_utf8(...);		Works with CLR generated text, not tried with real world text samples.

==============================================================================
Documentation

mpg123clr is 90% documentation, use intellisense as your reference. If in doubt refer back to the libmpg123
documents. NOTE: You need to build the mpg123clr library to create the intellisense file.

===============================================================================
Error Trapping

libmpg123 is an error agnostic library - it shouldn't generate errors, but neither does it trap them.

mpg123clr is built on the same concept although some CLR generated errors have been trapped. My personal
preference is that exceptions should be avoided not relied on...

THE CALLING APPLICATION IS RESPONSIBLE FOR ERROR DETECTION, TRAPPING AND CORRECTION.

All projects are "as is" with no express or implied warranty. Demonstration projects are to demonstrate the 
use of the library and are NOT considered examples of good coding practice. Use at own risk.


Constructive feedback preferred.

Malcolm Boczek
mpg123clr original author.
