/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM gzvm
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_GZVM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_GZVM_H
#include <trace/hooks/vendor_hooks.h>
#if IS_ENABLED(CONFIG_MTK_GZVM_DEBUG)
struct gzvm_vcpu;
struct gzvm;

DECLARE_HOOK(android_vh_gzvm_vcpu_exit_reason,
	     TP_PROTO(struct gzvm_vcpu *vcpu, bool *userspace),
	     TP_ARGS(vcpu, userspace));

DECLARE_HOOK(android_vh_gzvm_handle_demand_page_pre,
	     TP_PROTO(struct gzvm *vm, int memslot_id, u64 pfn, u64 gfn, u32 nr_entries),
	     TP_ARGS(vm, memslot_id, pfn, gfn, nr_entries));

DECLARE_HOOK(android_vh_gzvm_handle_demand_page_post,
	     TP_PROTO(struct gzvm *vm, int memslot_id, u64 pfn, u64 gfn, u32 nr_entries),
	     TP_ARGS(vm, memslot_id, pfn, gfn, nr_entries));

DECLARE_HOOK(android_vh_gzvm_destroy_vm_post_process,
	     TP_PROTO(struct gzvm *vm),
	     TP_ARGS(vm));
#endif

#endif /* _TRACE_HOOK_GZVM_H */
/* This part must be outside protection */
#include <trace/define_trace.h>

