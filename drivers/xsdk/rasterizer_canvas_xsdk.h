/* rasterizer_canvas_xsdk.h - part of Godot 360 / xsdk 2D canvas renderer */

#ifndef RASTERIZER_CANVAS_XSDK_H
#define RASTERIZER_CANVAS_XSDK_H

#include "core/os/os.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual_server.h"

#include "drivers/xsdk/rasterizer_storage_xsdk.h"

#include "xsdk/gpu.h"
#include "xsdk/shader.h"
#include "xsdk/present.h"

#include "drivers/xsdk/canvas_shaders_xsdk.h"

static const uint32_t XSDK_CANVAS_VERTEX_RING_BYTES = 1 << 18; // 256 KB of vertices (reduced for xsdk)
static const uint32_t XSDK_CANVAS_INDEX_RING_BYTES = 1 << 18;  // 256 KB of indices (reduced for xsdk)
static const uint32_t XSDK_CANVAS_VERTEX_SIZE = 8 * 4;         // 8 floats: pos(2), uv(2), color(4)

class RasterizerCanvasXsdk : public RasterizerCanvas {
public:
	uint32_t width = 1280;
	uint32_t height = 720;

	uint32_t frame_address = 0;
	uint32_t clear_argb = 0xFF2D2D2D; // Godot's default clear color (0.3)
	uint32_t frame_number = 0;

	XsdkShader vertex = {};
	XsdkShader pixel = {};
	bool shaders_loaded = false;

	volatile uint32_t *vertices = nullptr;
	volatile uint32_t *indices = nullptr;
	uint32_t vertex_offset = 0; // bytes
	uint32_t index_offset = 0;  // bytes

	XsdkBlendState alpha_blend = {};

	RasterizerCanvasXsdk() {}
	virtual ~RasterizerCanvasXsdk() {}

	void initialize();
	void finalize();

	virtual RID light_internal_create() override { return RID(); }
	virtual void light_internal_update(RID p_rid, Light *p_light) override {}
	virtual void light_internal_free(RID p_rid) override {}
	virtual void canvas_begin() override {}
	virtual void canvas_end() override {}
	virtual void canvas_render_items_begin(const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) override {}
	virtual void canvas_render_items_end() override {}
	virtual void canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) override;
	virtual void canvas_debug_viewport_shadows(Light *p_lights_with_shadow) override {}
	virtual void canvas_light_shadow_buffer_update(RID p_buffer, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders, CameraMatrix *p_xform_cache) override {}
	virtual void reset_canvas() override {}
	virtual void draw_window_margins(int *p_margins, RID *p_margin_textures) override {}

private:
	void _fill_vertex(uint32_t vb_offset_bytes, const Vector2 &p_pos, const Vector2 &p_uv, const Color &p_color);
	uint32_t _alloc_vertices(uint32_t p_count);
	uint32_t _alloc_indices(uint32_t p_count);

	void _draw_primitive_internal(uint32_t vb_offset, uint32_t vcount, uint32_t ib_offset, uint32_t icount,
		const Transform2D &p_xform, const Color &p_modulate, const struct RasterizerStorageXsdk::TextureData *p_tex, bool p_tile);
	void _draw_primitive_nonindexed(uint32_t vb_offset, uint32_t vcount,
		const Transform2D &p_xform, const Color &p_modulate, const struct RasterizerStorageXsdk::TextureData *p_tex, bool p_tile);
	void _draw_rect(const Item *ci, const Item::CommandRect *command, const Transform2D &xform, const Color &modulate);
	void _draw_multirect(const Item *ci, const Item::CommandMultiRect *command, const Transform2D &xform, const Color &modulate);
	void _draw_polyline(const Item *ci, const Item::CommandPolyLine *command, const Transform2D &xform, const Color &modulate);
	void _draw_polygon(const Item *ci, const Item::CommandPolygon *command, const Transform2D &xform, const Color &modulate);
	void _draw_primitive(const Item *ci, const Item::CommandPrimitive *command, const Transform2D &xform, const Color &modulate);
	void _draw_circle(const Item *ci, const Item::CommandCircle *command, const Transform2D &xform, const Color &modulate);
	void _draw_line(const Item *ci, const Item::CommandLine *command, const Transform2D &xform, const Color &modulate);
};

#endif
