#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
endif

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

export PATH	:=	$(DEVKITPPC)/bin:$(PATH)

export LIBOGC_MAJOR	:= 1
export LIBOGC_MINOR	:= 7
export LIBOGC_PATCH	:= 1

#---------------------------------------------------------------------------------
PREFIX	:=	powerpc-gekko
#---------------------------------------------------------------------------------

CC		:=	$(PREFIX)-gcc
CXX		:=	$(PREFIX)-g++
AS		:=	$(PREFIX)-as
AR		:=	$(PREFIX)-ar
LD		:=	$(PREFIX)-ld
OBJCOPY		:=	$(PREFIX)-objcopy

BUILD		:=	build

GCC_VERSION	:=	$(shell $(DEVKITPPC)/bin/$(CC) -dumpversion)
DATESTRING	:=	$(shell date +%Y%m%d)
VERSTRING	:=	$(LIBOGC_MAJOR).$(LIBOGC_MINOR).$(LIBOGC_PATCH)

#---------------------------------------------------------------------------------
ifeq ($(strip $(PLATFORM)),)
#---------------------------------------------------------------------------------
export BASEDIR		:= $(CURDIR)
export LWIPDIR		:= $(BASEDIR)/lwip
export OGCDIR		:= $(BASEDIR)/libogc
export MODDIR		:= $(BASEDIR)/libmodplay
export MADDIR		:= $(BASEDIR)/libmad
export SAMPLEDIR	:= $(BASEDIR)/libsamplerate
export DBDIR		:= $(BASEDIR)/libdb
export DIDIR		:= $(BASEDIR)/libdi
export BTEDIR		:= $(BASEDIR)/lwbt
export WIIUSEDIR	:= $(BASEDIR)/wiiuse
export TINYSMBDIR	:= $(BASEDIR)/libtinysmb
export LIBZDIR		:= $(BASEDIR)/libz
export LIBASNDDIR	:= $(BASEDIR)/libasnd
export STUBSDIR		:= $(BASEDIR)/lockstubs
export DEPS			:=	$(BASEDIR)/deps
export LIBS			:=	$(BASEDIR)/lib

export INCDIR		:=	$(BASEDIR)/include

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

export LIBDIR		:= $(LIBS)/$(PLATFORM)
export DEPSDIR		:=	$(DEPS)/$(PLATFORM)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------


#---------------------------------------------------------------------------------
BBALIB		:= $(LIBDIR)/libbba
OGCLIB		:= $(LIBDIR)/libogc
MODLIB		:= $(LIBDIR)/libmodplay
MADLIB		:= $(LIBDIR)/libmad
DBLIB		:= $(LIBDIR)/libdb
DILIB		:= $(LIBDIR)/libdi
BTELIB		:= $(LIBDIR)/libbte
WIIUSELIB	:= $(LIBDIR)/libwiiuse
TINYSMBLIB	:= $(LIBDIR)/libtinysmb
ZLIB		:= $(LIBDIR)/libz
ASNDLIB		:= $(LIBDIR)/libasnd
STUBSLIB	:= $(LIBDIR)/libgclibstubs

#---------------------------------------------------------------------------------
DEFINCS		:= -I$(BASEDIR) -I$(BASEDIR)/gc
INCLUDES	:=	$(DEFINCS) -I$(BASEDIR)/gc/netif -I$(BASEDIR)/gc/ipv4 \
				-I$(BASEDIR)/gc/ogc -I$(BASEDIR)/gc/ogc/machine \
				-I$(BASEDIR)/gc/modplay -I$(BASEDIR)/gc/mad \
				-I$(BASEDIR)/gc/z -I$(BASEDIR)/gc/bte \
				-I$(BASEDIR)/gc/sdcard -I$(BASEDIR)/gc/wiiuse \
				-I$(BASEDIR)/gc/di

MACHDEP		:= -DBIGENDIAN -DGEKKO -mcpu=750 -meabi -msdata=eabi -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections

ifeq ($(PLATFORM),wii)
MACHDEP		+=	-DHW_RVL
endif

ifeq ($(PLATFORM),cube)
MACHDEP		+=	-DHW_DOL
endif

CFLAGS		:= -DLIBOGC_INTERNAL -DGAMECUBE -O2 -Wall $(MACHDEP) -fno-strict-aliasing $(INCLUDES)


#---------------------------------------------------------------------------------
VPATH :=	$(LWIPDIR)				\
			$(LWIPDIR)/arch/gc		\
			$(LWIPDIR)/arch/gc/netif	\
			$(LWIPDIR)/core			\
			$(LWIPDIR)/core/ipv4	\
			$(LWIPDIR)/netif	\
			$(OGCDIR)			\
			$(MODDIR)			\
			$(MADDIR)			\
			$(SAMPLEDIR)			\
			$(DBDIR)			\
			$(DBDIR)/uIP		\
			$(DIDIR)		\
			$(BTEDIR)		\
			$(WIIUSEDIR)		\
			$(SDCARDDIR)			\
			$(TINYSMBDIR)		\
			$(LIBZDIR)		\
			$(LIBASNDDIR)		\
			$(STUBSDIR)


