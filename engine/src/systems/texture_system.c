#include "texture_system.h"

#include "containers/hashtable.h"
#include "core/dmemory.h"
#include "core/dstring.h"
#include "core/logger.h"

#include "renderer/renderer_frontend.h"

#include "systems/resource_system.h"

typedef struct texture_system_state
{
    texture_system_config config;
    texture default_texture;

    // Array of registered textures.
    texture *registered_textures;

    // Hashtable for texture lookups.
    hashtable registered_texture_table;
} texture_system_state;

typedef struct texture_reference
{
    u64 reference_count;
    u32 handle;
    b8 auto_release;
} texture_reference;

static texture_system_state *state_ptr = 0;

b8 create_default_textures(texture_system_state *state);
void destroy_default_textures(texture_system_state *state);
b8 load_texture(const char *texture_name, texture *t);
void destroy_texture(texture *t);

b8 texture_system_initialize(u64 *memory_requirement, void *state, texture_system_config config)
{
    if (config.max_texture_count == 0)
    {
        DFATAL("texture_system_initialize - config.max_texture_count must be > 0.");
        return false;
    }

    // Block of memory will contain state structure, then block for array, then block for hashtable.
    u64 struct_requirement    = sizeof(texture_system_state);
    u64 array_requirement     = sizeof(texture) * config.max_texture_count;
    u64 hashtable_requirement = sizeof(texture_reference) * config.max_texture_count;
    *memory_requirement       = struct_requirement + array_requirement + hashtable_requirement;

    if (!state)
    {
        return true;
    }

    state_ptr         = state;
    state_ptr->config = config;

    // The array block is after the state. Already allocated, so just set the pointer.
    void *array_block              = state + struct_requirement;
    state_ptr->registered_textures = array_block;

    // Hashtable block is after array.
    void *hashtable_block = array_block + array_requirement;

    // Create a hashtable for texture lookups.
    hashtable_create(sizeof(texture_reference), config.max_texture_count, hashtable_block, false,
                     &state_ptr->registered_texture_table);

    // Fill the hashtable with invalid references to use as a default.
    texture_reference invalid_ref;
    invalid_ref.auto_release    = false;
    invalid_ref.handle          = INVALID_ID; // Primary reason for needing default values.
    invalid_ref.reference_count = 0;
    hashtable_fill(&state_ptr->registered_texture_table, &invalid_ref);

    // Invalidate all textures in the array.
    u32 count = state_ptr->config.max_texture_count;
    for (u32 i = 0; i < count; ++i)
    {
        state_ptr->registered_textures[i].id         = INVALID_ID;
        state_ptr->registered_textures[i].generation = INVALID_ID;
    }

    // Create default textures for use in the system.
    create_default_textures(state_ptr);

    return true;
}

void texture_system_shutdown(void *state)
{
    if (state_ptr)
    {
        // Destroy all loaded textures.
        for (u32 i = 0; i < state_ptr->config.max_texture_count; ++i)
        {
            texture *t = &state_ptr->registered_textures[i];
            if (t->generation != INVALID_ID)
            {
                renderer_destroy_texture(t);
            }
        }

        destroy_default_textures(state_ptr);

        state_ptr = 0;
    }
}

texture *texture_system_acquire(const char *name, b8 auto_release)
{
    // Return default texture, but warn about it since this should be returned via get_default_texture();
    if (strings_equali(name, DEFAULT_TEXTURE_NAME))
    {
        DWARN("texture_system_acquire called for default texture. Use texture_system_get_default_texture for texture "
              "'default'.");
        return &state_ptr->default_texture;
    }

    texture_reference ref;
    if (state_ptr && hashtable_get(&state_ptr->registered_texture_table, name, &ref))
    {
        // This can only be changed the first time a texture is loaded.
        if (ref.reference_count == 0)
        {
            ref.auto_release = auto_release;
        }
        ref.reference_count++;
        if (ref.handle == INVALID_ID)
        {
            // This means no texture exists here. Find a free index first.
            u32 count  = state_ptr->config.max_texture_count;
            texture *t = 0;
            for (u32 i = 0; i < count; ++i)
            {
                if (state_ptr->registered_textures[i].id == INVALID_ID)
                {
                    // A free slot has been found. Use its index as the handle.
                    ref.handle = i;
                    t          = &state_ptr->registered_textures[i];
                    break;
                }
            }

            // Make sure an empty slot was actually found.
            if (!t || ref.handle == INVALID_ID)
            {
                DFATAL("texture_system_acquire - Texture system cannot hold anymore textures. Adjust configuration to "
                       "allow more.");
                return 0;
            }

            // Create new texture.
            if (!load_texture(name, t))
            {
                DERROR("Failed to load texture '%s'.", name);
                return 0;
            }

            // Also use the handle as the texture id.
            t->id = ref.handle;
            DTRACE("Texture '%s' does not yet exist. Created, and ref_count is now %i.", name, ref.reference_count);
        }
        else
        {
            DTRACE("Texture '%s' already exists, ref_count increased to %i.", name, ref.reference_count);
        }

        // Update the entry.
        hashtable_set(&state_ptr->registered_texture_table, name, &ref);
        return &state_ptr->registered_textures[ref.handle];
    }

    // NOTE: This would only happen in the event something went wrong with the state.
    DERROR("texture_system_acquire failed to acquire texture '%s'. Null pointer will be returned.", name);
    return 0;
}

