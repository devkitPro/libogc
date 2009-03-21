#ifndef __ISO9660_H__
#define __ISO9660_H__

#include <gctypes.h>

#define ISO_MAXPATHLEN		128

#ifdef __cplusplus
extern "C" {
#endif

BOOL ISO9660_Mount();
BOOL ISO9660_Unmount();
u64 ISO9660_LastAccess();

#ifdef __cplusplus
}
#endif

#endif
