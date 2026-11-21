/* rasterizer_canvas_xsdk.cpp - part of Godot 360 / xsdk canvas renderer */

#include "rasterizer_canvas_xsdk.h"

#include "core/image.h"
#include "core/math/transform_2d.h"
#include "drivers/xsdk/rasterizer_storage_xsdk.h"
#include "servers/visual/visual_server_globals.h"
#include "xsdk/present.h"
#include "xsdk/log.h"

static const uint32_t XSDK_CANVAS_TEXTURE_SLOT = CANVAS_FRAG_SAMPLER_0_0;

namespace {

static void compute_mvp(const Transform2D &p_tf, uint32_t p_width, uint32_t p_height, float *r_mvp) {
	const float sx = 2.0f / (float)p_width;
	const float sy = 2.0f / (float)p_height;
	const float a = p_tf.elements[0][0], b = p_tf.elements[0][1];
	const float c = p_tf.elements[1][0], d = p_tf.elements[1][1];
	const float ox = p_tf.elements[2][0], oy = p_tf.elements[2][1];
	// column-major mat4 for pc.mvp: canvas pixels (y down) -> clip space
	const float m[16] = {
		sx * a, -sy * b, 0.0f, 0.0f,
		sx * c, -sy * d, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		sx * ox - 1.0f, 1.0f - sy * oy, 0.0f, 1.0f,
	};
	for (int i = 0; i < 16; i++) {
		r_mvp[i] = m[i];
	}
}

} // namespace

void RasterizerCanvasXsdk::initialize() {
	NTSTATUS status = xsdk_gpu_open();
	if (!NT_SUCCESS(status)) {
		xsdk_log_write("xsdk canvas: xsdk_gpu_open failed: 0x%08X", (unsigned)status);
		return;
	}
	status = xsdk_present_open(XSDK_DISPLAY_720P, 2, 1, &width, &height);
	if (!NT_SUCCESS(status)) {
		xsdk_log_write("xsdk canvas: xsdk_present_open failed: 0x%08X", (unsigned)status);
		return;
	}
	xsdk_log_write("xsdk canvas: display is %ux%u", (unsigned)width, (unsigned)height);

	status = xsdk_shader_load(&vertex, canvas_vert.container, canvas_vert.container_bytes);
	if (NT_SUCCESS(status))
		status = xsdk_shader_load(&pixel, canvas_frag.container, canvas_frag.container_bytes);
	if (!NT_SUCCESS(status)) {
		xsdk_log_write("xsdk canvas: xsdk_shader_load failed: 0x%08X", (unsigned)status);
		return;
	}
	static const XsdkVertexElement layout[] = {
		{ .usage = XSDK_USAGE_POSITION, .usage_index = 0, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 0 },
		{ .usage = XSDK_USAGE_TEXCOORD, .usage_index = 0, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 8 },
		{ .usage = XSDK_USAGE_COLOR, .usage_index = 0, .format = XSDK_VERTEX_FLOAT4, .offset_bytes = 16 },
	};
	status = xsdk_shader_set_vertex_layout(&vertex, layout, 3, (const uint32_t[]){ XSDK_CANVAS_VERTEX_SIZE });
	if (!NT_SUCCESS(status)) {
		xsdk_log_write("xsdk canvas: xsdk_shader_set_vertex_layout failed: 0x%08X", (unsigned)status);
		return;
	}
	shaders_loaded = true;

	vertices = xsdk_gpu_alloc(XSDK_CANVAS_VERTEX_RING_BYTES);
	indices = xsdk_gpu_alloc(XSDK_CANVAS_INDEX_RING_BYTES);
	if (!vertices || !indices) {
		xsdk_log_write("xsdk canvas: xsdk_gpu_alloc failed (v=%p i=%p)", (void *)vertices, (void *)indices);
		return;
	}

	{
		const XsdkBlend blend = {
			.color_source = XSDK_BLEND_SRC_ALPHA,
			.color_dest = XSDK_BLEND_ONE_MINUS_SRC_ALPHA,
			.color_op = XSDK_BLEND_OP_ADD,
			.alpha_source = XSDK_BLEND_ONE,
			.alpha_dest = XSDK_BLEND_ZERO,
			.alpha_op = XSDK_BLEND_OP_ADD,
			.write_mask = XSDK_WRITE_ALL,
			.alpha_to_coverage = false,
		};
		xsdk_gpu_blend_state(&alpha_blend, &blend);
	}

	uint32_t address = 0;
	status = xsdk_present_begin(&address, 100);
	if (NT_SUCCESS(status)) {
		xsdk_gpu_set_target(width, height);
		xsdk_gpu_resolve(address, width, height, clear_argb);
		xsdk_present_end();
	}
}

