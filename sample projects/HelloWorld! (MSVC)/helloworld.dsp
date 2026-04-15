# Microsoft Developer Studio Project File - Name="helloworld" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=helloworld - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "helloworld.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "helloworld.mak" CFG="helloworld - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "helloworld - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "helloworld - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "helloworld - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\include" /I "..\..\include\dbapi" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "_BBDBAPI" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib winspool.lib shell32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "helloworld - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\include" /I "..\..\include\dbapi" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_BBDBAPI" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib winspool.lib shell32.lib /nologo /subsystem:console /debug /machine:I386 /nodefaultlib:"libcmt.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "helloworld - Win32 Release"
# Name "helloworld - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "general impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\scope.cpp
# End Source File
# End Group
# Begin Group "db impl"

# PROP Default_Filter ""
# Begin Group "buffman impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\buffhandle.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\buffmgmt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\paged_io.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\rawpage.cpp
# End Source File
# End Group
# Begin Group "context impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\ctxtdef.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\ctxtopen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\ctxtspec.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbctxt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\group.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\grpserv.cpp
# End Source File
# End Group
# Begin Group "dbfile impl"

# PROP Default_Filter ""
# Begin Group "pagetypes impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\page_a.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_b.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_e.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_f.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_i.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_l.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_m.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_p.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_t.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_v.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\page_x.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\pagebase.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\pagebitmap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\pageixval.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\pageslotrec.cpp
# End Source File
# End Group
# Begin Group "physdata impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\dbf_data.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_ebm.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_tableb.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\recdata.cpp
# End Source File
# End Group
# Begin Group "physindex impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\btree.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_find.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_idiag.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_index.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_ival.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_join.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_tabled.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\findwork.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\inverted.cpp
# End Source File
# End Group
# Begin Group "physmisc impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\cfr.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_field.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbf_rlt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\du1step.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\fastload.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\fastunload.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\loaddiag.cpp
# End Source File
# End Group
# Begin Group "logrecs impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\bmset.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\findspec.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\foundset.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\frecset.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\reccopy.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\reclist.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\record.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\recset.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\sortrec.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\sortset.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\sortspec.cpp
# End Source File
# End Group
# Begin Group "logvals impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\valdirect.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\valset.cpp
# End Source File
# End Group
# Begin Group "logmisc impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\dbcursor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\fieldatts.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\fieldname.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\fieldinfo.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\fieldval.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\source\dbfile.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbstatus.cpp
# End Source File
# End Group
# Begin Group "Integrity impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\atom.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\atomback.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\checkpt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\except_rlc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\lockspecial.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\recovery.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\update.cpp
# End Source File
# End Group
# Begin Group "seqio impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\iowrappers.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\seqfile.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\seqserv.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\source\dbserv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\handles.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\rsvwords.cpp
# End Source File
# End Group
# Begin Group "core impl"

# PROP Default_Filter ""
# Begin Group "msgs impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\audit.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\msgini.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\msgref.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\msgroute.cpp
# End Source File
# End Group
# Begin Group "parms impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\parmini.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\parmized.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\parmref.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\parmref1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\parmvr.cpp
# End Source File
# End Group
# Begin Group "stats impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\statized.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\statref.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\statref1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\statview.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\access.cpp
# End Source File
# End Group
# Begin Group "files impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\file.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\filehandle.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\sysfile.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\source\core.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\garbage.cpp
# End Source File
# End Group
# Begin Group "util impl"

# PROP Default_Filter ""
# Begin Group "lineio impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\lineio.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\liocons.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\lioshare.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\liostdio.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\source\bbfloat.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\bbstdio.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\bitmap3.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\buffer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\bbthread.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\charconv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dataconv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\except.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\hash.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\lockable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\merge.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\parsing.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\pattern.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\progress.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\resource.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\stlextra.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\winutil.cpp
# End Source File
# End Group
# Begin Group "dbapi impl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\source\dbapi\access_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\bmset_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\core_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\ctxtspec_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\cursor_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\dbctxt_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\dbserv_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\fieldatts_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\fieldinfo_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\fieldval_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\findspec_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\floatnum_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\foundset_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\grpserv_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\msgroute_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\parmvr_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\reccopy_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\reclist_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\record_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\recread_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\recset_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\seqfile_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\seqserv_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\sortset_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\sortspec_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\statview_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\valdirect_api.cpp
# End Source File
# Begin Source File

