# Microsoft Developer Studio Project File - Name="gcsdk" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=gcsdk - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "gcsdk.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "gcsdk.mak" CFG="gcsdk - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "gcsdk - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "gcsdk - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "gcsdk - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "gcsdk - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "gcsdk - Win32 Release"
# Name "gcsdk - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "libogc.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libogc\aram.c
# End Source File
# Begin Source File

SOURCE=.\libogc\arqmgr.c
# End Source File
# Begin Source File

SOURCE=.\libogc\arqueue.c
# End Source File
# Begin Source File

SOURCE=.\libogc\audio.c
# End Source File
# Begin Source File

SOURCE=.\libogc\cache.c
# End Source File
# Begin Source File

SOURCE=.\libogc\cache_asm.S
# End Source File
# Begin Source File

SOURCE=.\libogc\card.c
# End Source File
# Begin Source File

SOURCE=.\libogc\cond.c
# End Source File
# Begin Source File

SOURCE=.\libogc\decrementer.c
# End Source File
# Begin Source File

SOURCE=.\libogc\decrementer_handler.S
# End Source File
# Begin Source File

SOURCE=.\libogc\depackrnc.S
# End Source File
# Begin Source File

SOURCE=.\libogc\dsp.c
# End Source File
# Begin Source File

SOURCE=.\libogc\dvd.c
# End Source File
# Begin Source File

SOURCE=.\libogc\exception.c
# End Source File
# Begin Source File

SOURCE=.\libogc\exception_handler.S
# End Source File
# Begin Source File

SOURCE=.\libogc\exi.c
# End Source File
# Begin Source File

SOURCE=.\libogc\gu.c
# End Source File
# Begin Source File

SOURCE=.\libogc\gu_psasm.S
# End Source File
# Begin Source File

SOURCE=.\libogc\gx.c
# End Source File
# Begin Source File

SOURCE=.\libogc\irq.c
# End Source File
# Begin Source File

SOURCE=.\libogc\irq_handler.S
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_handler.S
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_messages.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_messages.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_mutex.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_mutex.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_priority.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_priority.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_queue.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_queue.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_sema.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_sema.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_stack.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_stack.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_states.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_threadq.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_threadq.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_threads.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_threads.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_watchdog.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_watchdog.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_wkspace.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_wkspace.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\message.c
# End Source File
# Begin Source File

SOURCE=.\libogc\mutex.c
# End Source File
# Begin Source File

SOURCE=.\libogc\ogc_crt0.S
# End Source File
# Begin Source File

SOURCE=.\libogc\pad.c
# End Source File
# Begin Source File

SOURCE=.\libogc\semaphore.c
# End Source File
# Begin Source File

SOURCE=.\libogc\si.c
# End Source File
# Begin Source File

SOURCE=.\libogc\sys_state.c
# End Source File
# Begin Source File

SOURCE=.\libogc\sys_state.inl
# End Source File
# Begin Source File

SOURCE=.\libogc\system.c
# End Source File
# Begin Source File

SOURCE=.\libogc\system_asm.S
# End Source File
# Begin Source File

SOURCE=.\libogc\video.c
# End Source File
# Begin Source File

SOURCE=.\libogc\video_asm.S
# End Source File
# End Group
# Begin Group "lwip.c"

# PROP Default_Filter ""
# Begin Group "arch"

# PROP Default_Filter ""
# Begin Group "gc"

# PROP Default_Filter ""
# Begin Group "netif"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\lwip\arch\gc\netif\gcif.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\lwip\arch\gc\lib_arch.c
# End Source File
# End Group
# End Group
# Begin Group "core"

# PROP Default_Filter ""
# Begin Group "ip4"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\lwip\core\ipv4\icmp.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\ipv4\ip.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\ipv4\ip_addr.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\ipv4\ip_frag.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\lwip\core\dhcp.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\inet.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\inet6.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\mem.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\memp.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\netif.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\pbuf.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\raw.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\stats.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\sys.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\tcp.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\tcp_in.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\tcp_out.c
# End Source File
# Begin Source File

SOURCE=.\lwip\core\udp.c
# End Source File
# End Group
# Begin Group "netif_1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\lwip\netif\etharp.c
# End Source File
# Begin Source File

SOURCE=.\lwip\netif\loopif.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\lwip\netio.c
# End Source File
# Begin Source File

SOURCE=.\lwip\network.c
# End Source File
# End Group
# Begin Group "libmodplay.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libmodplay\bpmtab32.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\bpmtab48.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\freqtab.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\gcmodplay.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\inctab32.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\inctab48.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\mixer.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\modplay.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\semitonetab.c
# End Source File
# End Group
# Begin Group "libmad.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libmad\bit.c
# End Source File
# Begin Source File

SOURCE=.\libmad\D.dat
# End Source File
# Begin Source File

SOURCE=.\libmad\decoder.c
# End Source File
# Begin Source File

SOURCE=.\libmad\fixed.c
# End Source File
# Begin Source File

SOURCE=.\libmad\frame.c
# End Source File
# Begin Source File

SOURCE=.\libmad\huffman.c
# End Source File
# Begin Source File

SOURCE=.\libmad\imdct_s.dat
# End Source File
# Begin Source File

SOURCE=.\libmad\layer12.c
# End Source File
# Begin Source File

SOURCE=.\libmad\layer3.c
# End Source File
# Begin Source File

SOURCE=.\libmad\madplayer.c
# End Source File
# Begin Source File

SOURCE=.\libmad\qc_table.dat
# End Source File
# Begin Source File

SOURCE=.\libmad\rq_table.dat
# End Source File
# Begin Source File

SOURCE=.\libmad\sf_table.dat
# End Source File
# Begin Source File