void RasterizerCanvasXsdk::finalize() {
	xsdk_present_close();
	xsdk_gpu_close();
}

uint32_t RasterizerCanvasXsdk::_alloc_vertices(uint32_t p_count) {
	const uint32_t need = p_count * XSDK_CANVAS_VERTEX_SIZE;
	if (need > XSDK_CANVAS_VERTEX_RING_BYTES)
		return vertex_offset;
	if (vertex_offset + need > XSDK_CANVAS_VERTEX_RING_BYTES) {
		// Wrap: wait for the GPU to finish reading before reusing the ring.
		uint32_t fence = xsdk_gpu_fence();
		NTSTATUS status = xsdk_gpu_wait(fence, 5000);
		if (!NT_SUCCESS(status))
			xsdk_log_write("xsdk canvas: xsdk_gpu_wait (ring wrap) failed: 0x%08X", (unsigned)status);
		vertex_offset = 0;
	}
	const uint32_t ret = vertex_offset;
	vertex_offset += need;
	return ret;
}

uint32_t RasterizerCanvasXsdk::_alloc_indices(uint32_t p_count) {
	const uint32_t need = p_count * 2; // 16-bit indices
	if (need > XSDK_CANVAS_INDEX_RING_BYTES)
		return index_offset;
	if (index_offset + need > XSDK_CANVAS_INDEX_RING_BYTES) {
		uint32_t fence = xsdk_gpu_fence();
		NTSTATUS status = xsdk_gpu_wait(fence, 5000);
		if (!NT_SUCCESS(status))
			xsdk_log_write("xsdk canvas: xsdk_gpu_wait (index wrap) failed: 0x%08X", (unsigned)status);
		index_offset = 0;
	}
	const uint32_t ret = index_offset;
	index_offset += need;
	return ret;
}

void RasterizerCanvasXsdk::_fill_vertex(uint32_t vb_offset_bytes, const Vector2 &p_pos, const Vector2 &p_uv, const Color &p_color) {
	const uint32_t word_offset = vb_offset_bytes / 4;
	const float *const pos = &p_pos.x;
	const float *const uv = &p_uv.x;
	const float *const col = &p_color.r;
	vertices[word_offset + 0] = *(const uint32_t *)&pos[0];
	vertices[word_offset + 1] = *(const uint32_t *)&pos[1];
	vertices[word_offset + 2] = *(const uint32_t *)&uv[0];
	vertices[word_offset + 3] = *(const uint32_t *)&uv[1];
	vertices[word_offset + 4] = *(const uint32_t *)&col[0];
	vertices[word_offset + 5] = *(const uint32_t *)&col[1];
	vertices[word_offset + 6] = *(const uint32_t *)&col[2];
	vertices[word_offset + 7] = *(const uint32_t *)&col[3];
}

// xsdk reads 16-bit indices two to a dword, the first in the high half (gpu_draw.c).
static void _set_index(volatile uint32_t *p_indices, uint32_t ib_offset_bytes, uint32_t p_index, uint16_t p_value) {
	volatile uint32_t *word = (volatile uint32_t *)((uint8_t *)p_indices + ib_offset_bytes) + p_index / 2;
	if ((p_index & 1) == 0) {
		*word = (*word & 0x0000FFFFu) | ((uint32_t)p_value << 16);
	} else {
		*word = (*word & 0xFFFF0000u) | (uint32_t)p_value;
	}
}

static RasterizerStorageXsdk *_xsdk_storage() {
	// VSG::storage always holds the storage created by RasterizerXsdk in this build.
	return static_cast<RasterizerStorageXsdk *>(VSG::storage);
}

static RasterizerStorageXsdk::TextureData *_bind_texture(const RID &p_texture) {
	RasterizerStorageXsdk *storage = _xsdk_storage();
	if (storage) {
		if (p_texture.is_valid()) {
			RasterizerStorageXsdk::TextureData *td = storage->texture_get(p_texture);
			if (td && td->allocated)
				return td;
		}
		return storage->texture_get(storage->white_texture);
	}
	return nullptr;
}