#---------------------------------------------------------------------------------
LWIPOBJ		:=	network.o netio.o gcif.o	\
			inet.o mem.o dhcp.o raw.o		\
			memp.o netif.o pbuf.o stats.o	\
			sys.o tcp.o tcp_in.o tcp_out.o	\
			udp.o icmp.o ip.o ip_frag.o		\
			ip_addr.o etharp.o loopif.o

#---------------------------------------------------------------------------------
OGCOBJ		:=	\
			console.o  lwp_priority.o lwp_queue.o lwp_threadq.o lwp_threads.o lwp_sema.o	\
			lwp_messages.o lwp.o lwp_handler.o lwp_stack.o lwp_mutex.o 	\
			lwp_watchdog.o lwp_wkspace.o lwp_objmgr.o lwp_heap.o sys_state.o \
			exception_handler.o exception.o irq.o irq_handler.o semaphore.o \
			video_asm.o video.o pad.o dvd.o exi.o mutex.o arqueue.o	arqmgr.o	\
			cache_asm.o system.o system_asm.o cond.o			\
			gx.o gu.o gu_psasm.o audio.o cache.o decrementer.o			\
			message.o card.o aram.o depackrnc.o decrementer_handler.o	\
			depackrnc1.o dsp.o si.o tpl.o ipc.o ogc_crt0.o \
			console_font_8x16.o timesupp.o lock_supp.o newlibc.o usbgecko.o \
			sbrk.o malloc_lock.o kprintf.o stm.o ios.o es.o isfs.o usb.o network_common.o \
			sdgecko_io.o sdgecko_buf.o gcsd.o argv.o network_wii.o wiisd.o conf.o usbstorage.o \
			texconv.o wiilaunch.o

#---------------------------------------------------------------------------------
MODOBJ		:=	freqtab.o mixer.o modplay.o semitonetab.o gcmodplay.o

#---------------------------------------------------------------------------------
MADOBJ		:=	mp3player.o bit.o decoder.o fixed.o frame.o huffman.o \
			layer12.o layer3.o stream.o synth.o timer.o \
			version.o

#---------------------------------------------------------------------------------
DBOBJ		:=	uip_ip.o uip_tcp.o uip_pbuf.o uip_netif.o uip_arp.o uip_arch.o \
				uip_icmp.o memb.o memr.o bba.o tcpip.o debug.o debug_handler.o \
				debug_supp.o geckousb.o
#---------------------------------------------------------------------------------
DIOBJ		:=	di.o di_read.o stubasm.o stubload.o 

#---------------------------------------------------------------------------------
BTEOBJ		:=	bte.o hci.o l2cap.o btmemb.o btmemr.o btpbuf.o physbusif.o

#---------------------------------------------------------------------------------
WIIUSEOBJ	:=	classic.o dynamics.o events.o guitar_hero_3.o io.o io_wii.o ir.o \
				nunchuk.o wiiboard.o wiiuse.o wpad.o

#---------------------------------------------------------------------------------
TINYSMBOBJ	:=	des.o md4.o ntlm.o smb.o smb_devoptab.o

#---------------------------------------------------------------------------------
ZLIBOBJ		:=	adler32.o compress.o crc32.o gzio.o uncompr.o \
			deflate.o trees.o zutil.o inflate.o infback.o \
			inftrees.o inffast.o

#---------------------------------------------------------------------------------
ASNDLIBOBJ	:=	asndlib.o

#---------------------------------------------------------------------------------
# Build rules:
#---------------------------------------------------------------------------------
%.o : %.c
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -Wa,-mgekko -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.cpp
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -Wa,-mgekko -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.S
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -D_LANGUAGE_ASSEMBLY -Wa,-mgekko -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(AS) -mgekko -Qy $< -o $@

#---------------------------------------------------------------------------------
%.a:
#---------------------------------------------------------------------------------
	$(AR) -rc $@ $^

all: wii cube 

#---------------------------------------------------------------------------------
wii: gc/ogc/libversion.h
#---------------------------------------------------------------------------------
	@[ -d $(INCDIR) ] || mkdir -p $(INCDIR)
	@[ -d $(LIBS)/wii ] || mkdir -p $(LIBS)/wii
	@[ -d $(DEPS)/wii ] || mkdir -p $(DEPS)/wii
	@[ -d wii ] || mkdir -p wii
	@$(MAKE) PLATFORM=wii libs -C wii -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
cube: gc/ogc/libversion.h
#---------------------------------------------------------------------------------
	@[ -d $(INCDIR) ] || mkdir -p $(INCDIR)
	@[ -d $(LIBS)/cube ] || mkdir -p $(LIBS)/cube
	@[ -d $(DEPS)/cube ] || mkdir -p $(DEPS)/cube
	@[ -d cube ] || mkdir -p cube
	@$(MAKE) PLATFORM=cube libs -C cube -f $(CURDIR)/Makefile


