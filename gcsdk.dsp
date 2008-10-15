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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ /c
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

SOURCE=.\libogc\argv.c
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

SOURCE=.\libogc\conf.c
# End Source File
# Begin Source File

SOURCE=.\libogc\console.c
# End Source File
# Begin Source File

SOURCE=.\libogc\console.h
# End Source File
# Begin Source File

SOURCE=.\libogc\console_font_8x16.c
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

SOURCE=.\libogc\depackrnc1.c
# End Source File
# Begin Source File

SOURCE=.\libogc\dsp.c
# End Source File
# Begin Source File

SOURCE=.\libogc\dvd.c
# End Source File
# Begin Source File

SOURCE=.\libogc\es.c
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

SOURCE=.\libogc\ios.c
# End Source File
# Begin Source File

SOURCE=.\libogc\ipc.c
# End Source File
# Begin Source File

SOURCE=.\libogc\irq.c
# End Source File
# Begin Source File

SOURCE=.\libogc\irq_handler.S
# End Source File
# Begin Source File

SOURCE=.\libogc\isfs.c
# End Source File
# Begin Source File

SOURCE=.\libogc\kprintf.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lock_supp.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_handler.S
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_heap.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_heap.inl
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

SOURCE=.\libogc\lwp_objmgr.c
# End Source File
# Begin Source File

SOURCE=.\libogc\lwp_objmgr.inl
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

SOURCE=.\libogc\malloc_lock.c
# End Source File
# Begin Source File

SOURCE=.\libogc\message.c
# End Source File
# Begin Source File

SOURCE=.\libogc\mutex.c
# End Source File
# Begin Source File

SOURCE=.\libogc\network_common.c
# End Source File
# Begin Source File

SOURCE=.\libogc\network_wii.c
# End Source File
# Begin Source File

SOURCE=.\libogc\newlibc.c
# End Source File
# Begin Source File

SOURCE=.\libogc\ogc_crt0.S
# End Source File
# Begin Source File

SOURCE=.\libogc\pad.c
# End Source File
# Begin Source File

SOURCE=.\libogc\sbrk.c
# End Source File
# Begin Source File

SOURCE=.\libogc\sdgecko_buf.c
# End Source File
# Begin Source File

SOURCE=.\libogc\sdgecko_io.c
# End Source File
# Begin Source File

SOURCE=.\libogc\semaphore.c
# End Source File
# Begin Source File

SOURCE=.\libogc\si.c
# End Source File
# Begin Source File

SOURCE=.\libogc\stm.c
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

SOURCE=.\libogc\tdf.c
# End Source File
# Begin Source File

SOURCE=.\libogc\texconv.c
# End Source File
# Begin Source File

SOURCE=.\libogc\timesupp.c
# End Source File
# Begin Source File

SOURCE=.\libogc\timesupp.h
# End Source File
# Begin Source File

SOURCE=.\libogc\usb.c
# End Source File
# Begin Source File

SOURCE=.\libogc\usbgecko.c
# End Source File
# Begin Source File

SOURCE=.\libogc\usbstorage.c
# End Source File
# Begin Source File

SOURCE=.\libogc\video.c
# End Source File
# Begin Source File

SOURCE=.\libogc\video_asm.S
# End Source File
# Begin Source File

SOURCE=.\libogc\wiilaunch.c
# End Source File
# Begin Source File

SOURCE=.\libogc\wiisd.c
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

SOURCE=.\libmodplay\freqtab.c
# End Source File
# Begin Source File

SOURCE=.\libmodplay\gcmodplay.c
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

SOURCE=.\libmad\mp3player.c
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
# Begin Group "uIP.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libdb\uIP\bba.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\bba.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\memb.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\memb.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\memr.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\memr.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_arch.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_arch.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_arp.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_arp.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_icmp.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_icmp.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_ip.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_ip.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_netif.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_netif.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_pbuf.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_pbuf.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_tcp.c
# End Source File
# Begin Source File

SOURCE=.\libdb\uIP\uip_tcp.h
# End Source File
# Begin Source File

SOURCE=.\libdb\uip\uipopt.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\libdb\debug.c
# End Source File
# Begin Source File

SOURCE=.\libdb\debug_handler.S
# End Source File
# Begin Source File

SOURCE=.\libdb\debug_if.h
# End Source File
# Begin Source File

SOURCE=.\libdb\debug_supp.c
# End Source File
# Begin Source File

SOURCE=.\libdb\debug_supp.h
# End Source File
# Begin Source File

SOURCE=.\libdb\geckousb.c
# End Source File
# Begin Source File

SOURCE=.\libdb\geckousb.h
# End Source File
# Begin Source File

SOURCE=.\libdb\tcpip.c
# End Source File
# Begin Source File

SOURCE=.\libdb\tcpip.h
# End Source File
# End Group
# Begin Group "lockstubs.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\lockstubs\flock_supp_stub.c
# End Source File
# Begin Source File

SOURCE=.\lockstubs\gcn_crt0.S
# End Source File
# Begin Source File

SOURCE=.\lockstubs\lock_supp_stub.c
# End Source File
# Begin Source File

SOURCE=.\lockstubs\malloc_lock_stub.c
# End Source File
# End Group
# Begin Group "libtinysmb.c"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\libtinysmb\des.c
# End Source File
# Begin Source File

