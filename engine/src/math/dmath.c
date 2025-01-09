#include "dmath.h"
#include "platform/platform.h"

#include <math.h>
#include <stdlib.h>

static b8 rand_seeded = false;

/**
 * Note that these are here in order to prevent having to import the
 * entire <math.h> everywhere.
 */
f32 dsin(f32 x)
{
    return sinf(x);
}

f32 dcos(f32 x)
{
    return cosf(x);
}

f32 dtan(f32 x)
{
    return tanf(x);
}

f32 dacos(f32 x)
{
    return acosf(x);
}

f32 dsqrt(f32 x)
{
    return sqrtf(x);
}

f32 dabs(f32 x)
{
    return fabsf(x);
}

i32 drandom()
{
    if (!rand_seeded)
    {
        srand((u32)platform_get_absolute_time());
        rand_seeded = true;
    }
    return rand();
}

i32 drandom_in_range(i32 min, i32 max)
{
    if (!rand_seeded)
    {
        srand((u32)platform_get_absolute_time());
        rand_seeded = true;
    }
    return (rand() % (max - min + 1)) + min;
}

f32 fdrandom()
{
    return (float)drandom() / (f32)RAND_MAX;
}

f32 fdrandom_in_range(f32 min, f32 max)
{
    return min + ((float)drandom() / ((f32)RAND_MAX / (max - min)));
}
