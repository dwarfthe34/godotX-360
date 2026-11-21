/* rasterizer_xsdk.cpp - part of Godot 360 / xsdk rasterizer */

#include "rasterizer_xsdk.h"

#include "xsdk/present.h"
#include "xsdk/log.h"

void RasterizerXsdk::initialize() {
	canvas.initialize();
	xsdk_log_write("xsdk: rasterizer canvas done");
	storage.initialize();
	xsdk_log_write("xsdk: rasterizer storage done");
}

void RasterizerXsdk::begin_frame(double p_frame_step) {
	uint32_t address = 0;
	const NTSTATUS status = xsdk_present_begin(&address, 5000);
	if (!NT_SUCCESS(status)) {
		return;
	}
	canvas.frame_address = address;
	xsdk_gpu_set_target(canvas.width, canvas.height);
	xsdk_gpu_set_blend(&canvas.alpha_blend);
	xsdk_gpu_set_scissor(0, 0, canvas.width, canvas.height);
}

void RasterizerXsdk::set_current_render_target(RID p_render_target) {
	// The root viewport draws directly to eDRAM; the resolve at end_frame shows it.
}

void RasterizerXsdk::restore_render_target(bool p_3d_was_drawn) {
}

void RasterizerXsdk::clear_render_target(const Color &p_color) {
	// The resolve at end_frame clears eDRAM; no-op here.
}

void RasterizerXsdk::blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen) {
	// The resolve at end_frame already shows the frame; nothing else to do.
}

void RasterizerXsdk::output_lens_distorted_to_screen(RID p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample) {
}

void RasterizerXsdk::end_frame(bool p_swap_buffers) {
	xsdk_gpu_resolve(canvas.frame_address, canvas.width, canvas.height, canvas.clear_argb);
	xsdk_present_end();
}

void RasterizerXsdk::finalize() {
	canvas.finalize();
}
