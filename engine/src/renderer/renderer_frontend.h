#pragma once

#include "renderer_types.h"

struct static_mesh_data;
struct platform_state;

b8 renderer_system_initialize(u64 *renderer_mem_requirements, void *state, const char *application_name);

void renderer_system_shutdown();

void renderer_on_resized(u16 width, u16 height);

b8 renderer_draw_frame(render_packet *packet);

DAPI void renderer_set_view(mat4 view);
