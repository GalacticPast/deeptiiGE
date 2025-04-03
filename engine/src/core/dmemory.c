#include "dmemory.h"

#include "core/dstring.h"
#include "core/logger.h"
#include "platform/platform.h"

// TODO: Custom string lib
#include <stdio.h>
#include <string.h>

typedef struct memory_stats
{
    u64 total_allocated;
    u64 tagged_allocations[MEMORY_TAG_MAX_TAGS];
} memory_stats;

typedef struct memory_state
{
    u64          alloc_count;
    memory_stats stats;
} memory_state;

// clang-format off
static const char *memory_tag_strings[MEMORY_TAG_MAX_TAGS] = 
{
    "UNKNOWN    ",
    "ARRAY      ",
    "LINEAR ALOC",
    "DARRAY     ",
    "DICT       ",
    "RING_QUEUE ",
    "BST        ",
    "STRING     ",
    "APPLICATION",
    "JOB        ",
    "TEXTURE    ",
    "MAT_INST   ",
    "RENDERER   ",
    "GAME       ",
    "TRANSFORM  ",
    "ENTITY     ",
    "ENTITY_NODE",
    "SCENE      "
};

// clang-format on

static memory_state *mem_state_ptr;

DAPI b8 memory_system_initialize(u64 *memory_system_mem_requirements, void *mem_state)
{
    *memory_system_mem_requirements = sizeof(memory_state);
    if (mem_state == 0)
    {
        return true;
    }
    mem_state_ptr = (memory_state *)mem_state;

    mem_state_ptr->alloc_count = 0;
    platform_zero_memory(&mem_state_ptr->stats, sizeof(mem_state_ptr->stats));

    DINFO("Memory system initalized.");
    return true;
}

void memory_shutdown(void *mem_state)
{
    mem_state_ptr = 0;
}

void *dallocate(u64 size, memory_tag tag)
{
    if (tag == MEMORY_TAG_UNKNOWN)
    {
        DWARN("dallocate called using MEMORY_TAG_UNKNOWN. Re-class this allocation.");
    }

    if (mem_state_ptr)
    {
        mem_state_ptr->stats.total_allocated += size;
        mem_state_ptr->stats.tagged_allocations[tag] += size;
        mem_state_ptr->alloc_count++;
    }
    DWARN("Allocating %llu bytes", size);

    // TODO: Memory alignment
    void *block = platform_allocate(size, false);
    platform_zero_memory(block, size);
    return block;
}

void dfree(void *block, u64 size, memory_tag tag)
{
    if (tag == MEMORY_TAG_UNKNOWN)
    {
        DWARN("dfree called using MEMORY_TAG_UNKNOWN. Re-class this allocation.");
    }

    if (mem_state_ptr)
    {
        mem_state_ptr->stats.total_allocated -= size;
        mem_state_ptr->stats.tagged_allocations[tag] -= size;
    }

    // TODO: Memory alignment
    platform_free(block, false);
}

void *dzero_memory(void *block, u64 size)
{
    return platform_zero_memory(block, size);
}

void *dcopy_memory(void *dest, const void *source, u64 size)
{
    return platform_copy_memory(dest, source, size);
}

void *dset_memory(void *dest, s32 value, u64 size)
{
    return platform_set_memory(dest, value, size);
}

char *get_memory_usage_str()
{
    const u64 gib = 1024 * 1024 * 1024;
    const u64 mib = 1024 * 1024;
    const u64 kib = 1024;

    char buffer[8000] = "System memory use (tagged):\n";
    u64  offset       = strlen(buffer);
    for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i)
    {
        char  unit[4] = "XiB";
        float amount  = 1.0f;
        if (mem_state_ptr->stats.tagged_allocations[i] >= gib)
        {
            unit[0] = 'G';
            amount  = mem_state_ptr->stats.tagged_allocations[i] / (float)gib;
        }
        else if (mem_state_ptr->stats.tagged_allocations[i] >= mib)
        {
            unit[0] = 'M';
            amount  = mem_state_ptr->stats.tagged_allocations[i] / (float)mib;
        }
        else if (mem_state_ptr->stats.tagged_allocations[i] >= kib)
        {
            unit[0] = 'K';
            amount  = mem_state_ptr->stats.tagged_allocations[i] / (float)kib;
        }
        else
        {
            unit[0] = 'B';
            unit[1] = 0;
            amount  = (float)mem_state_ptr->stats.tagged_allocations[i];
        }

        s32 length = snprintf(buffer + offset, 8000, "  %s: %.2f%s\n", memory_tag_strings[i], amount, unit);
        offset += length;
    }
    char *out_string = string_duplicate(buffer);
    return out_string;
}

u64 get_memory_alloc_count()
{
    if (mem_state_ptr)
    {
        return mem_state_ptr->alloc_count;
    }
    return 0;
}

void add_stats(u64 size, memory_tag tag)
{
    if (mem_state_ptr)
    {
        mem_state_ptr->stats.total_allocated += size;
        mem_state_ptr->stats.tagged_allocations[tag] += size;
        mem_state_ptr->alloc_count++;
    }
}