void RasterizerCanvasXsdk::_draw_primitive_internal(uint32_t vb_offset, uint32_t vcount, uint32_t ib_offset, uint32_t icount,
	const Transform2D &p_xform, const Color &p_modulate, const RasterizerStorageXsdk::TextureData *p_tex, bool p_tile) {
	float mvp[16];
	compute_mvp(p_xform, width, height, mvp);

	xsdk_gpu_set_shaders(&vertex, &pixel);
	xsdk_built_shader_set_literals(&canvas_vert);
	xsdk_built_shader_set_literals(&canvas_frag);
	xsdk_gpu_set_vertex_constants(CANVAS_VERT_PUSH, mvp, CANVAS_VERT_PUSH_REGISTERS);
	const float color[4] = { p_modulate.r, p_modulate.g, p_modulate.b, p_modulate.a };
	xsdk_gpu_set_pixel_constants(CANVAS_FRAG_PUSH, color, CANVAS_FRAG_PUSH_REGISTERS);

	const RasterizerStorageXsdk::TextureData *tex = p_tex ? p_tex : _bind_texture(RID());
	if (tex)
		xsdk_gpu_set_texture(XSDK_CANVAS_TEXTURE_SLOT, &tex->tex, p_tile ? &_xsdk_storage()->wrap_sampler_state : &_xsdk_storage()->clamp_sampler_state);

	const uint32_t vertices_addr = (uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb_offset;
	const uint32_t indices_addr = (uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib_offset;
	xsdk_gpu_set_vertices(0, vertices_addr, vcount * XSDK_CANVAS_VERTEX_SIZE);
	xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_TRIANGLE_LIST, indices_addr, icount, false);
}

void RasterizerCanvasXsdk::_draw_primitive_nonindexed(uint32_t vb_offset, uint32_t vcount,
	const Transform2D &p_xform, const Color &p_modulate, const RasterizerStorageXsdk::TextureData *p_tex, bool p_tile) {
	float mvp[16];
	compute_mvp(p_xform, width, height, mvp);
	xsdk_gpu_set_shaders(&vertex, &pixel);
	xsdk_built_shader_set_literals(&canvas_vert);
	xsdk_built_shader_set_literals(&canvas_frag);
	xsdk_gpu_set_vertex_constants(CANVAS_VERT_PUSH, mvp, CANVAS_VERT_PUSH_REGISTERS);
	const float color[4] = { p_modulate.r, p_modulate.g, p_modulate.b, p_modulate.a };
	xsdk_gpu_set_pixel_constants(CANVAS_FRAG_PUSH, color, CANVAS_FRAG_PUSH_REGISTERS);
	const RasterizerStorageXsdk::TextureData *tex = p_tex ? p_tex : _bind_texture(RID());
	if (tex)
		xsdk_gpu_set_texture(XSDK_CANVAS_TEXTURE_SLOT, &tex->tex, p_tile ? &_xsdk_storage()->wrap_sampler_state : &_xsdk_storage()->clamp_sampler_state);
	xsdk_gpu_set_vertices(0, (uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb_offset, vcount * XSDK_CANVAS_VERTEX_SIZE);
	xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_LIST, vcount);
}

void RasterizerCanvasXsdk::_draw_rect(const Item *ci, const Item::CommandRect *command, const Transform2D &xform, const Color &modulate) {
	const Vector2 size = command->rect.size;
	const Vector2 p0 = command->rect.position;

	Vector2 pos[4] = {
		p0,
		p0 + Vector2(size.x, 0.0f),
		p0 + size,
		p0 + Vector2(0.0f, size.y),
	};

	const uint32_t flags = command->flags;
	Rect2 src(0.0f, 0.0f, 1.0f, 1.0f);
	if (flags & CANVAS_RECT_REGION) {
		src = command->source;
	}

	Vector2 uv[4] = {
		Vector2(src.position.x, src.position.y),
		Vector2(src.position.x + src.size.x, src.position.y),
		Vector2(src.position.x + src.size.x, src.position.y + src.size.y),
		Vector2(src.position.x, src.position.y + src.size.y),
	};
	const bool flip_h = (flags & CANVAS_RECT_FLIP_H) != 0;
	const bool flip_v = (flags & CANVAS_RECT_FLIP_V) != 0;
	if (flip_h) {
		SWAP(uv[0], uv[1]);
		SWAP(uv[2], uv[3]);
	}
	if (flip_v) {
		SWAP(uv[0], uv[3]);
		SWAP(uv[1], uv[2]);
	}

	const RasterizerStorageXsdk::TextureData *tex = _bind_texture(command->texture);
	if (tex && (flags & CANVAS_RECT_TILE)) {
		// Repeat the source region over the rect; UVs scale by rect/src ratio.
		const float fx = (float)(tex->width);
		const float fy = (float)(tex->height);
		if (flags & CANVAS_RECT_REGION) {
			const Vector2 smin = src.position, ssize = src.size;
			for (int i = 0; i < 4; i++) {
				uv[i] = (uv[i] - smin) / ssize;
			}
			for (int i = 0; i < 4; i++) {
				uv[i] = uv[i] * (size / ssize);
			}
		} else {
			uv[0] = Vector2(0.0f, 0.0f);
			uv[1] = Vector2(size.x / fx, 0.0f);
			uv[2] = Vector2(size.x / fx, size.y / fy);
			uv[3] = Vector2(0.0f, size.y / fy);
		}
	} else if (tex && (flags & CANVAS_RECT_REGION)) {
		// Source rect in texture pixels -> 0..1
		const float fx = 1.0f / (float)tex->width;
		const float fy = 1.0f / (float)tex->height;
		for (int i = 0; i < 4; i++) {
			uv[i].x *= fx;
			uv[i].y *= fy;
		}
	}

	const uint32_t vb = _alloc_vertices(4);
	const uint32_t ib = _alloc_indices(6);
	const Color color = command->modulate;

	for (int i = 0; i < 4; i++) {
		_fill_vertex(vb + i * XSDK_CANVAS_VERTEX_SIZE, pos[i], uv[i], color);
	}
	uint16_t idx[6];
	if (size.y < 0) {
		const uint16_t t[6] = { 3, 1, 0, 3, 2, 1 };
		for (int i = 0; i < 6; i++) idx[i] = t[i];
	} else {
		const uint16_t t[6] = { 0, 1, 2, 0, 2, 3 };
		for (int i = 0; i < 6; i++) idx[i] = t[i];
	}
	if (size.x < 0) {
		const uint16_t t[6] = { 1, 0, 2, 2, 0, 3 };
		for (int i = 0; i < 6; i++) idx[i] = t[i];
	}
	for (int i = 0; i < 6; i++)
		((volatile uint16_t *)indices)[ib / 2 + i] = idx[i];

	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, 4 * XSDK_CANVAS_VERTEX_SIZE);
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, 6 * 2);

	_draw_primitive_internal(vb, 4, ib, 6, xform, modulate, tex, (flags & CANVAS_RECT_TILE) != 0);
}

