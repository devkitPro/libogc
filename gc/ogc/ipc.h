#ifndef __IPC_H__
#define __IPC_H__

#include <gctypes.h>

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

u32 IPC_ReadReg(u32 reg);
void IPC_WriteReg(u32 reg,u32 val);
void ACR_WriteReg(u32 reg,u32 val);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
