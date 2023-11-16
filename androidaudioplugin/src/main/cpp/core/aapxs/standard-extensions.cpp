
#include "aap/core/aapxs/standard-extensions.h"

aap::xs::AAPXSDefinition_Midi midi;
aap::xs::AAPXSDefinition_Parameters parameters;
aap::xs::AAPXSDefinition_Presets presets;
aap::xs::AAPXSDefinition_State state;
aap::xs::AAPXSDefinition_Gui gui;
aap::xs::AAPXSDefinition_Urid urid;

aap::xs::AAPXSDefinitionRegistry::AAPXSDefinitionRegistry(
        std::unique_ptr<UridMapping> mapping,
        std::vector<AAPXSDefinition> items)
        : AAPXSUridMapping(mapping.get()), mapping(std::move(mapping)) {
    std::function<void(AAPXSDefinition)> add = [&](AAPXSDefinition d) { this->add(d, d.uri); };

    for (auto item : items)
        add(item);
}

aap::xs::AAPXSDefinitionRegistry standard_extensions{std::make_unique<aap::xs::UridMapping>(), std::vector<AAPXSDefinition>({
    urid.asPublic(),
    midi.asPublic(),
    parameters.asPublic(),
    presets.asPublic(),
    state.asPublic(),
    gui.asPublic()
})};

aap::xs::AAPXSDefinitionRegistry *aap::xs::AAPXSDefinitionRegistry::getStandardExtensions() {
    return &standard_extensions;
}
