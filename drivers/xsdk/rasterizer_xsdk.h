/* rasterizer_xsdk.h - part of Godot 360 / xsdk rasterizer (2D UI on Xenos) */

#ifndef RASTERIZER_XSDK_H
#define RASTERIZER_XSDK_H

#include "drivers/xsdk/rasterizer_canvas_xsdk.h"
#include "drivers/xsdk/rasterizer_scene_xsdk.h"
#include "drivers/xsdk/rasterizer_storage_xsdk.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual_server.h"

class RasterizerXsdk : public Rasterizer {
	RasterizerStorageXsdk storage;
	RasterizerCanvasXsdk canvas;
	RasterizerSceneXsdk scene;
	uint64_t frame_count = 0;

public:
	virtual RasterizerStorage *get_storage() { return &storage; }
	virtual RasterizerCanvas *get_canvas() { return &canvas; }
	virtual RasterizerScene *get_scene() { return &scene; }

	virtual void initialize();
	virtual void begin_frame(double p_frame_step);
	virtual void set_current_render_target(RID p_render_target);
	virtual void restore_render_target(bool p_3d_was_drawn);
	virtual void clear_render_target(const Color &p_color);
	virtual void blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen = 0);
	virtual void output_lens_distorted_to_screen(RID p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample);
	virtual void end_frame(bool p_swap_buffers);
	virtual void finalize();

	virtual void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) {}
	virtual void set_shader_time_scale(float p_scale) {}

	virtual bool is_low_end() const { return true; }

	static Error is_viable() {
		return OK;
	}

	static Rasterizer *_create_current() {
		return memnew(RasterizerXsdk);
	}

	static void make_current() {
		Rasterizer::_create_func = _create_current;
	}

	RasterizerXsdk() {}
	virtual ~RasterizerXsdk() {}
};

#endif
