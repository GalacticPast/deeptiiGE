#include "vulkan_object_shader.h"
#include "core/dmemory.h"
#include "core/logger.h"
#include "math/math_types.h"
#include "renderer/vulkan/vulkan_buffer.h"
#include "renderer/vulkan/vulkan_pipeline.h"
#include "renderer/vulkan/vulkan_shader_utils.h"

#define BUILTIN_SHADER_OBJECT_NAME "Builtin.ObjectShader"
#define ATTRIBUTE_COUNT 1

b8 vulkan_object_shader_create(vulkan_context *context, vulkan_object_shader *out_shader)
{
    char                  stage_type_strings[OBJECT_SHADER_STAGE_COUNT][5] = {"vert", "frag"};
    VkShaderStageFlagBits stage_types[OBJECT_SHADER_STAGE_COUNT] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; i++)
    {
        if (!create_shader_module(context, BUILTIN_SHADER_OBJECT_NAME, stage_type_strings[i], stage_types[i], i, out_shader->stages))
        {
            DERROR("Unable to create %s shader module for '%s'.", stage_type_strings[i], BUILTIN_SHADER_OBJECT_NAME);
            return false;
        }
    }
    // Global Descriptors
    VkDescriptorSetLayoutBinding global_ubo_layout_binding = {};
    global_ubo_layout_binding.binding = 0;
    global_ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    global_ubo_layout_binding.descriptorCount = 1;
    global_ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    global_ubo_layout_binding.pImmutableSamplers = 0;

    VkDescriptorSetLayoutCreateInfo global_descriptor_set_layout_create_info = {};
    global_descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    global_descriptor_set_layout_create_info.bindingCount = 1;
    global_descriptor_set_layout_create_info.pBindings = &global_ubo_layout_binding;

    VK_CHECK(vkCreateDescriptorSetLayout(context->device.logical_device, &global_descriptor_set_layout_create_info, context->allocator, &out_shader->global_descriptor_set_layout));

    // for our global projection/view matrix
    VkDescriptorPoolSize global_descriptor_pool_size;
    global_descriptor_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    global_descriptor_pool_size.descriptorCount = context->swapchain.image_count;

    VkDescriptorPoolCreateInfo global_descriptor_pool_create_info = {};
    global_descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    global_descriptor_pool_create_info.maxSets = context->swapchain.image_count;
    global_descriptor_pool_create_info.poolSizeCount = 1;
    global_descriptor_pool_create_info.pPoolSizes = &global_descriptor_pool_size;

    VK_CHECK(vkCreateDescriptorPool(context->device.logical_device, &global_descriptor_pool_create_info, context->allocator, &out_shader->global_descriptor_pool));

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

    /* Descriptors set layouts */

    const s32             descriptor_set_layouts_count = 1;
    VkDescriptorSetLayout descriptor_set_layouts[1] = {out_shader->global_descriptor_set_layout};

    // Stages
    // NOTE: Should match the number of shader->stages.
    VkPipelineShaderStageCreateInfo stage_create_infos[OBJECT_SHADER_STAGE_COUNT];
    dzero_memory(stage_create_infos, sizeof(stage_create_infos));

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; ++i)
    {
        stage_create_infos[i].sType = out_shader->stages[i].shader_stage_create_info.sType;
        stage_create_infos[i] = out_shader->stages[i].shader_stage_create_info;
    }

    b8 result = vulkan_graphics_pipeline_create(context, &context->main_renderpass, ATTRIBUTE_COUNT, attribute_descriptions, descriptor_set_layouts_count, descriptor_set_layouts, OBJECT_SHADER_STAGE_COUNT,
                                                stage_create_infos, viewport, scissor, false, &out_shader->pipeline);
    if (!result)
    {
        DERROR("Failed to load graphics pipeline for object shader.");
        return false;
    }

    result = vulkan_buffer_create(context, sizeof(global_uniform_object), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true, &out_shader->global_uniform_buffer);

    if (!result)
    {
        DERROR("Vulkan buffer creation failed for object shader.");
        return false;
    }

    VkDescriptorSetLayout global_layouts[3] = {
        out_shader->global_descriptor_set_layout,
        out_shader->global_descriptor_set_layout,
        out_shader->global_descriptor_set_layout,
    };

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = out_shader->global_descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = 3;
    descriptor_set_allocate_info.pSetLayouts = global_layouts;

    VK_CHECK(vkAllocateDescriptorSets(context->device.logical_device, &descriptor_set_allocate_info, out_shader->global_descriptor_sets));

    return true;
}

void vulkan_object_shader_destroy(vulkan_context *context, vulkan_object_shader *shader)
{
    vulkan_buffer_destroy(context, &shader->global_uniform_buffer);

    vulkan_pipeline_destroy(context, &shader->pipeline);

    vkDestroyDescriptorPool(context->device.logical_device, shader->global_descriptor_pool, context->allocator);

    vkDestroyDescriptorSetLayout(context->device.logical_device, shader->global_descriptor_set_layout, context->allocator);

    for (u32 i = 0; i < OBJECT_SHADER_STAGE_COUNT; i++)
    {
        vkDestroyShaderModule(context->device.logical_device, shader->stages[i].handle, context->allocator);
        shader->stages[i].handle = 0;
    }
}

void vulkan_object_shader_use(vulkan_context *context, vulkan_object_shader *shader)
{
    u32 image_index = context->image_index;
    vulkan_pipeline_bind(&context->graphics_command_buffers[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, &shader->pipeline);
}

void vulkan_object_shader_update_global_state(vulkan_context *context, vulkan_object_shader *shader)
{
    u32             image_index = context->image_index;
    VkCommandBuffer cmd_buffer = context->graphics_command_buffers[image_index].handle;
    VkDescriptorSet global_descriptor = shader->global_descriptor_sets[image_index];

    vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline.pipeline_layout, 0, 1, &global_descriptor, 0, 0);

    u32 range = sizeof(global_uniform_object);
    u64 offset = 0;

    vulkan_buffer_load_data(context, &shader->global_uniform_buffer, offset, range, 0, &shader->global_ubo);

    VkDescriptorBufferInfo descriptor_buffer_info = {};

    descriptor_buffer_info.buffer = shader->global_uniform_buffer.handle;
    descriptor_buffer_info.offset = offset;
    descriptor_buffer_info.range = range;

    VkWriteDescriptorSet descriptor_write = {};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet = shader->global_descriptor_sets[image_index];
    descriptor_write.dstBinding = 0;
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_write.pBufferInfo = &descriptor_buffer_info;

    vkUpdateDescriptorSets(context->device.logical_device, 1, &descriptor_write, 0, 0);
}

void vulkan_object_shader_update_object(vulkan_context *context, vulkan_object_shader *shader, mat4 model)
{
    u32             image_index = context->image_index;
    VkCommandBuffer command_buffer = context->graphics_command_buffers[image_index].handle;

    // NOTE: guarenteed for only 128 bytes
    vkCmdPushConstants(command_buffer, shader->pipeline.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4), &model);
}
