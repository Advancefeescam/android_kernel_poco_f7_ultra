/*
 * Copyright (c) 2024-2025 X-Ring Technologies Inc., all rights reserved.
 */
#ifndef __UEFI_DBG_DEF_H__
#define __UEFI_DBG_DEF_H__

#define RESULT_SUCCE			0
#define RESULT_FAIL			1

#define XCTRLCPU_RPC_MAX_DATA_CNT 	4

#define MSG_SYNC			1
#define MSG_ASYNC			0

#define KERNEL_RPC_DATA_SIZE		(60 / (sizeof(unsigned int)))


typedef enum {
        START_UP,
        SLT,
        ATE,
        PMU_OPS,
        SCENE_MAX
} scene_t;

typedef enum {
        MOD_CPU,
        MOD_GPU,
        MOD_NPU,
        MOD_PMU,
        MOD_XCTRL_CPU,
        MOD_BCL,
        MOD_MAX
} module_t;

typedef struct {
	unsigned int result      : 1;
	unsigned int sync_type   : 1;
	unsigned int size        : 6;
	scene_t scene_type       : 8;
	module_t module_type     : 8;
	unsigned int id          : 8;
}uefi_rpc_frame_t;

typedef struct {
	unsigned int index;
	unsigned int size;
	unsigned int sync_type;
	int result;
} kernel_rpc_frame_t;

typedef struct {
	unsigned int sync_type   : 8;
	unsigned int size        : 8;
	scene_t scene_type       : 8;
	unsigned int id          : 8;
} uefi_rpc_t;

typedef int(*func_entry_t)(uefi_rpc_t* msg, unsigned int *data);
typedef int(*kernel_rpc_entry_t)(unsigned int *data, unsigned int data_size);

#endif