void RasterizerCanvasXsdk::_draw_polygon(const Item *ci, const Item::CommandPolygon *command, const Transform2D &xform, const Color &modulate) {
	const Vector2 *points = command->points.ptr();
	const Vector2 *uvs = command->uvs.size() == command->points.size() ? command->uvs.ptr() : nullptr;
	const Color *colors = command->colors.ptr();
	const bool single_color = command->colors.size() == 1;
	const int point_count = command->points.size();
	const Vector<int> &cmd_indices = command->indices;
	const int index_count = cmd_indices.size();

	const uint32_t vb = _alloc_vertices(point_count);
	const uint32_t ib = _alloc_indices(index_count);

	for (int i = 0; i < point_count; i++) {
		Vector2 uv = uvs ? uvs[i] : Vector2(0.0f, 0.0f);
		Color color = single_color ? colors[0] : (colors ? colors[i] : Color(1, 1, 1, 1));
		_fill_vertex(vb + i * XSDK_CANVAS_VERTEX_SIZE, points[i], uv, color);
	}
	for (int i = 0; i < index_count; i++)
		((volatile uint16_t *)indices)[ib / 2 + i] = (uint16_t)cmd_indices[i];

	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, point_count * XSDK_CANVAS_VERTEX_SIZE);
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, index_count * 2);

	const RasterizerStorageXsdk::TextureData *tex = _bind_texture(command->texture);
	_draw_primitive_internal(vb, point_count, ib, index_count, xform, Color(1, 1, 1, 1), tex, false);
}