SOURCE=..\..\source\dbapi\valset_api.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\helloworld.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "apiheaders"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\dbapi\access.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\apiconst.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\bmset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\core.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\ctxtspec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\cursor.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\dbctxt.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\dbserv.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\dptdb.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\fieldatts.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\fieldname.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\fieldinfo.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\fieldval.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\findspec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\floatnum.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\foundset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\grpserv.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\msgroute.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\parmvr.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\reccopy.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\reclist.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\record.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\recread.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\recset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\seqfile.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\seqserv.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\sortset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\sortspec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\statview.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\valdirect.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbapi\valset.h
# End Source File
# End Group
# Begin Group "util"

# PROP Default_Filter ""
# Begin Group "lineio"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\lineio.h
# End Source File
# Begin Source File

SOURCE=..\..\include\liocons.h
# End Source File
# Begin Source File

SOURCE=..\..\include\lioshare.h
# End Source File
# Begin Source File

SOURCE=..\..\include\liostdio.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\include\bbfloat.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bbstdio.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bitmap3.h
# End Source File
# Begin Source File

SOURCE=..\..\include\buffer.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bbthread.h
# End Source File
# Begin Source File

SOURCE=..\..\include\bbarray.h
# End Source File
# Begin Source File

SOURCE=..\..\include\charconv.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dataconv.h
# End Source File
# Begin Source File

SOURCE=..\..\include\except.h
# End Source File
# Begin Source File

SOURCE=..\..\include\hash.h
# End Source File
# Begin Source File

SOURCE=..\..\include\lockable.h
# End Source File
# Begin Source File

SOURCE=..\..\include\merge.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msg_util.h
# End Source File
# Begin Source File

SOURCE=..\..\include\parsing.h
# End Source File
# Begin Source File

SOURCE=..\..\include\pattern.h
# End Source File
# Begin Source File

SOURCE=..\..\include\progress.h
# End Source File
# Begin Source File

SOURCE=..\..\include\resource.h
# End Source File
# Begin Source File

SOURCE=..\..\include\stlextra.h
# End Source File
# Begin Source File

SOURCE=..\..\include\winutil.h
# End Source File
# End Group
# Begin Group "core"

# PROP Default_Filter ""
# Begin Group "msgs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\audit.h
# End Source File
# Begin Source File

SOURCE=..\..\include\const_route.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msgcodes.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msgini.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msgopts.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msgref.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msgroute.h
# End Source File
# End Group
# Begin Group "parms"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\parmini.h
# End Source File
# Begin Source File

SOURCE=..\..\include\parmized.h
# End Source File
# Begin Source File

SOURCE=..\..\include\parmref.h
# End Source File
# Begin Source File

SOURCE=..\..\include\parmvr.h
# End Source File
# End Group
# Begin Group "stats"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\const_stat.h
# End Source File
# Begin Source File

SOURCE=..\..\include\statized.h
# End Source File
# Begin Source File

SOURCE=..\..\include\statref.h
# End Source File
# Begin Source File

SOURCE=..\..\include\statview.h
# End Source File
# Begin Source File

SOURCE=..\..\include\access.h
# End Source File
# End Group
# Begin Group "files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\const_file.h
# End Source File
# Begin Source File

SOURCE=..\..\include\file.h
# End Source File
# Begin Source File

SOURCE=..\..\include\filehandle.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msg_file.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sysfile.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\include\core.h
# End Source File
# Begin Source File

SOURCE=..\..\include\flgs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\garbage.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msg_core.h
# End Source File
# End Group
# Begin Group "db"

# PROP Default_Filter ""
# Begin Group "buffman"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\buffhandle.h
# End Source File
# Begin Source File

SOURCE=..\..\include\buffmgmt.h
# End Source File
# Begin Source File

SOURCE=..\..\include\paged_io.h
# End Source File
# Begin Source File

