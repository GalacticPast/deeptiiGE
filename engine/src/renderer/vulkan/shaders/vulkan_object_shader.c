#include "vulkan_object_shader.h"
#include "core/dmemory.h"
#include "core/logger.h"
#include "math/math_types.h"
#include "renderer/vulkan/vulkan_pipeline.h"
#include "renderer/vulkan/vulkan_shader_utils.h"

#define BUILTIN_SHADER_OBJECT_NAME "Builtin.ObjectShader"
#define ATTRIBUTE_COUNT 1

b8 vulkan_object_shader_create(vulkan_context *context, vulkan_object_shader *out_shader)
{
    char                  stage_type_strings[OBJECT_SHADER_STAGE_COUNT][5] = {"vert", "frag"};
    VkShaderStageFlagBits stage_types[OBJECT_SHADER_STAGE_COUNT] = {VK_SHADER_STAGE_VERTEX_BIT,
                                                                    VK_SHADER_STAGE_FRAGMENT_BIT};

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; i++)
    {
        if (!create_shader_module(context, BUILTIN_SHADER_OBJECT_NAME, stage_type_strings[i], stage_types[i], i,
                                  out_shader->stages))
        {
            DERROR("Unable to create %s shader module for '%s'.", stage_type_strings[i], BUILTIN_SHADER_OBJECT_NAME);
            return false;
        }
    }

    // pipeline creation

    // Pipeline creation
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (f32)context->framebuffer_height;
    viewport.width = (f32)context->framebuffer_width;
    viewport.height = -(f32)context->framebuffer_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = context->framebuffer_width;
    scissor.extent.height = context->framebuffer_height;

    // Attributes
    u32                               offset = 0;
    VkVertexInputAttributeDescription attribute_descriptions[ATTRIBUTE_COUNT];

    // Position
    VkFormat formats[ATTRIBUTE_COUNT] = {VK_FORMAT_R32G32B32_SFLOAT};
    u64      sizes[ATTRIBUTE_COUNT] = {sizeof(vec3)};

    for (u32 i = 0; i < ATTRIBUTE_COUNT; ++i)
    {
        attribute_descriptions[i].binding = 0;  // binding index - should match binding desc
        attribute_descriptions[i].location = i; // attrib location
        attribute_descriptions[i].format = formats[i];
        attribute_descriptions[i].offset = offset;
        offset += sizes[i];
    }

    // TODO: Desciptor set layouts.

    // Stages
    // NOTE: Should match the number of shader->stages.
    VkPipelineShaderStageCreateInfo stage_create_infos[OBJECT_SHADER_STAGE_COUNT];
    dzero_memory(stage_create_infos, sizeof(stage_create_infos));

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i)
    {
        stage_create_infos[i].sType = out_shader->stages[i].shader_stage_create_info.sType;
        stage_create_infos[i] = out_shader->stages[i].shader_stage_create_info;
    }

    if (!vulkan_graphics_pipeline_create(context, &context->main_renderpass, ATTRIBUTE_COUNT, attribute_descriptions, 0,
                                         0, OBJECT_SHADER_STAGE_COUNT, stage_create_infos, viewport, scissor, false,
                                         &out_shader->pipeline))
    {
        DERROR("Failed to load graphics pipeline for object shader.");
        return false;
    }

    return true;
}

void vulkan_object_shader_destroy(vulkan_context *context, vulkan_object_shader *shader)
{
    vulkan_pipeline_destroy(context, &shader->pipeline);

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; i++)
    {
        vkDestroyShaderModule(context->device.logical_device, shader->stages[i].handle, context->allocator);
        shader->stages[i].handle = 0;
    }
}

void vulkan_object_shader_use(vulkan_context *context, vulkan_object_shader *shader)
{
    u32 image_index = context->image_index;
    vulkan_pipeline_bind(&context->graphics_command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
                         &shader->pipeline);
}
