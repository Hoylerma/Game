#include "register_types.h"

#include "uhrwerk_simulation.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

void initialize_uhrwerk_module(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(uhrwerk_bridge::UhrwerkSimulation);
}

void uninitialize_uhrwerk_module(godot::ModuleInitializationLevel /*level*/) {}

extern "C" {

// Einstiegspunkt, den game/bin/uhrwerk.gdextension als entry_symbol nennt.
GDExtensionBool GDE_EXPORT uhrwerk_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                                GDExtensionClassLibraryPtr library,
                                                GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_uhrwerk_module);
    init.register_terminator(uninitialize_uhrwerk_module);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}

} // extern "C"
