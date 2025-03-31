#include "game.h"

#include <core/event.h>
#include <core/logger.h>
#include <renderer/renderer_frontend.h>
#include <renderer/renderer_types.h>

b8 game_initialize(game *game_inst)
{
    DDEBUG("game_initialize() called!");
    return true;
}

b8 game_update(game *game_inst, f32 delta_time)
{
    return true;
}

b8 game_render(game *game_inst, f32 delta_time)
{
    render_packet packet = {delta_time};
    b8            result = renderer_draw_frame(&packet);
    return result;
}

void game_on_resize(game *game_inst, u32 width, u32 height)
{
    
}
