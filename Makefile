.SUFFIXES:

#---------------------------------------------------------------------------------
ifeq ($(USERNAME),davem)
#---------------------------------------------------------------------------------
export PATH:=/c/devkitPPC_r8/bin:/bin
PREFIX	:=	powerpc-elf-
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
PREFIX	:=	powerpc-eabi-elf-
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------



CC		:=	$(PREFIX)gcc
CXX		:=	$(PREFIX)g++
AS		:=	$(PREFIX)as
AR		:=	$(PREFIX)ar
LD		:=	$(PREFIX)ld
OBJCOPY	:=	$(PREFIX)objcopy

BUILD	:=	build
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

#---------------------------------------------------------------------------------
DEFINCS		:= -I$(BASEDIR) -I$(BASEDIR)/gc
INCLUDES	:=	$(DEFINCS) -I$(BASEDIR)/gc/netif -I$(BASEDIR)/gc/ipv4 \
				-I$(BASEDIR)/gc/ogc -I$(BASEDIR)/gc/ogc/machine \
				-I$(BASEDIR)/gc/modplay -I$(BASEDIR)/gc/mad -I$(BASEDIR)/gc/sdcard

MACHDEP		:= -DGEKKO -mcpu=750 -meabi -msdata=eabi -mhard-float
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
			$(SDCARDDIR)			\
			$(GCSYSDIR)


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
			cache_asm.o system.o system_asm.o cond.o processor64.o			\
			gx.o gu.o gu_psasm.o audio.o cache.o decrementer.o			\
			message.o card.o aram.o depackrnc.o decrementer_handler.o	\
			dsp.o ogc_crt0.o

#---------------------------------------------------------------------------------
MODOBJ	:=	freqtab.o mixer.o modplay.o semitonetab.o gcmodplay.o \
			bpmtab32.o bpmtab48.o \
			inctab32.o inctab48.o

#---------------------------------------------------------------------------------
MADOBJ	:=	bit.o decoder.o fixed.o frame.o huffman.o \
			layer12.o layer3.o stream.o synth.o timer.o \
			version.o madplayer.o

#---------------------------------------------------------------------------------
DBOBJ	:=	debug_handler.o debug.o

#---------------------------------------------------------------------------------
SDCARDOBJ	:=	fat.o fat_ops.o

#---------------------------------------------------------------------------------
GCSYSOBJ	:=	newlibc.o sbrk.o open.o write.o close.o \
				getpid.o kill.o isatty.o fstat.o read.o \
				lseek.o sleep.o usleep.o timesupp.o \
				malloc_lock.o console.o console_font.o \
				console_font_8x8.o iosupp.o netio_fake.o \
				stdin_fake.o

#---------------------------------------------------------------------------------
# Build rules:
#---------------------------------------------------------------------------------
%.o : %.c
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.cpp
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.S
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(CC) -MMD -MF $(DEPSDIR)/$*.d $(CFLAGS) -D_LANGUAGE_ASSEMBLY -c $< -o $@

#---------------------------------------------------------------------------------
%.o : %.s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(AS) -mppc -Qy $< -o $(OBJDIR)/$@

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

.PHONEY: libs install dist

#---------------------------------------------------------------------------------
install:
#---------------------------------------------------------------------------------
	@mkdir -p $(INCDIR)
	@mkdir -p $(INCDIR)/ogc
	@mkdir -p $(INCDIR)/modplay
	@mkdir -p $(INCDIR)/mad
	@mkdir -p $(INCDIR)/sdcard
	cp ./gc/*.h $(INCDIR)
	cp ./gc/ogc/*.h $(INCDIR)/ogc
	cp ./gc/modplay/*.h $(INCDIR)/modplay
	cp ./gc/mad/*.h $(INCDIR)/mad
	cp ./gc/sdcard/*.h $(INCDIR)/sdcard

#---------------------------------------------------------------------------------
dist:
#---------------------------------------------------------------------------------
	tar -cjf libogc.tar.bz2 include lib license.txt

#---------------------------------------------------------------------------------
libs: $(OGCLIB).a $(BBALIB).a $(MODLIB).a $(MADLIB).a $(DBLIB).a $(SDCARDLIB).a $(GCSYSLIB).a
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
clean:
#---------------------------------------------------------------------------------
	rm -fr $(BUILDDIR)
	rm -fr $(DEPSDIR)
	rm -f *.map

-include $(DEPSDIR)/*.d
