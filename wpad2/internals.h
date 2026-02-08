#ifndef WPAD2_INTERNALS_H
#define WPAD2_INTERNALS_H

#define __BTE_H__ /* Prevent inclusion */
#include "ogc/lwp_queue.h"
#include "bte/bd_addr.h"

#include "wiiuse/wpad.h"
#include "wiiuse_internal.h"

#include "bt-embedded/l2cap.h"

#ifndef __wii__
#  include <endian.h>
#else
#  include <sys/endian.h>
#endif
#include <stdio.h>

#define read_be16(ptr) be16toh(*(uint16_t *)(ptr))
#define read_be32(ptr) be32toh(*(uint32_t *)(ptr))

#define write_be16(n, ptr) *(uint16_t *)(ptr) = htobe16(n)
#define write_be32(n, ptr) *(uint32_t *)(ptr) = htobe32(n)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define WIIMOTE_PI                        3.14159265f
/* Convert between radians and degrees */
#define RAD_TO_DEGREE(r)    ((r * 180.0f) / WIIMOTE_PI)
#define DEGREE_TO_RAD(d)    (d * (WIIMOTE_PI / 180.0f))

#define absf(x)                     ((x >= 0) ? (x) : (x * -1.0f))
#define diff_f(x, y)                ((x >= y) ? (absf(x - y)) : (absf(y - x)))

//#define WITH_WPAD_DEBUG

#ifdef WITH_WPAD_DEBUG
	#define WPAD2_DEBUG(fmt, ...)  SYS_Report("[DEBUG] %s:%i: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
	#define WPAD2_WARNING(fmt, ...) SYS_Report("[WARNING] " fmt "\n",  ##__VA_ARGS__)
	#define WPAD2_ERROR(fmt, ...) SYS_Report("[ERROR] " fmt "\n",  ##__VA_ARGS__)
#else
	#define WPAD2_DEBUG(fmt, ...)
	#define WPAD2_WARNING(fmt, ...)
	#define WPAD2_ERROR(fmt, ...)
#endif

#define BD_ADDR_FROM_CONF(bdaddr, b) do { \
    (bdaddr)->bytes[0] = b[5]; \
    (bdaddr)->bytes[1] = b[4]; \
    (bdaddr)->bytes[2] = b[3]; \
    (bdaddr)->bytes[3] = b[2]; \
    (bdaddr)->bytes[4] = b[1]; \
    (bdaddr)->bytes[5] = b[0]; } while(0)

#define BD_ADDR_TO_CONF(b, bdaddr) do { \
    b[0] = (bdaddr)->bytes[5]; \
    b[1] = (bdaddr)->bytes[4]; \
    b[2] = (bdaddr)->bytes[3]; \
    b[3] = (bdaddr)->bytes[2]; \
    b[4] = (bdaddr)->bytes[1]; \
    b[5] = (bdaddr)->bytes[0]; } while(0)

#define BD_ADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define BD_ADDR_DATA(b) \
    (b)->bytes[5], (b)->bytes[4], (b)->bytes[3], \
    (b)->bytes[2], (b)->bytes[1], (b)->bytes[0]

#define WPAD2_MAX_DEVICES (4 + 1)

#endif /* WPAD2_INTERNALS_H */