#---------------------------------------------------------------------------------
gc/ogc/libversion.h : Makefile
#---------------------------------------------------------------------------------
	@echo "#ifndef __LIBVERSION_H__" > $@
	@echo "#define __LIBVERSION_H__" >> $@
	@echo >> $@
	@echo "#define _V_MAJOR_	$(LIBOGC_MAJOR)" >> $@
	@echo "#define _V_MINOR_	$(LIBOGC_MINOR)" >> $@
	@echo "#define _V_PATCH_	$(LIBOGC_PATCH)" >> $@
	@echo >> $@
	@echo "#define _V_DATE_			__DATE__" >> $@
	@echo "#define _V_TIME_			__TIME__" >> $@
	@echo >> $@
	@echo '#define _V_STRING "libOGC Release '$(LIBOGC_MAJOR).$(LIBOGC_MINOR).$(LIBOGC_PATCH)'"' >> $@
	@echo >> $@
	@echo "#endif // __LIBVERSION_H__" >> $@

#---------------------------------------------------------------------------------
$(BBALIB).a: $(LWIPOBJ)
#---------------------------------------------------------------------------------
$(OGCLIB).a: $(OGCOBJ)
#---------------------------------------------------------------------------------
$(MP3LIB).a: $(MP3OBJ)
#---------------------------------------------------------------------------------
$(MODLIB).a: $(MODOBJ)
#---------------------------------------------------------------------------------
$(MADLIB).a: $(MADOBJ)
#---------------------------------------------------------------------------------
$(DBLIB).a: $(DBOBJ)
#---------------------------------------------------------------------------------
$(DILIB).a: $(DIOBJ)
#---------------------------------------------------------------------------------
$(TINYSMBLIB).a: $(TINYSMBOBJ)
#---------------------------------------------------------------------------------
$(ZLIB).a: $(ZLIBOBJ)
#---------------------------------------------------------------------------------
$(ASNDLIB).a: $(ASNDLIBOBJ)
#---------------------------------------------------------------------------------
$(BTELIB).a: $(BTEOBJ)
#---------------------------------------------------------------------------------
$(WIIUSELIB).a: $(WIIUSEOBJ)
#---------------------------------------------------------------------------------
 
.PHONY: libs wii cube install-headers install dist docs

#---------------------------------------------------------------------------------
install-headers:
#---------------------------------------------------------------------------------
	@mkdir -p $(INCDIR)
	@mkdir -p $(INCDIR)/ogc/machine
	@mkdir -p $(INCDIR)/bte
	@mkdir -p $(INCDIR)/wiiuse
	@mkdir -p $(INCDIR)/modplay
	@mkdir -p $(INCDIR)/mad
	@mkdir -p $(INCDIR)/sdcard
	@mkdir -p $(INCDIR)/di
	@cp ./gc/*.h $(INCDIR)
	@cp ./gc/ogc/*.h $(INCDIR)/ogc
	@cp ./gc/ogc/machine/*.h $(INCDIR)/ogc/machine
	@cp ./gc/bte/*.h $(INCDIR)/bte
	@cp ./gc/wiiuse/*.h $(INCDIR)/wiiuse
	@cp ./gc/modplay/*.h $(INCDIR)/modplay
	@cp ./gc/mad/*.h $(INCDIR)/mad
	@cp ./gc/sdcard/*.h $(INCDIR)/sdcard
	@cp ./gc/di/*.h $(INCDIR)/di

#---------------------------------------------------------------------------------
install: install-headers
#---------------------------------------------------------------------------------
	@mkdir -p $(DEVKITPRO)/libogc
	@cp -frv include $(DEVKITPRO)/libogc
	@cp -frv lib $(DEVKITPRO)/libogc
	@cp -frv libogc_license.txt $(DEVKITPRO)/libogc


#---------------------------------------------------------------------------------
dist: install-headers
#---------------------------------------------------------------------------------
	@tar    --exclude=*CVS* --exclude=wii --exclude=cube --exclude=*deps* \
		--exclude=*.bz2  --exclude=*include* --exclude=*lib/* --exclude=*docs/*\
		-cvjf libogc-src-$(VERSTRING).tar.bz2 *
	@tar -cvjf libogc-$(VERSTRING).tar.bz2 include lib libogc_license.txt


LIBRARIES	:=	$(OGCLIB).a  $(MODLIB).a $(MADLIB).a $(DBLIB).a $(ZLIB).a $(TINYSMBLIB).a $(ASNDLIB).a

ifeq ($(PLATFORM),cube)
LIBRARIES	+=	$(BBALIB).a 
endif
ifeq ($(PLATFORM),wii)
LIBRARIES	+=	$(BTELIB).a $(WIIUSELIB).a $(DILIB).a
endif

#---------------------------------------------------------------------------------
libs: $(LIBRARIES)
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	rm -fr wii cube
	rm -fr $(DEPS)
	rm -fr $(LIBS)
	rm -fr $(INCDIR)
	rm -f *.map

#---------------------------------------------------------------------------------
docs: install-headers
#---------------------------------------------------------------------------------
	doxygen libogc.dox

-include $(DEPSDIR)/*.d