void RasterizerCanvasXsdk::_draw_polyline(const Item *ci, const Item::CommandPolyLine *command, const Transform2D &xform, const Color &modulate) {
	// Triangle strips for the antialiased fill, line strips for the lines.
	if (command->triangles.size() >= 3) {
		const Vector2 *points = command->triangles.ptr();
		const Vector<Color> &colors = command->triangle_colors;
		const bool single_color = colors.size() == 1;
		const int point_count = command->triangles.size();
		const uint32_t vb = _alloc_vertices(point_count);

		for (int i = 0; i < point_count; i++) {
			Color color = single_color ? colors[0] : (colors.size() == point_count ? colors[i] : Color(1, 1, 1, 1));
			_fill_vertex(vb + i * XSDK_CANVAS_VERTEX_SIZE, points[i], Vector2(0.0f, 0.0f), color);
		}
		xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, point_count * XSDK_CANVAS_VERTEX_SIZE);

		// Draw as repeated triangle list: p0 p1 p2, p2 p1 p3 ... simplest: treat as triangle list derived from the strip.
		const uint32_t indices_needed = 3 * (point_count - 2);
		const uint32_t ib = _alloc_indices(indices_needed);
		for (int i = 0; i < point_count - 2; i++) {
			((volatile uint16_t *)indices)[ib / 2 + i * 3 + 0] = (uint16_t)i;
			((volatile uint16_t *)indices)[ib / 2 + i * 3 + 1] = (uint16_t)(i + 1);
			((volatile uint16_t *)indices)[ib / 2 + i * 3 + 2] = (uint16_t)(i + 2);
		}
		xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, indices_needed * 2);

		const RasterizerStorageXsdk::TextureData *tex = _bind_texture(RID());
		_draw_primitive_internal(vb, point_count, ib, indices_needed, xform, Color(1, 1, 1, 1), tex, false);
		return;
	}

	if (command->lines.size() >= 2) {
		const Vector2 *points = command->lines.ptr();
		const Vector<Color> &colors = command->line_colors;
		const bool single_color = colors.size() == 1;
		const int point_count = command->lines.size();
		const uint32_t vb = _alloc_vertices(point_count);

		for (int i = 0; i < point_count; i++) {
			Color color = single_color ? colors[0] : (colors.size() == point_count ? colors[i] : Color(1, 1, 1, 1));
			_fill_vertex(vb + i * XSDK_CANVAS_VERTEX_SIZE, points[i], Vector2(0.0f, 0.0f), color);
		}
		xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, point_count * XSDK_CANVAS_VERTEX_SIZE);
		// Linear strip: 0..N-1 as line list (paired)
		const uint32_t ib = _alloc_indices((point_count - 1) * 2);
		for (int i = 0; i < point_count - 1; i++) {
			((volatile uint16_t *)indices)[ib / 2 + i * 2 + 0] = (uint16_t)i;
			((volatile uint16_t *)indices)[ib / 2 + i * 2 + 1] = (uint16_t)(i + 1);
		}
		xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, (point_count - 1) * 2 * 2);

		float mvp[16];
		compute_mvp(xform, width, height, mvp);
		xsdk_gpu_set_shaders(&vertex, &pixel);
		xsdk_built_shader_set_literals(&canvas_vert);
		xsdk_built_shader_set_literals(&canvas_frag);
		xsdk_gpu_set_vertex_constants(CANVAS_VERT_PUSH, mvp, CANVAS_VERT_PUSH_REGISTERS);
		const RasterizerStorageXsdk::TextureData *tex = _bind_texture(RID());
		if (tex)
			xsdk_gpu_set_texture(XSDK_CANVAS_TEXTURE_SLOT, &tex->tex, &_xsdk_storage()->clamp_sampler_state);
		xsdk_gpu_set_vertices(0, (uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, point_count * XSDK_CANVAS_VERTEX_SIZE);
		// xsdk line list as triangle-list of degenerate triangles is complex; use raw line draw if available.
		// XSDK_PRIMITIVE_LINE_LIST exists (2)
		xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_LINE_LIST, (uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, (point_count - 1) * 2, false);
	}
}

