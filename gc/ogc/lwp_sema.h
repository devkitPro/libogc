#ifndef __LWP_SEMA_H__
#define __LWP_SEMA_H__

#include <gctypes.h>
#include <lwp_threadq.h>

#define LWP_SEMA_MODEFIFO				0
#define LWP_SEMA_MODEPRIORITY			1

#define LWP_SEMA_SUCCESSFUL				0
#define LWP_SEMA_UNSATISFIED_NOWAIT		1
#define LWP_SEMA_DELETED				2
#define LWP_SEMA_TIMEOUT				3
#define LWP_SEMA_MAXCNT_EXCEEDED		4

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _lwpsemattr {
	u32 max_cnt;
	u32 mode;
} lwp_semattr;

typedef struct _lwpsema {
	lwp_thrqueue wait_queue;
	lwp_semattr	attrs;
	u32 count;
} lwp_sema;

void __lwp_sema_initialize(lwp_sema *,lwp_semattr *,u32);
u32 __lwp_sema_surrender(lwp_sema *,u32);
u32 __lwp_sema_seize(lwp_sema *,u32,u32,u32);
void __lwp_sema_flush(lwp_sema *,u32);

#include <libogc/lwp_sema.inl>

#ifdef __cplusplus
	}
#endif

#endif
