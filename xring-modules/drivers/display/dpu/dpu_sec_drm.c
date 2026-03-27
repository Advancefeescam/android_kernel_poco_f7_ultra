// SPDX-License-Identifier: GPL-2.0-only
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
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_writeback.h>
#include "dpu_hw_rch.h"
#include "dpu_plane.h"
#include "dpu_crtc.h"
#include "dpu_wb.h"
#include "dpu_kms.h"

int dpu_plane_create_sec_drm_properties(struct drm_plane *plane)
{
	struct drm_property *property;
	struct dpu_plane *dpu_plane;

	if (!plane) {
		DPU_ERROR("invalid parameters, %pK\n", plane);
		return -EINVAL;
	}
	dpu_plane = to_dpu_plane(plane);

	property = drm_property_create_range(plane->dev, 0,
					"rch_protected_buffer", 0, 1);
	if (!property) {
		DPU_ERROR("create failed\n");
		return -ENOMEM;
	}

	drm_object_attach_property(&plane->base, property, 0);
	dpu_plane->protected_buffer_status_prop = property;

	return 0;
}

int dpu_wb_create_sec_drm_properties(struct drm_writeback_connector *wb_conn)
{
	struct dpu_wb_connector *dpu_wb_conn;
	struct drm_property *property;

	if (!wb_conn) {
		DPU_ERROR("invalid parameters, %pK\n", wb_conn);
		return -EINVAL;
	}

	dpu_wb_conn = to_dpu_wb_connector(wb_conn);
	property = drm_property_create_range(wb_conn->base.dev, 0,
			"wb_protected_buffer_enable_prop", 0, 1);
	if (!property) {
		DPU_ERROR("drm_property_create fail: wb_protected_buffer_enable_prop\n");
		return -ENOMEM;
	}
	drm_object_attach_property(&wb_conn->base.base, property, 0);
	dpu_wb_conn->protected_buffer_enable_prop = property;

	return 0;
}

static bool dpu_plane_check_protected_enable(struct drm_plane *plane,
		struct drm_atomic_state *state)
{
	struct drm_plane_state *plane_state;
	struct dpu_plane_state *dpu_plane_state;
	bool is_protected_read_enable = false;

	if (!plane || !state) {
		DPU_ERROR("invalid parameters, %pK, %pK\n",
				plane, state);
		return is_protected_read_enable;
	}

	plane_state = drm_atomic_get_old_plane_state(state, plane);
	dpu_plane_state = to_dpu_plane_state(plane_state);
	is_protected_read_enable = dpu_plane_state->is_protected_buffer;

	return is_protected_read_enable;
}

u8 dpu_sec_drm_rdma_cfg(struct drm_plane *plane, struct drm_atomic_state *state)
{
	enum protected_status drm_enable_status = HOLD;
	struct drm_plane_state *plane_state;
	struct dpu_plane_state *dpu_plane_state;

	plane_state = plane->state;
	dpu_plane_state = to_dpu_plane_state(plane_state);
	if (dpu_plane_check_protected_enable(plane, state))
		drm_enable_status = SWITCH_OFF;
	else
		drm_enable_status = HOLD;

	if (dpu_plane_state->is_protected_buffer)
		drm_enable_status = SWITCH_ON;

	return drm_enable_status;
}

void dpu_sec_drm_wb_cfg(struct wb_frame_cfg *wb_cfg, struct drm_atomic_state *state,
		struct drm_connector *connector)
{
	struct drm_writeback_connector *drm_wb_conn;
	struct dpu_wb_connector_state *wb_state;
	struct drm_connector_state *conn_state;
	struct dpu_wb_connector *dpu_wb_conn;

	if (!connector || !state) {
		DPU_ERROR("invalid parameters, %pK, %pK\n",
			connector, state);
		return;
	}
	// generally speaking, it need to do nothing
	wb_cfg->drm_enable_status = HOLD;

