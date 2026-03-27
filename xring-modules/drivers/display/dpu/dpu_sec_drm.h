/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023-2024 XRing Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _DPU_SEC_DRM_H_
#define _DPU_SEC_DRM_H_

#include "dpu_hw_common.h"
#include "dpu_plane.h"
#include "dpu_wb.h"

int dpu_plane_create_sec_drm_properties(struct drm_plane *plane);
int dpu_wb_create_sec_drm_properties(struct drm_writeback_connector *wb_conn);
u8 dpu_sec_drm_rdma_cfg(struct drm_plane *plane, struct drm_atomic_state *state);
void dpu_sec_drm_wb_cfg(struct wb_frame_cfg *wb_cfg, struct drm_atomic_state *state,
		struct drm_connector *connector);

/**
 * dpu_sec_is_primary_protect_drm_frame - Determine if the primary screen has protection plane
 * @crtc: point of primary crtc
 * @new_state: point of crtc state
 * @state: point of atomic state
 * Return: whether the current main screen crtc has protection plane switching
 */
enum protected_status dpu_sec_is_primary_protect_drm_frame(struct drm_crtc *crtc,
		struct drm_crtc_state *new_state, struct drm_atomic_state *state);

/**
 * @crtc: point of primary crtc
 * @new_state: point of crtc state
 * @state: point of atomic state
 */
void dpu_sec_protect_drm_prepare(struct drm_crtc *crtc,
		struct drm_crtc_state *new_state, struct drm_atomic_state *state);

#endif /* _DPU_SEC_DRM_H_ */
