// SPDX-License-Identifier: Zlib
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#pragma once
#include "types.h"
#include "mm.h"
#include "mm_env.h"

#define g_envLowMem    ((EnvLowMem*)         MM_ENV_LOW_MEM)
#define g_envBootstub  ((EnvBootstubHeader*) MM_ENV_HB_BOOTSTUB)
#define g_envLowMem2   ((EnvLowMem2*)        MM_ENV_LOW_MEM2)
#if defined(__wii__)
#define g_envRvlLowMem ((EnvRvlLowMem*)      MM_ENV_RVL_LOW_MEM)
#endif

#define ENV_DISC_CUBE_MAGIC 0xc2339f3d
#define ENV_DISC_WII_MAGIC  0x5d1c9ea3

#define ENV_DISC_ID_OFFSET        0x00000
#define ENV_DISC_NAME_OFFSET      0x00020
#define ENV_DISC_BB2_OFFSET       0x00420
#define ENV_DISC_BI2_OFFSET       0x00440
#define ENV_DISC_APPL_OFFSET      0x02440
#define ENV_DISC_WII_TABLE_OFFSET 0x40000
#define ENV_DISC_WII_INFO_OFFSET  0x4e000

#define ENV_BOOT_INFO_MAGIC 0x0d15ea5e
#define ENV_BOOT_INFO2_SZ   0x2000

#define ENV_COUNTRY_JP    0
#define ENV_COUNTRY_US    1
#define ENV_COUNTRY_EU    2
#define ENV_COUNTRY_OTHER 3

#define ENV_DOL_NUM_TEXT_SECTIONS 7
#define ENV_DOL_NUM_DATA_SECTIONS 11

#define ENV_HB_BOOTSTUB_MAGIC1 0x53545542 // 'STUB'
#define ENV_HB_BOOTSTUB_MAGIC2 0x48415858 // 'HAXX'

MK_EXTERN_C_START

typedef struct EnvDiscId {
	char gamecode[4];
	char makercode[2];
	u8   disc_num;
	u8   version;
	u8   stream_enable;
	u8   stream_buf_size;
	u8   reserved_0x0A[12];
	u32  wii_magic;
	u32  cube_magic;
} EnvDiscId;

typedef struct EnvDiscBB2 {
	u32 dol_offset;
	u32 fst_offset;
	u32 fst_size;
	u32 fst_max_size;
	u32 fst_address;
	u32 user_offset;
	u32 user_size;
	u32 reserved;
} EnvDiscBB2;

typedef struct EnvFstEntry {
	u32 flags       : 8;
	u32 name_offset : 24;

	union {
		u32 file_offset;
		u32 dir_parent_offset;
	};

	union {
		u32 file_length;
		u32 root_num_entries;
		u32 dir_next_offset;
	};
} EnvFstEntry;

typedef struct EnvBootInfo {
	EnvDiscId disc_id;
	u32   magic;
	u32   version;
	u32   phys_mem_size;
	u32   console_type;
	void* arena_lo;
	void* arena_hi;
	EnvFstEntry* fst;
	u32   fst_max_size;
} EnvBootInfo;

typedef struct EnvBootInfo2 {
	u32 dbg_mon_size;
	u32 sim_mem_size;
	u32 arg_offset;
	u32 debug_flag;
	u32 track_location;
	u32 track_size;
	u32 country_code;
	u32 unk_0x1C;
} EnvBootInfo2;

typedef struct EnvLowMem {
	EnvBootInfo boot_info;

	u8 reserved_0x40[0x80];
	u8 reserved_0xC0[0x0c];

	u32 tv_mode;
	u32 aram_size;

	u8 reserved_0xD4[0x14];

	u32 dbg_mon_size;
	void* dbg_mon;
	u32 sim_mem_size;
	EnvBootInfo2* bi2;

	u32 bus_clock;
	u32 core_clock;
} EnvLowMem;

typedef struct EnvBootstubHeader {
	u32 first_inst;
	u32 magic[3];
} EnvBootstubHeader;

typedef struct EnvLowMem2 {
	u8  reserved_0x00[0xc0];

	u32 exi_probe_start_time[2];
	u8  reserved_0xC8[12];

	u32 app_load_size;

	u64 system_time;
	u8  unk_0xE0[4];
	u16 pad4_buttons;
	u8  unk_0xE6[6];
	u32 unk_0xEC;

	u8  unk_0xF0[0x10];
} EnvLowMem2;

typedef struct EnvRvlMemInfo {
	u32 phys_size;
	u32 sim_size;
	u32 unk_0x08;
	void* arena_lo;
	void* arena_hi;
	u32 unk_0x14;
} EnvRvlMemInfo;

typedef struct EnvRvlLowMem {
	EnvRvlMemInfo mem1;
	EnvRvlMemInfo mem2;
	void* ios_ipc_lo;
	void* ios_ipc_hi;
	u32 hollywood_ver;
	u32 unk_0x3C;
	u32 ios_version;
	u32 ios_build_date;
	u32 ios_heap_lo;
	u32 ios_heap_hi;
	u32 unk_0x50[2];
	u32 gddr_vendor_code;
	u8  unk_0x5C[0xa4];
} EnvRvlLowMem;

typedef struct EnvDolHeader {
	u32 text_offsets[ENV_DOL_NUM_TEXT_SECTIONS];
	u32 data_offsets[ENV_DOL_NUM_DATA_SECTIONS];
	u32 text_addrs[ENV_DOL_NUM_TEXT_SECTIONS];
	u32 data_addrs[ENV_DOL_NUM_DATA_SECTIONS];
	u32 text_sizes[ENV_DOL_NUM_TEXT_SECTIONS];
	u32 data_sizes[ENV_DOL_NUM_DATA_SECTIONS];
	u32 bss_addr;
	u32 bss_size;
	u32 entrypoint;
	u32 unk_0xE4[7];
} EnvDolHeader;

MK_EXTERN_C_END

