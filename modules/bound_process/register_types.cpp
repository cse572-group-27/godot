#include "register_types.h"

#include "bound_process.h"


void initialize_bound_process_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) { return; }

    GDREGISTER_CLASS(BoundProcess);
}


void uninitialize_bound_process_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) { return; }
}