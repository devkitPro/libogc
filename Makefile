#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC)
endif

export PATH	:=	$(DEVKITPPC)/bin:$(PATH)

export LIBOGC_MAJOR	:= 1
export LIBOGC_MINOR	:= 5
export LIBOGC_PATCH	:= 0

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
DATESTRING	:=	$(shell date +%Y)$(shell date +%m)$(shell date +%d)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
export BASEDIR		:= $(CURDIR)
export BUILDDIR		:= $(BASEDIR)/$(BUILD)
export LIBDIR		:= $(BASEDIR)/lib
export LWIPDIR		:= $(BASEDIR)/lwip
export OGCDIR		:= $(BASEDIR)/libogc
export MODDIR		:= $(BASEDIR)/libmodplay
export MADDIR		:= $(BASEDIR)/libmad
export SAMPLEDIR	:= $(BASEDIR)/libsamplerate
export DBDIR		:= $(BASEDIR)/libdb
export SDCARDDIR	:= $(BASEDIR)/libsdcard
export GCSYSDIR		:= $(BASEDIR)/libogcsys
export TINYSMBDIR	:= $(BASEDIR)/libtinysmb
export LIBZDIR		:= $(BASEDIR)/libz
export STUBSDIR		:= $(BASEDIR)/lockstubs

export DEPSDIR		:=	$(BASEDIR)/deps
export INCDIR		:=	$(BASEDIR)/include
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
BBALIB		:= $(LIBDIR)/libbba
OGCLIB		:= $(LIBDIR)/libogc
MODLIB		:= $(LIBDIR)/libmodplay
MADLIB		:= $(LIBDIR)/libmad
DBLIB		:= $(LIBDIR)/libdb
SDCARDLIB	:= $(LIBDIR)/libsdcard
GCSYSLIB	:= $(LIBDIR)/libogcsys
TINYSMBLIB	:= $(LIBDIR)/libtinysmb
ZLIB		:= $(LIBDIR)/libz
STUBSLIB	:= $(LIBDIR)/libgclibstubs
GCN_CRT0	:= $(LIBDIR)/gcn_crt0
#---------------------------------------------------------------------------------
DEFINCS		:= -I$(BASEDIR) -I$(BASEDIR)/gc
INCLUDES	:=	$(DEFINCS) -I$(BASEDIR)/gc/netif -I$(BASEDIR)/gc/ipv4 \
				-I$(BASEDIR)/gc/ogc -I$(BASEDIR)/gc/ogc/machine -I$(BASEDIR)/gc/tinysmb \
				-I$(BASEDIR)/gc/modplay -I$(BASEDIR)/gc/mad -I$(BASEDIR)/gc/sdcard \
				-I$(BASEDIR)/gc/z

MACHDEP		:= -DBIGENDIAN -DGEKKO -mcpu=750 -meabi -msdata=eabi -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections
CFLAGS		:= -DLIBOGC_INTERNAL -DGAMECUBE -g -O2 -Wall $(MACHDEP) -fno-strict-aliasing $(INCLUDES)
LDFLAGS		:=

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
			$(SDCARDDIR)			\
			$(GCSYSDIR)		\
			$(TINYSMBDIR)		\
			$(LIBZDIR)		\
			$(STUBSDIR)


#---------------------------------------------------------------------------------
LWIPOBJ		:=	network.o netio.o gcif.o	\
			inet.o mem.o dhcp.o raw.o		\
			memp.o netif.o pbuf.o stats.o	\
			sys.o tcp.o tcp_in.o tcp_out.o	\
			udp.o icmp.o ip.o ip_frag.o		\
			ip_addr.o etharp.o loopif.o

#---------------------------------------------------------------------------------
OGCOBJ		:=	lwp_priority.o lwp_queue.o lwp_threadq.o lwp_threads.o lwp_sema.o	\
			lwp_messages.o lwp.o lwp_handler.o lwp_stack.o lwp_mutex.o 	\
			lwp_watchdog.o lwp_wkspace.o lwp_objmgr.o lwp_heap.o sys_state.o \
			exception_handler.o exception.o irq.o irq_handler.o semaphore.o \
			video_asm.o video.o pad.o dvd.o exi.o mutex.o arqueue.o	arqmgr.o	\
			cache_asm.o system.o system_asm.o cond.o			\
			gx.o gu.o gu_psasm.o audio.o cache.o decrementer.o			\
			message.o card.o aram.o depackrnc.o decrementer_handler.o	\
			depackrnc1.o dsp.o si.o tdf.o ogc_crt0.o

#---------------------------------------------------------------------------------
MODOBJ		:=	freqtab.o mixer.o modplay.o semitonetab.o gcmodplay.o

#---------------------------------------------------------------------------------
MADOBJ		:=	mp3player.o bit.o decoder.o fixed.o frame.o huffman.o \
			layer12.o layer3.o stream.o synth.o timer.o \
			version.o

#---------------------------------------------------------------------------------
DBOBJ		:=	uip_ip.o uip_tcp.o uip_pbuf.o uip_netif.o uip_arp.o uip_arch.o \
				uip_icmp.o memb.o memr.o bba.o tcpip.o debug.o debug_handler.o \
				debug_supp.o usb.o

#---------------------------------------------------------------------------------
SDCARDOBJ	:=	sdcard.o sdcardio.o card_fat.o card_buf.o card_io.o card_uni.o