void RasterizerCanvasXsdk::_draw_primitive(const Item *ci, const Item::CommandPrimitive *command, const Transform2D &xform, const Color &modulate) {
	const Vector2 *points = command->points.ptr();
	const Vector2 *uvs = command->uvs.size() == command->points.size() ? command->uvs.ptr() : nullptr;
	const Vector<Color> &colors = command->colors;
	const bool single_color = colors.size() == 1;
	const int point_count = command->points.size();
	if (point_count < 3) {
		// Line strip for 2 points
		if (point_count == 2) {
			const uint32_t vb = _alloc_vertices(2);
			for (int i = 0; i < 2; i++) {
				Color color = single_color ? colors[0] : (colors.size() == 2 ? colors[i] : Color(1, 1, 1, 1));
				_fill_vertex(vb + i * XSDK_CANVAS_VERTEX_SIZE, points[i], uvs ? uvs[i] : Vector2(0, 0), color);
			}
			xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, 2 * XSDK_CANVAS_VERTEX_SIZE);

			float mvp[16];
			compute_mvp(xform, width, height, mvp);
			xsdk_gpu_set_shaders(&vertex, &pixel);
			xsdk_built_shader_set_literals(&canvas_vert);
			xsdk_built_shader_set_literals(&canvas_frag);
			xsdk_gpu_set_vertex_constants(CANVAS_VERT_PUSH, mvp, CANVAS_VERT_PUSH_REGISTERS);
			const RasterizerStorageXsdk::TextureData *tex = _bind_texture(command->texture);
			if (tex)
				xsdk_gpu_set_texture(XSDK_CANVAS_TEXTURE_SLOT, &tex->tex, &_xsdk_storage()->clamp_sampler_state);
			xsdk_gpu_set_vertices(0, (uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, 2 * XSDK_CANVAS_VERTEX_SIZE);
			const uint32_t ib = _alloc_indices(2);
			((volatile uint16_t *)indices)[ib / 2 + 0] = 0;
			((volatile uint16_t *)indices)[ib / 2 + 1] = 1;
			xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, 2 * 2);
			xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_LINE_LIST, (uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, 2, false);
		}
		return;
	}

	// Triangle fan for 3+ points
	const uint32_t vb = _alloc_vertices(point_count);
	for (int i = 0; i < point_count; i++) {
		Color color = single_color ? colors[0] : (colors.size() == point_count ? colors[i] : Color(1, 1, 1, 1));
		_fill_vertex(vb + i * XSDK_CANVAS_VERTEX_SIZE, points[i], uvs ? uvs[i] : Vector2(0.0f, 0.0f), color);
	}
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, point_count * XSDK_CANVAS_VERTEX_SIZE);

	const uint32_t tri_count = (uint32_t)(point_count - 2);
	const uint32_t ib = _alloc_indices(tri_count * 3);
	for (uint32_t i = 0; i < tri_count; i++) {
		((volatile uint16_t *)indices)[ib / 2 + i * 3 + 0] = 0;
		((volatile uint16_t *)indices)[ib / 2 + i * 3 + 1] = (uint16_t)(i + 1);
		((volatile uint16_t *)indices)[ib / 2 + i * 3 + 2] = (uint16_t)(i + 2);
	}
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, tri_count * 3 * 2);

	const RasterizerStorageXsdk::TextureData *tex = _bind_texture(command->texture);
	_draw_primitive_internal(vb, point_count, ib, tri_count * 3, xform, Color(1, 1, 1, 1), tex, false);
}

void RasterizerCanvasXsdk::_draw_circle(const Item *ci, const Item::CommandCircle *command, const Transform2D &xform, const Color &modulate) {
	// Filled circle: center + perimeter fan.
	enum { SEGMENTS = 24 };
	const float radius = command->radius;
	const Point2 center = command->pos;
	const Color color = command->color;

	const uint32_t vb = _alloc_vertices(SEGMENTS + 1);
	_fill_vertex(vb, center, Vector2(0.0f, 0.0f), color);
	for (int i = 0; i <= SEGMENTS; i++) {
		const float theta = 2.0f * Math_PI * (float)i / (float)SEGMENTS;
		const Vector2 p = center + Vector2(Math::cos(theta), Math::sin(theta)) * radius;
		_fill_vertex(vb + (i + 1) * XSDK_CANVAS_VERTEX_SIZE, p, Vector2(0.0f, 0.0f), color);
	}
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, (SEGMENTS + 1) * XSDK_CANVAS_VERTEX_SIZE);

	const uint32_t ib = _alloc_indices(SEGMENTS * 3);
	for (int i = 0; i < SEGMENTS; i++) {
		((volatile uint16_t *)indices)[ib / 2 + i * 3 + 0] = 0;
		((volatile uint16_t *)indices)[ib / 2 + i * 3 + 1] = (uint16_t)(i + 1);
		((volatile uint16_t *)indices)[ib / 2 + i * 3 + 2] = (uint16_t)(i + 2);
	}
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, SEGMENTS * 3 * 2);

	const RasterizerStorageXsdk::TextureData *tex = _bind_texture(RID());
	_draw_primitive_internal(vb, SEGMENTS + 1, ib, SEGMENTS * 3, xform, Color(1, 1, 1, 1), tex, false);
}