SOURCE=..\..\include\rawpage.h
# End Source File
# End Group
# Begin Group "context"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\const_group.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ctxtdef.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ctxtopen.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ctxtspec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbctxt.h
# End Source File
# Begin Source File

SOURCE=..\..\include\group.h
# End Source File
# Begin Source File

SOURCE=..\..\include\grpfind.h
# End Source File
# Begin Source File

SOURCE=..\..\include\grpserv.h
# End Source File
# End Group
# Begin Group "dbfile"

# PROP Default_Filter ""
# Begin Group "pagetypes"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\page_a.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_all.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_b.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_e.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_f.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_i.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_l.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_m.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_p.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_t.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_v.h
# End Source File
# Begin Source File

SOURCE=..\..\include\page_x.h
# End Source File
# Begin Source File

SOURCE=..\..\include\pagebase.h
# End Source File
# Begin Source File

SOURCE=..\..\include\pagebitmap.h
# End Source File
# Begin Source File

SOURCE=..\..\include\pageixval.h
# End Source File
# Begin Source File

SOURCE=..\..\include\pageslotrec.h
# End Source File
# End Group
# Begin Group "physdata"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\dbf_data.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_ebm.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_tableb.h
# End Source File
# Begin Source File

SOURCE=..\..\include\recdata.h
# End Source File
# End Group
# Begin Group "physindex"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\btree.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_find.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_index.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_join.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_tabled.h
# End Source File
# Begin Source File

SOURCE=..\..\include\findwork.h
# End Source File
# Begin Source File

SOURCE=..\..\include\inverted.h
# End Source File
# End Group
# Begin Group "physmisc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\cfr.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_field.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbf_rlt.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbfield.h
# End Source File
# Begin Source File

SOURCE=..\..\include\du1step.h
# End Source File
# Begin Source File

SOURCE=..\..\include\fastload.h
# End Source File
# Begin Source File

SOURCE=..\..\include\fastunload.h
# End Source File
# Begin Source File

SOURCE=..\..\include\loaddiag.h
# End Source File
# End Group
# Begin Group "logrecs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\bmset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\findspec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\foundset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\frecset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\reccopy.h
# End Source File
# Begin Source File

SOURCE=..\..\include\reclist.h
# End Source File
# Begin Source File

SOURCE=..\..\include\record.h
# End Source File
# Begin Source File

SOURCE=..\..\include\recread.h
# End Source File
# Begin Source File

SOURCE=..\..\include\recset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sortrec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sortset.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sortspec.h
# End Source File
# End Group
# Begin Group "logvals"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\valdirect.h
# End Source File
# Begin Source File

SOURCE=..\..\include\valset.h
# End Source File
# End Group
# Begin Group "logmisc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\dbcursor.h
# End Source File
# Begin Source File

SOURCE=..\..\include\fieldatts.h
# End Source File
# Begin Source File

SOURCE=..\..\include\fieldinfo.h
# End Source File
# Begin Source File

SOURCE=..\..\include\fieldval.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\include\dbfile.h
# End Source File
# Begin Source File

SOURCE=..\..\include\dbstatus.h
# End Source File
# End Group
# Begin Group "Integrity"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\atom.h
# End Source File
# Begin Source File

SOURCE=..\..\include\atomback.h
# End Source File
# Begin Source File

SOURCE=..\..\include\checkpt.h
# End Source File
# Begin Source File

SOURCE=..\..\include\except_rlc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\lockspecial.h
# End Source File
# Begin Source File

SOURCE=..\..\include\molecerr.h
# End Source File
# Begin Source File

SOURCE=..\..\include\recovery.h
# End Source File
# Begin Source File

SOURCE=..\..\include\update.h
# End Source File
# End Group
# Begin Group "seqio"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\iowrappers.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msg_seq.h
# End Source File
# Begin Source File

SOURCE=..\..\include\seqfile.h
# End Source File
# Begin Source File

SOURCE=..\..\include\seqserv.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\include\dbserv.h
# End Source File
# Begin Source File

SOURCE=..\..\include\handles.h
# End Source File
# Begin Source File

SOURCE=..\..\include\infostructs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\msg_db.h
# End Source File
# Begin Source File

SOURCE=..\..\include\rsvwords.h
# End Source File
# End Group
# Begin Group "general"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\scope.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