SOURCE=.\libtinysmb\lmhash.c
# End Source File
# Begin Source File

SOURCE=.\libtinysmb\smb.c
# End Source File
# End Group
# Begin Group "libz.c"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\libz\adler32.c
# End Source File
# Begin Source File

SOURCE=.\libz\compress.c
# End Source File
# Begin Source File

SOURCE=.\libz\crc32.c
# End Source File
# Begin Source File

SOURCE=.\libz\deflate.c
# End Source File
# Begin Source File

SOURCE=.\libz\gzio.c
# End Source File
# Begin Source File

SOURCE=.\libz\infback.c
# End Source File
# Begin Source File

SOURCE=.\libz\inffast.c
# End Source File
# Begin Source File

SOURCE=.\libz\inflate.c
# End Source File
# Begin Source File

SOURCE=.\libz\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\libz\trees.c
# End Source File
# Begin Source File

SOURCE=.\libz\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\libz\zutil.c
# End Source File
# End Group
# Begin Group "lwbt.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\lwbt\bt.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\btarch.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\bte.c
# End Source File
# Begin Source File

SOURCE=.\lwbt\btmemb.c
# End Source File
# Begin Source File

SOURCE=.\lwbt\btmemb.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\btmemr.c
# End Source File
# Begin Source File

SOURCE=.\lwbt\btmemr.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\btopt.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\btpbuf.c
# End Source File
# Begin Source File

SOURCE=.\lwbt\btpbuf.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\hci.c
# End Source File
# Begin Source File

SOURCE=.\lwbt\hci.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\l2cap.c
# End Source File
# Begin Source File

SOURCE=.\lwbt\l2cap.h
# End Source File
# Begin Source File

SOURCE=.\lwbt\physbusif.c
# End Source File
# Begin Source File

SOURCE=.\lwbt\physbusif.h
# End Source File
# End Group
# Begin Group "wiiuse.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\wiiuse\classic.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\classic.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\definitions.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\dynamics.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\dynamics.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\events.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\events.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\guitar_hero_3.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\guitar_hero_3.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\io.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\io.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\io_wii.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\ir.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\ir.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\nunchuk.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\nunchuk.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\os.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\wiiuse.c
# End Source File
# Begin Source File

SOURCE=.\wiiuse\wiiuse_internal.h
# End Source File
# Begin Source File

SOURCE=.\wiiuse\wpad.c
# End Source File
# End Group
# Begin Group "libdi.c"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libdi\di.c
# End Source File
# Begin Source File

SOURCE=.\libdi\di_read.c
# End Source File
# Begin Source File

SOURCE=.\libdi\stubasm.h
# End Source File
# Begin Source File

SOURCE=.\libdi\stubasm.S
# End Source File
# Begin Source File

SOURCE=.\libdi\stubload.c
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

SOURCE=.\gc\ogc\color.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\cond.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\conf.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\consol.h
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

SOURCE=.\gc\ogc\es.h
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

SOURCE=.\gc\ogc\ios.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\ipc.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\irq.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\isfs.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_config.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_heap.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_messages.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_mutex.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\lwp_objmgr.h
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

SOURCE=.\gc\ogc\stm.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\sys_state.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\system.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\tdf.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\usb.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\usbgecko.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\usbstorage.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\video.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\video_types.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogc\wiilaunch.h
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

SOURCE=.\gc\lwip\api.h
# End Source File
# Begin Source File

SOURCE=.\gc\lwip\api_msg.h
# End Source File
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

SOURCE=.\gc\modplay\defines.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\freqtab.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\mixer.h
# End Source File
# Begin Source File

SOURCE=.\gc\modplay\modplay.h
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
# Begin Group "tinysmb.h"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\gc\tinysmb\DES.h
# End Source File
# Begin Source File

SOURCE=.\gc\tinysmb\LMhash.h
# End Source File
# End Group
# Begin Group "libz.h"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\gc\z\crc32.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\deflate.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\inffast.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\inflate.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\trees.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\zconf.in.h
# End Source File
# Begin Source File

SOURCE=.\gc\z\zutil.h
# End Source File
# End Group
# Begin Group "bte.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\bte\bd_addr.h
# End Source File
# Begin Source File

SOURCE=.\gc\bte\bte.h
# End Source File
# End Group
# Begin Group "wiiuse.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\wiiuse\wiiuse.h
# End Source File
# Begin Source File

SOURCE=.\gc\wiiuse\wpad.h
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

SOURCE=.\gc\sdcard\card_io.h
# End Source File
# Begin Source File

SOURCE=.\gc\sdcard\wiisd_io.h
# End Source File
# End Group
# Begin Group "libdi.h"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gc\di\di.h
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

SOURCE=.\gc\gcutil.h
# End Source File
# Begin Source File

SOURCE=.\gc\mp3player.h
# End Source File
# Begin Source File

SOURCE=.\gc\network.h
# End Source File
# Begin Source File

SOURCE=.\gc\ogcsys.h
# End Source File
# Begin Source File

SOURCE=.\gc\smb.h
# End Source File
# Begin Source File

SOURCE=.\gc\zconf.h
# End Source File
# Begin Source File

SOURCE=.\gc\zlib.h
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
# End Target
# End Project