void RasterizerCanvasXsdk::_draw_line(const Item *ci, const Item::CommandLine *command, const Transform2D &xform, const Color &modulate) {
	const Vector2 from = command->from, to = command->to;
	const Color color = command->color;
	const float line_width = command->width;

	if (line_width <= 1.5f) {
		const uint32_t vb = _alloc_vertices(2);
		_fill_vertex(vb, from, Vector2(0.0f, 0.0f), color);
		_fill_vertex(vb + XSDK_CANVAS_VERTEX_SIZE, to, Vector2(0.0f, 0.0f), color);
		xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, 2 * XSDK_CANVAS_VERTEX_SIZE);

		float mvp[16];
		compute_mvp(xform, width, height, mvp);
		xsdk_gpu_set_shaders(&vertex, &pixel);
		xsdk_built_shader_set_literals(&canvas_vert);
		xsdk_built_shader_set_literals(&canvas_frag);
		xsdk_gpu_set_vertex_constants(CANVAS_VERT_PUSH, mvp, CANVAS_VERT_PUSH_REGISTERS);
		const RasterizerStorageXsdk::TextureData *tex = _bind_texture(RID());
		if (tex)
			xsdk_gpu_set_texture(XSDK_CANVAS_TEXTURE_SLOT, &tex->tex, &_xsdk_storage()->clamp_sampler_state);

		const uint32_t ib = _alloc_indices(2);
		((volatile uint16_t *)indices)[ib / 2 + 0] = 0;
		((volatile uint16_t *)indices)[ib / 2 + 1] = 1;
		xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, 2 * 2);
		xsdk_gpu_set_vertices(0, (uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, 2 * XSDK_CANVAS_VERTEX_SIZE);
		xsdk_gpu_draw_indexed(XSDK_PRIMITIVE_LINE_LIST, (uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, 2, false);
		return;
	}

	// Thick line: 4-vert quad oriented along the segment.
	const Vector2 dir = (to - from).normalized();
	const Vector2 perp = dir.tangent() * (line_width * 0.5f);

	const Vector2 pos[4] = {
		from - perp,
		from + perp,
		to + perp,
		to - perp,
	};

	const uint32_t vb = _alloc_vertices(4);
	for (int i = 0; i < 4; i++)
		_fill_vertex(vb + i * XSDK_CANVAS_VERTEX_SIZE, pos[i], Vector2(0.0f, 0.0f), color);
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, 4 * XSDK_CANVAS_VERTEX_SIZE);

	const uint32_t ib = _alloc_indices(6);
	const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; i++)
		((volatile uint16_t *)indices)[ib / 2 + i] = idx[i];
	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, 6 * 2);

	const RasterizerStorageXsdk::TextureData *tex = _bind_texture(RID());
	_draw_primitive_internal(vb, 4, ib, 6, xform, Color(1, 1, 1, 1), tex, false);
}

void RasterizerCanvasXsdk::_draw_multirect(const Item *ci, const Item::CommandMultiRect *command, const Transform2D &xform, const Color &modulate) {
	const Vector<Rect2> &rects = command->rects;
	const Vector<Rect2> &sources = command->sources;
	const int count = rects.size();
	if (count == 0)
		return;

	const RasterizerStorageXsdk::TextureData *tex = _bind_texture(command->texture);
	if (!tex)
		tex = _bind_texture(RID());
	const float fx = 1.0f / (float)tex->width;
	const float fy = 1.0f / (float)tex->height;

	const uint32_t vb_base = _alloc_vertices(count * 6);
	static const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };

	for (int i = 0; i < count; i++) {
		const Rect2 &r = rects[i];
		const Rect2 &s = i < sources.size() ? sources[i] : Rect2(0.0f, 0.0f, (float)tex->width, (float)tex->height);

		Vector2 pos[4] = {
			r.position,
			r.position + Vector2(r.size.x, 0.0f),
			r.position + r.size,
			r.position + Vector2(0.0f, r.size.y),
		};
		Vector2 uv[4] = {
			Vector2(s.position.x * fx, s.position.y * fy),
			Vector2((s.position.x + s.size.x) * fx, s.position.y * fy),
			Vector2((s.position.x + s.size.x) * fx, (s.position.y + s.size.y) * fy),
			Vector2(s.position.x * fx, (s.position.y + s.size.y) * fy),
		};

		const uint32_t vb = vb_base + i * 6 * XSDK_CANVAS_VERTEX_SIZE;
		const Color color = command->modulate;
		for (int j = 0; j < 6; j++)
			_fill_vertex(vb + j * XSDK_CANVAS_VERTEX_SIZE, pos[idx[j]], uv[idx[j]], color);
	}

	xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb_base, count * 6 * XSDK_CANVAS_VERTEX_SIZE);

	_draw_primitive_nonindexed(vb_base, count * 6, xform, modulate, tex, false);
}