	drm_wb_conn = drm_connector_to_writeback(connector);
	dpu_wb_conn = to_dpu_wb_connector(drm_wb_conn);
	conn_state = connector->state;
	wb_state = to_dpu_wb_connector_state(conn_state);
	// if mid status is enable, it need to be changed to disable and reset mid register at first
	if (dpu_wb_conn->mid_enable) {
		wb_cfg->drm_enable_status = SWITCH_OFF;
		dpu_wb_conn->mid_enable = false;
	}

	// if current frame is protected, it need to set mid register and record mid enable
	if (wb_state->protected_buffer_enable) {
		wb_cfg->drm_enable_status = SWITCH_ON;
		dpu_wb_conn->mid_enable = true;
	}
}

static bool dpu_sec_is_cmd_mode(struct drm_crtc *crtc)
{
	struct drm_connector *connector;
	bool is_cmd_mode = false;

	if (!crtc) {
		DPU_ERROR("drm_crtc is null");
		return false;
	}

	connector = dsi_primary_connector_get(crtc->dev);
	if (!connector)
		return false;

	if (connector->connector_type == DRM_MODE_CONNECTOR_DSI &&
			dsi_connector_is_cmd_mode(connector))
		is_cmd_mode = true;

	return is_cmd_mode;
}

enum protected_status dpu_sec_is_primary_protect_drm_frame(struct drm_crtc *crtc,
		struct drm_crtc_state *new_state, struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state, *old_plane_state;
	struct dpu_plane_state *dpu_old_plane_state;
	struct dpu_plane_state *dpu_new_plane_state;
	bool is_old_plane_has_protect = false;
	bool is_new_plane_has_protect = false;
	struct drm_plane *plane;
	int i;

	if (!crtc || !new_state || !state) {
		DPU_ERROR("has null point");
		return HOLD;
	}

	if (!is_primary_display(new_state))
		return HOLD;

	for_each_oldnew_plane_in_state(state, plane,
			old_plane_state, new_plane_state, i) {
		dpu_old_plane_state = to_dpu_plane_state(old_plane_state);
		dpu_new_plane_state = to_dpu_plane_state(new_plane_state);
		if (is_old_plane_has_protect == false &&
				dpu_old_plane_state->is_protected_buffer)
			is_old_plane_has_protect = true;
		if (is_new_plane_has_protect == false &&
				dpu_new_plane_state->is_protected_buffer)
			is_new_plane_has_protect = true;
	}

	if ((!is_old_plane_has_protect) && is_new_plane_has_protect)
		return SWITCH_ON;

	if (is_old_plane_has_protect && (!is_new_plane_has_protect))
		return SWITCH_OFF;

	return HOLD;
}

void dpu_sec_protect_drm_prepare(struct drm_crtc *crtc,
		struct drm_crtc_state *new_state, struct drm_atomic_state *state)
{
	enum protected_status drm_state = HOLD;
	static bool drm_idle_vote_enable;

	if (!new_state) {
		DPU_ERROR("new_state is null");
		return;
	}

	/* cmd mode support protect idle*/
	if (dpu_sec_is_cmd_mode(crtc))
		return;

	if (drm_idle_vote_enable && need_disable_crtc(new_state)) {
		IDLE_DEBUG("restore idle when power off");
		dpu_idle_enable_ctrl(true);
		return;
	}

	drm_state = dpu_sec_is_primary_protect_drm_frame(crtc, new_state, state);
	if (drm_state == SWITCH_ON) {
		dpu_idle_enable_ctrl(false);
		drm_idle_vote_enable = true;
		IDLE_DEBUG("frame_no:%llu stop idle",
			to_dpu_crtc_state(new_state)->frame_no);
	}
	if (drm_state == SWITCH_OFF) {
		dpu_idle_enable_ctrl(true);
		drm_idle_vote_enable = false;
		IDLE_DEBUG("frame_no:%llu restore idle",
			to_dpu_crtc_state(new_state)->frame_no);
	}
}
