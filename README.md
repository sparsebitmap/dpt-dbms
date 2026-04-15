# dpt-dbms

## What this is
A DBMS with file structures inspired by the high-performance mainframe system "Model 204", which means:
- Optimised for fast queries against very large files, using bit-mapped indexes where appropriate
- Tolerant of heavy update volumes without undue fragmentation
- Fairly low-level and quirky to configure

This repository contains C++ source for the DBMS plus:
- C-style API layer allowing access from languages which can handle that (e.g. Python)
- The JNI module
- Fairly detailed documentation on configuring the DBMS, as well as compiling it and some sample code
  - Once this repository is up and running I'll put more here about specific docs 

## DPT background
DPT was a kind of unofficial proof-of-concept for a potential cross-platform version of the mainframe-only application development platform "Model 204", including a loosely copycat DBMS (this repo) as well as a compiler for the built-in 4GL ("User Language") and various other tooling like a visual User Language debugger. 

In the 90s and 00s small hardware and networking tech were advancing so fast it seemed like the end for mainframes, dumb terminals, tape drives and all that. Legacy IT in general still had a bit of a bad smell from Y2K and everyone was thinking about trading their old systems in for shiny new ones involving PCs. DPT was a product of that time when nobody really knew which way mainframe computing would go.     

Of course in the end the mainframes didn't die, and interest in non-mainframe Model 204 turned out to be pretty low so the project fizzled out. Users did have some success porting small applications to DPT, but not with enough confidence to swap for good, and the technical leadership of Model 204 who (frustratingly for many clients) had held off from much product development during those uncertain years in the industry, finally got stuck into some significant enhancements to the core mainframe engine and developer tools, as well as vastly improving the connectivity to other platforms. I'm happy to admit they were right and I was wrong!