SOURCE=.\libmad\stream.c
# End Source File
# Begin Source File

SOURCE=.\libmad\synth.c
# End Source File
# Begin Source File

SOURCE=.\libmad\timer.c
# End Source File
# Begin Source File

SOURCE=.\libmad\version.c
# End Source File
# End Group
# Begin Group "libdb.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libdb\debug.c
# End Source File
# Begin Source File

SOURCE=.\libdb\debug_handler.S
# End Source File
# End Group
# Begin Group "libogcsys.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libogcsys\close.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\color.h
# End Source File
# Begin Source File

SOURCE=.\libogcsys\console.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\console.h
# End Source File
# Begin Source File

SOURCE=.\libogcsys\console_font.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\console_font_8x8.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\fstat.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\getpid.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\iosupp.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\iosupp.h
# End Source File
# Begin Source File

SOURCE=.\libogcsys\isatty.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\kill.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\lseek.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\malloc_lock.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\netio_fake.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\netio_fake.h
# End Source File
# Begin Source File

SOURCE=.\libogcsys\newlibc.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\open.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\read.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\sbrk.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\sleep.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\stdin_fake.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\stdin_fake.h
# End Source File
# Begin Source File

SOURCE=.\libogcsys\timesupp.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\timesupp.h
# End Source File
# Begin Source File

SOURCE=.\libogcsys\usleep.c
# End Source File
# Begin Source File

SOURCE=.\libogcsys\write.c
# End Source File
# End Group
# Begin Group "libsdcard.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libsdcard\card_buf.c
# End Source File
# Begin Source File

SOURCE=.\libsdcard\card_fat.c
# End Source File
# Begin Source File

SOURCE=.\libsdcard\card_io.c
# End Source File
# Begin Source File

SOURCE=.\libsdcard\card_uni.c
# End Source File
# Begin Source File

SOURCE=.\libsdcard\sdcard.c
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "ogc.h"

# PROP Default_Filter ""
# Begin Group "machine"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\ogc\machine\asm.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\machine\processor.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\machine\spinlock.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\gc\ogc\aram.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\arqmgr.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\arqueue.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\audio.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\cache.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\card.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\cast.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\cond.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\context.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\dsp.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\dvd.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\exception.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\exi.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\gu.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\gx.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\gx_struct.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\irq.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_messages.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_mutex.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_priority.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_queue.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_sema.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_stack.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_states.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_threadq.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_threads.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_tqdata.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_watchdog.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_wkspace.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\message.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\mutex.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\pad.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\semaphore.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\si.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\sys_state.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\system.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\video.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\video_types.h
# End Source File
# End Group
# Begin Group "ip4.h"

# PROP Default_Filter ""
# Begin Group "lwip1.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\ipv4\lwip\icmp.h
# End Source File
# Begin Source File

SOURCE=.\gc\ipv4\lwip\inet.h
# End Source File
# Begin Source File

SOURCE=.\gc\ipv4\lwip\ip.h
# End Source File
# Begin Source File

SOURCE=.\gc\ipv4\lwip\ip_addr.h
# End Source File
# End Group
# End Group
# Begin Group "lwip.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\lwip\arch.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\debug.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\def.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\dhcp.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\err.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\lwipopts.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\mem.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\memp.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\netif.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\opt.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\pbuf.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\raw.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\sio.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\snmp.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\sockets.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\stats.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\sys.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\tcp.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\tcpip.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\udp.h
# End Source File
# End Group
# Begin Group "netif.h"

# PROP Default_Filter ""
# Begin Group "arch.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\netif\arch\cc.h
# End Source File
# Begin Source File

SOURCE=.\gc\netif\arch\cpu.h
# End Source File
# Begin Source File

SOURCE=.\gc\netif\arch\init.h
# End Source File
# Begin Source File

SOURCE=.\gc\netif\arch\lib.h
# End Source File
# Begin Source File

SOURCE=.\gc\netif\arch\perf.h
# End Source File
# Begin Source File

SOURCE=.\gc\netif\arch\sys_arch.h
# End Source File
# End Group
# Begin Group "gcif.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\netif\gcif\gcif.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\gc\netif\etharp.h
# End Source File
# Begin Source File

SOURCE=.\gc\netif\loopif.h
# End Source File
# End Group
# Begin Group "modplay.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\modplay\bpmtab.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\defines.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\freqtab.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\inctab.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\mixer.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\semitonetab.h
# End Source File
# End Group
# Begin Group "mad.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\mad\bit.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\config.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\decoder.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\fixed.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\frame.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\global.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\huffman.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\layer12.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\layer3.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\mad.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\stream.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\synth.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\timer.h
# End Source File
# Begin Source File

SOURCE=.\gc\mad\version.h
# End Source File
# End Group
# Begin Group "sdcard.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\sdcard\card_buf.h
# End Source File
# Begin Source File

SOURCE=.\gc\sdcard\card_cmn.h
# End Source File
# Begin Source File

SOURCE=.\gc\sdcard\card_fat.h
# End Source File
# Begin Source File

SOURCE=.\gc\sdcard\card_io.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\gc\debug.h
# End Source File
# Begin Source File

SOURCE=.\gc\gccore.h
# End Source File
# Begin Source File

SOURCE=.\gc\gcmodplay.h
# End Source File
# Begin Source File

SOURCE=.\gc\gctypes.h
# End Source File
# Begin Source File

SOURCE=.\gc\madplayer.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay.h
# End Source File
# Begin Source File

SOURCE=.\gc\network.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogcsys.h
# End Source File
# Begin Source File

SOURCE=.\gc\sdcard.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\libogc.dox
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=.\ogc.ld
# End Source File
# Begin Source File

SOURCE=.\specs.ogc
# End Source File
# End Target
# End Project
