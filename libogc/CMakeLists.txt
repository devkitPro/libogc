
add_library(ogc STATIC
	lwpcompat/lwpc_alarm.c
	lwpcompat/lwpc_queue.c
	lwpcompat/lwpc_thread.c
	lwpcompat/lwpc_sync.c
	lwpcompat/lwpc_sem.c
	lwpcompat/lwpc_mq.c

	ogc_sockets/soc_common.c
	ogc_sockets/soc_socket.c

	console.c
	lwp_queue.c
	lwp_wkspace.c
	lwp_heap.c
	video.c
	pad.c
	dvd.c
	exi.c
	arqueue.c
	arqmgr.c
	cache_asm.S
	system.c
	gx.c
	gu.c
	gu_psasm.S
	gu_ps_concat44.S
	audio.c
	cache.c
	card.c
	aram.c
	depackrnc.S
	depackrnc1.c
	dsp.c
	si.c
	tpl.c
	ipc.c
	console_font_8x16.c
	timesupp.c
	usbgecko.c
	usbmouse.c
	sbrk.c
	kprintf.c
	stm.c
	aes.c
	sha.c
	ios.c
	es.c
	isfs.c
	usb.c
	network_common.c
	sdgecko_io.c
	sdgecko_buf.c
	gcsd.c
	argv.c
	network_wii.c
	wiisd.c
	conf.c
	usbstorage.c
	texconv.c
	wiilaunch.c
	wd.c
	sys_report.c
	default_panic.c
)

target_link_libraries(ogc PRIVATE libogc_inc tuxedo)

libogc_install_lib(ogc)
