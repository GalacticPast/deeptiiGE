#pragma once

#include "renderer_types.h"

struct static_mesh_data;
struct platform_state;

b8   renderer_initialize(u64 *renderer_mem_requirements, void *state, const char *application_name);
void renderer_shutdown();

DAPI void renderer_on_resized(u16 width, u16 height);

DAPI b8 renderer_draw_frame(render_packet *packet);
