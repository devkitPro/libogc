.SUFFIXES:

#---------------------------------------------------------------------------------
PREFIX	:=	powerpc-gekko
#PREFIX	:=	powerpc-eabi-elf
#---------------------------------------------------------------------------------



CC		:=	$(PREFIX)-gcc
CXX		:=	$(PREFIX)-g++
AS		:=	$(PREFIX)-as
AR		:=	$(PREFIX)-ar
LD		:=	$(PREFIX)-ld
OBJCOPY	:=	$(PREFIX)-objcopy

BUILD	:=	build

GETPATH		:=	$(CC) -v 2>&1 | sed -n -e 's/Reading specs from //p' | sed -e 's/\/lib.*//'
INSTALLPATH	:=	$(shell $(GETPATH))
GCC_VERSION	:=	$(shell $(CC) -dumpversion)

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
export DBDIR		:= $(BASEDIR)/libdb
export SDCARDDIR	:= $(BASEDIR)/libsdcard
export GCSYSDIR		:= $(BASEDIR)/libogcsys
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
STUBSLIB	:= $(LIBDIR)/libgclibstubs
GCN_CRT0	:= $(LIBDIR)/gcn_crt0
#---------------------------------------------------------------------------------
DEFINCS		:= -I$(BASEDIR) -I$(BASEDIR)/gc
INCLUDES	:=	$(DEFINCS) -I$(BASEDIR)/gc/netif -I$(BASEDIR)/gc/ipv4 \
				-I$(BASEDIR)/gc/ogc -I$(BASEDIR)/gc/ogc/machine \
				-I$(BASEDIR)/gc/modplay -I$(BASEDIR)/gc/mad -I$(BASEDIR)/gc/sdcard

MACHDEP		:= -DBIGENDIAN -DGEKKO -mcpu=750 -meabi -msdata=eabi -mhard-float -ffunction-sections -fdata-sections
CFLAGS		:= -DGAMECUBE -O2 $(MACHDEP) -Wall $(INCLUDES)
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
			$(DBDIR)			\
			$(DBDIR)/uIP		\
			$(SDCARDDIR)			\
			$(GCSYSDIR)		\
			$(STUBSDIR)


#---------------------------------------------------------------------------------
LWIPOBJ	:=	network.o netio.o gcif.o lib_arch.o		\
			inet.o mem.o dhcp.o raw.o		\
			memp.o netif.o pbuf.o stats.o	\
			sys.o tcp.o tcp_in.o tcp_out.o	\
			udp.o icmp.o ip.o ip_frag.o		\
			ip_addr.o etharp.o loopif.o

#---------------------------------------------------------------------------------
OGCOBJ	:=	lwp_priority.o lwp_queue.o lwp_threadq.o lwp_threads.o lwp_sema.o	\
			lwp_messages.o lwp.o lwp_handler.o lwp_stack.o lwp_mutex.o 	\
			lwp_watchdog.o lwp_wkspace.o sys_state.o \
			exception_handler.o exception.o irq.o irq_handler.o semaphore.o \
			video_asm.o video.o pad.o dvd.o exi.o mutex.o arqueue.o	arqmgr.o	\
			cache_asm.o system.o system_asm.o cond.o			\
			gx.o gu.o gu_psasm.o audio.o cache.o decrementer.o			\
			message.o card.o aram.o depackrnc.o decrementer_handler.o	\
			depackrnc1.o dsp.o si.o tdf.o ogc_crt0.o

#---------------------------------------------------------------------------------
MODOBJ	:=	freqtab.o mixer.o modplay.o semitonetab.o gcmodplay.o

#---------------------------------------------------------------------------------
MADOBJ	:=	bit.o decoder.o fixed.o frame.o huffman.o \
			layer12.o layer3.o stream.o synth.o timer.o \
			version.o mp3player.o

#---------------------------------------------------------------------------------
DBOBJ	:=	debug_handler.o debug.o bba_dbg.o uip_arp.o uip_arch.o uip.o 

#---------------------------------------------------------------------------------
SDCARDOBJ	:=	sdcard.o sdcardio.o card_fat.o card_buf.o card_io.o card_uni.o

#---------------------------------------------------------------------------------
GCSYSOBJ	:=	newlibc.o sbrk.o open.o write.o close.o \
				getpid.o kill.o isatty.o fstat.o read.o \
				lseek.o sleep.o usleep.o timesupp.o \
				malloc_lock.o console.o console_font.o \
				console_font_8x8.o iosupp.o netio_fake.o \
				stdin_fake.o sdcardio_fake.o flock_supp.o \
				lock_supp.o
				
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
all:
#---------------------------------------------------------------------------------
	@[ -d $(LIBDIR) ] || mkdir -p $(LIBDIR)
	@[ -d $(DEPSDIR) ] || mkdir -p $(DEPSDIR)
	@[ -d $(BUILDDIR) ] || mkdir -p $(BUILDDIR)
	@make libs -C $(BUILDDIR) -f $(CURDIR)/Makefile

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
$(STUBSLIB).a: $(STUBSOBJ)
#---------------------------------------------------------------------------------
 
.PHONEY: libs install dist

#---------------------------------------------------------------------------------
install:
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
	@cp -frv include $(INSTALLPATH)/$(PREFIX)
	@cp -frv lib $(INSTALLPATH)/$(PREFIX)
	@cp -fv ogc.ld $(INSTALLPATH)/$(PREFIX)/lib/ogc.ld
	@cp -fv vgcogc.ld $(INSTALLPATH)/$(PREFIX)/lib/vgcogc.ld
	@cp -fv specs.ogc $(INSTALLPATH)/lib/gcc/$(PREFIX)/$(GCC_VERSION)/specs

#---------------------------------------------------------------------------------
dist:
#---------------------------------------------------------------------------------
	@tar -cvjf libogc.tar.bz2 include lib license.txt

#---------------------------------------------------------------------------------
libs: $(OGCLIB).a $(BBALIB).a $(MODLIB).a $(MADLIB).a $(DBLIB).a $(SDCARDLIB).a $(GCSYSLIB).a $(STUBSLIB).a
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	rm -fr $(BUILDDIR)
	rm -fr $(DEPSDIR)
	rm -f *.map

-include $(DEPSDIR)/*.d
