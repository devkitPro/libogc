#ifndef __OGCSYS_H__
#define __OGCSYS_H__

#include <gccore.h>
#include <sys/types.h>

#ifdef __cplusplus
	extern "C" {
#endif

time_t time(time_t *timer);
unsigned int usleep(unsigned int us);
unsigned int nanosleep(struct timespec *tb);

void console_init(void *framebuffer,int xstart,int ystart,int xres,int yres,int stride);

#ifdef __cplusplus
	}
#endif

#endif
