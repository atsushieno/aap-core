
#include "presets-aapxs.h"
#include "parameters-aapxs.h"
#include "state-aapxs.h"

aap::xs::AAPXSDefinition_Parameters parameters;
aap::xs::AAPXSDefinition_Presets presets;
aap::xs::AAPXSDefinition_State state;

aap::xs::AAPXSDefinitionRegistry::AAPXSDefinitionRegistry() {}

aap::xs::AAPXSDefinitionRegistry::AAPXSDefinitionRegistry(std::vector<AAPXSDefinition> items) {
    std::function<void(AAPXSDefinition)> add = [&](AAPXSDefinition d) { this->add(d, d.uri); };

    for (auto item : items)
        add(item);
}

aap::xs::AAPXSDefinitionRegistry standard_extensions{std::vector<AAPXSDefinition>({
    parameters.asPublic(),
    presets.asPublic(),
    state.asPublic()
})};

aap::xs::AAPXSDefinitionRegistry *aap::xs::AAPXSDefinitionRegistry::getStandardExtensions() {
    return &standard_extensions;
}