#---------------------------------------------------------------------------------
GCSYSOBJ	:=	newlibc.o sbrk.o open.o write.o close.o \
			getpid.o kill.o isatty.o fstat.o read.o \
			lseek.o sleep.o usleep.o timesupp.o \
			console.o console_font.o \
			console_font_8x8.o iosupp.o netio_fake.o \
			stdin_fake.o sdcardio_fake.o flock_supp.o \
			lock_supp.o dvd_supp.o malloc_lock.o
				
#---------------------------------------------------------------------------------
TINYSMBOBJ	:=	des.o lmhash.o smb.o

#---------------------------------------------------------------------------------
ZLIBOBJ		:=	adler32.o compress.o crc32.o gzio.o uncompr.o \
			deflate.o trees.o zutil.o inflate.o infback.o \
			inftrees.o inffast.o

#---------------------------------------------------------------------------------
STUBSOBJ	:=	malloc_lock_stub.o flock_supp_stub.o lock_supp_stub.o gcn_crt0.o

#---------------------------------------------------------------------------------
# Build rules:
#---------------------------------------------------------------------------------
%.o : %.c
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MF $(DEPSDIR)/$*.d $(CFLAGS) -Wa,-mgekko -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.cpp
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MF $(DEPSDIR)/$*.d $(CFLAGS) -Wa,-mgekko -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.S
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MF $(DEPSDIR)/$*.d $(CFLAGS) -D_LANGUAGE_ASSEMBLY -Wa,-mgekko -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(AS) -mgekko -Qy $< -o $@

#---------------------------------------------------------------------------------
%.a:
#---------------------------------------------------------------------------------
	$(AR) -rc $@ $^

#---------------------------------------------------------------------------------
all: gc/ogc/libversion.h
#---------------------------------------------------------------------------------
	@[ -d $(LIBDIR) ] || mkdir -p $(LIBDIR)
	@[ -d $(INCDIR) ] || mkdir -p $(INCDIR)
	@[ -d $(DEPSDIR) ] || mkdir -p $(DEPSDIR)
	@[ -d $(BUILDDIR) ] || mkdir -p $(BUILDDIR)
	@$(MAKE) libs -C $(BUILDDIR) -f $(CURDIR)/Makefile

gc/ogc/libversion.h : Makefile
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
$(SDCARDLIB).a: $(SDCARDOBJ)
#---------------------------------------------------------------------------------
$(GCSYSLIB).a: $(GCSYSOBJ)
#---------------------------------------------------------------------------------
$(TINYSMBLIB).a: $(TINYSMBOBJ)
#---------------------------------------------------------------------------------
$(ZLIB).a: $(ZLIBOBJ)
#---------------------------------------------------------------------------------
$(STUBSLIB).a: $(STUBSOBJ)
#---------------------------------------------------------------------------------
 
.PHONY: libs install-headers install dist docs

#---------------------------------------------------------------------------------
install-headers:
#---------------------------------------------------------------------------------
	@mkdir -p $(INCDIR)
	@mkdir -p $(INCDIR)/ogc
	@mkdir -p $(INCDIR)/modplay
	@mkdir -p $(INCDIR)/mad
	@mkdir -p $(INCDIR)/sdcard
	@cp ./gc/*.h $(INCDIR)
	@cp ./gc/ogc/*.h $(INCDIR)/ogc
	@cp ./gc/modplay/*.h $(INCDIR)/modplay
	@cp ./gc/mad/*.h $(INCDIR)/mad
	@cp ./gc/sdcard/*.h $(INCDIR)/sdcard
	@cp ./*.ld $(LIBDIR)
#---------------------------------------------------------------------------------
install: install-headers
#---------------------------------------------------------------------------------
	@cp -frv include $(DEVKITPPC)/$(PREFIX)
	@cp -frv lib $(DEVKITPPC)/$(PREFIX)
	@cp -fv ogc.ld $(DEVKITPPC)/$(PREFIX)/lib/ogc.ld
	@cp -fv vgcogc.ld $(DEVKITPPC)/$(PREFIX)/lib/vgcogc.ld
	@cp -fv gcbogc.ld $(DEVKITPPC)/$(PREFIX)/lib/gcbogc.ld
	@cp -fv specs.$(GCC_VERSION) $(DEVKITPPC)/lib/gcc/$(PREFIX)/$(GCC_VERSION)/specs

#---------------------------------------------------------------------------------
dist: install-headers
#---------------------------------------------------------------------------------
	@tar    --exclude=*CVS* --exclude=*build* --exclude=*deps* \
		--exclude=*.bz2  --exclude=*include* --exclude=*lib/* --exclude=*docs/*\
		-cvjf libogc-src-$(DATESTRING).tar.bz2 *
	@tar -cvjf libogc-$(DATESTRING).tar.bz2 include lib license.txt

#---------------------------------------------------------------------------------
libs: $(OGCLIB).a $(BBALIB).a $(MODLIB).a $(MADLIB).a $(DBLIB).a $(SDCARDLIB).a $(GCSYSLIB).a $(ZLIB).a $(TINYSMBLIB).a $(STUBSLIB).a
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	rm -fr $(BUILDDIR)
	rm -fr $(DEPSDIR)
	rm -fr $(LIBDIR)
	rm -fr $(INCDIR)
	rm -f *.map
#---------------------------------------------------------------------------------
docs: install-headers
#---------------------------------------------------------------------------------
	doxygen libogc.dox

-include $(DEPSDIR)/*.d