void texture_system_release(const char *name)
{
    // Ignore release requests for the default texture.
    if (strings_equali(name, DEFAULT_TEXTURE_NAME))
    {
        return;
    }
    texture_reference ref;
    if (state_ptr && hashtable_get(&state_ptr->registered_texture_table, name, &ref))
    {
        if (ref.reference_count == 0)
        {
            DWARN("Tried to release non-existent texture: '%s'", name);
            return;
        }

        // Take a copy of the name since it will be wiped out by destroy,
        // (as passed in name is generally a pointer to the actual texture's name).
        char name_copy[TEXTURE_NAME_MAX_LENGTH];
        string_ncopy(name_copy, name, TEXTURE_NAME_MAX_LENGTH);

        ref.reference_count--;
        if (ref.reference_count == 0 && ref.auto_release)
        {
            texture *t = &state_ptr->registered_textures[ref.handle];

            // Destroy/reset texture.
            destroy_texture(t);

            // Reset the reference.
            ref.handle       = INVALID_ID;
            ref.auto_release = false;
            DTRACE("Released texture '%s'., Texture unloaded because reference count=0 and auto_release=true.",
                   name_copy);
        }
        else
        {
            DTRACE("Released texture '%s', now has a reference count of '%i' (auto_release=%s).", name_copy,
                   ref.reference_count, ref.auto_release ? "true" : "false");
        }

        // Update the entry.
        hashtable_set(&state_ptr->registered_texture_table, name_copy, &ref);
    }
    else
    {
        DERROR("texture_system_release failed to release texture '%s'.", name);
    }
}

texture *texture_system_get_default_texture()
{
    if (state_ptr)
    {
        return &state_ptr->default_texture;
    }

    DERROR("texture_system_get_default_texture called before texture system initialization! Null pointer returned.");
    return 0;
}

b8 create_default_textures(texture_system_state *state)
{
    // NOTE: Create default texture, a 256x256 blue/white checkerboard pattern.
    // This is done in code to eliminate asset dependencies.
    DTRACE("Creating default texture...");
    const u32 tex_dimension = 256;
    const u32 channels      = 4;
    const u32 pixel_count   = tex_dimension * tex_dimension;
    u8 pixels[pixel_count * channels];
    dset_memory(pixels, 255, sizeof(u8) * pixel_count * channels);

    // Each pixel.
    for (u64 row = 0; row < tex_dimension; ++row)
    {
        for (u64 col = 0; col < tex_dimension; ++col)
        {
            u64 index     = (row * tex_dimension) + col;
            u64 index_bpp = index * channels;
            if (row % 2)
            {
                if (col % 2)
                {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
            else
            {
                if (!(col % 2))
                {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
        }
    }

    string_ncopy(state->default_texture.name, DEFAULT_TEXTURE_NAME, TEXTURE_NAME_MAX_LENGTH);
    state->default_texture.width            = tex_dimension;
    state->default_texture.height           = tex_dimension;
    state->default_texture.channel_count    = 4;
    state->default_texture.generation       = INVALID_ID;
    state->default_texture.has_transparency = false;
    renderer_create_texture(pixels, &state->default_texture);
    // Manually set the texture generation to invalid since this is a default texture.
    state->default_texture.generation = INVALID_ID;

    return true;
}

void destroy_default_textures(texture_system_state *state)
{
    if (state)
    {
        destroy_texture(&state->default_texture);
    }
}

b8 load_texture(const char *texture_name, texture *t)
{
    resource img_resource;
    if (!resource_system_load(texture_name, RESOURCE_TYPE_IMAGE, &img_resource))
    {
        DERROR("Failed to load image resource for texture '%s'", texture_name);
        return false;
    }

    image_resource_data *resource_data = img_resource.data;

    // Use a temporary texture to load into.
    texture temp_texture;
    temp_texture.width         = resource_data->width;
    temp_texture.height        = resource_data->height;
    temp_texture.channel_count = resource_data->channel_count;

    u32 current_generation = t->generation;
    t->generation          = INVALID_ID;

    u64 total_size = temp_texture.width * temp_texture.height * temp_texture.channel_count;
    // Check for transparency
    b32 has_transparency = false;
    for (u64 i = 0; i < total_size; i += temp_texture.channel_count)
    {
        u8 a = resource_data->pixels[i + 3];
        if (a < 255)
        {
            has_transparency = true;
            break;
        }
    }

    // Take a copy of the name.
    string_ncopy(temp_texture.name, texture_name, TEXTURE_NAME_MAX_LENGTH);
    temp_texture.generation       = INVALID_ID;
    temp_texture.has_transparency = has_transparency;

    // Acquire internal texture resources and upload to GPU.
    renderer_create_texture(resource_data->pixels, &temp_texture);

    // Take a copy of the old texture.
    texture old = *t;

    // Assign the temp texture to the pointer.
    *t = temp_texture;

    // Destroy the old texture.
    renderer_destroy_texture(&old);

    if (current_generation == INVALID_ID)
    {
        t->generation = 0;
    }
    else
    {
        t->generation = current_generation + 1;
    }

    // Clean up data.
    resource_system_unload(&img_resource);
    return true;
}

void destroy_texture(texture *t)
{
    // Clean up backend resources.
    renderer_destroy_texture(t);

    dzero_memory(t->name, sizeof(char) * TEXTURE_NAME_MAX_LENGTH);
    dzero_memory(t, sizeof(texture));
    t->id         = INVALID_ID;
    t->generation = INVALID_ID;
}