#ifndef __OGCSYS_H__
#define __OGCSYS_H__

#include <gccore.h>
#include <sys/types.h>

#define TB_MSPERSEC						1000
#define TB_USPERSEC						1000000
#define TB_NSPERSEC						1000000000
#define TB_NSPERMS						1000000
#define TB_NSPERUS						1000
#define TB_USPERTICK					10000

#define TB_SECSPERMIN					60
#define TB_MINSPERHR					60
#define TB_MONSPERYR					12
#define TB_DAYSPERYR					365
#define TB_HRSPERDAY					24
#define TB_SECSPERDAY					(TB_SECSPERMIN*TB_MINSPERHR*TB_HRSPERDAY)
#define TB_SECSPERNYR					(365*TB_SECSPERDAY)
									
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
