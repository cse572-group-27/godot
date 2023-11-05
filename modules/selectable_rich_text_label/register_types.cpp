#include "register_types.h"

#include "selectable_rich_text_label.h"


void initialize_selectable_rich_text_label_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) { return; }

    GDREGISTER_CLASS(SelectableRichTextLabel);
}


void uninitialize_selectable_rich_text_label_module(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) { return; }
}