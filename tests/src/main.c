#include "containers/hashtable_tests.h"
#include "memory/linear_allocator_tests.h"
#include "test_manager.h"

#include <core/logger.h>
int main()
{
    test_manager_init();

    DDEBUG("Starting tests...");

    linear_allocator_register_tests();
    hashtable_register_tests();

    test_manager_run_tests();

    return 0;
}