void RasterizerCanvasXsdk::canvas_render_items(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	while (p_item_list) {
		Item *ci = p_item_list;

		if (ci->visible && !ci->commands.empty()) {
			if (ci->final_clip_owner) {
				const Rect2 &clip = ci->final_clip_owner->final_clip_rect;
				const uint32_t cx = (uint32_t)CLAMP(clip.position.x, 0, (int)width);
				const uint32_t cy = (uint32_t)CLAMP(clip.position.y, 0, (int)height);
				const uint32_t cw = (uint32_t)MAX(0.0f, MIN(clip.size.width, (float)width - (float)cx));
				const uint32_t ch = (uint32_t)MAX(0.0f, MIN(clip.size.height, (float)height - (float)cy));
				xsdk_gpu_set_scissor(cx, cy, cw == 0 ? 1 : cw, ch == 0 ? 1 : ch);
			} else {
				xsdk_gpu_set_scissor(0, 0, width, height);
			}

			const Transform2D xform = ci->final_transform;
			const Color modulate = ci->final_modulate;

			for (int cmd_i = 0; cmd_i < ci->commands.size(); cmd_i++) {
				Item::Command *c = ci->commands[cmd_i];
				switch (c->type) {
					case Item::Command::TYPE_RECT:
						_draw_rect(ci, static_cast<Item::CommandRect *>(c), xform, modulate);
						break;
					case Item::Command::TYPE_MULTIRECT:
						_draw_multirect(ci, static_cast<Item::CommandMultiRect *>(c), xform, modulate);
						break;
					case Item::Command::TYPE_POLYGON:
						_draw_polygon(ci, static_cast<Item::CommandPolygon *>(c), xform, modulate);
						break;
					case Item::Command::TYPE_POLYLINE:
						_draw_polyline(ci, static_cast<Item::CommandPolyLine *>(c), xform, modulate);
						break;
					case Item::Command::TYPE_PRIMITIVE:
						_draw_primitive(ci, static_cast<Item::CommandPrimitive *>(c), xform, modulate);
						break;
					case Item::Command::TYPE_CIRCLE:
						_draw_circle(ci, static_cast<Item::CommandCircle *>(c), xform, modulate);
						break;
					case Item::Command::TYPE_LINE:
						_draw_line(ci, static_cast<Item::CommandLine *>(c), xform, modulate);
						break;
					case Item::Command::TYPE_NINEPATCH: {
						const Item::CommandNinePatch *cmd = static_cast<const Item::CommandNinePatch *>(c);
						Vector2 pos[4] = {
							cmd->rect.position,
							cmd->rect.position + Vector2(cmd->rect.size.x, 0.0f),
							cmd->rect.position + cmd->rect.size,
							cmd->rect.position + Vector2(0.0f, cmd->rect.size.y),
						};
						Vector2 uv[4] = { Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1) };
						const uint32_t vb = _alloc_vertices(4);
						for (int j = 0; j < 4; j++)
							_fill_vertex(vb + j * XSDK_CANVAS_VERTEX_SIZE, pos[j], uv[j], cmd->color);
						xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(vertices) + vb, 4 * XSDK_CANVAS_VERTEX_SIZE);

						const uint32_t ib = _alloc_indices(6);
						const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };
						for (int j = 0; j < 6; j++)
							((volatile uint16_t *)indices)[ib / 2 + j] = idx[j];
						xsdk_gpu_invalidate((uint32_t)(uintptr_t)xsdk_gpu_address(indices) + ib, 6 * 2);

						const RasterizerStorageXsdk::TextureData *tex = _bind_texture(cmd->texture);
						_draw_primitive_internal(vb, 4, ib, 6, xform, Color(1, 1, 1, 1), tex, false);
						break;
					}
					case Item::Command::TYPE_MESH:
					case Item::Command::TYPE_MULTIMESH:
					case Item::Command::TYPE_PARTICLES:
					case Item::Command::TYPE_TRANSFORM:
					case Item::Command::TYPE_CLIP_IGNORE:
						break;
				}
			}
		}
		p_item_list = ci->next;
	}
	xsdk_gpu_set_scissor(0, 0, width, height);
}
