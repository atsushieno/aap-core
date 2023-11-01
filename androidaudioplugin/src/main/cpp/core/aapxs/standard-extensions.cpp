
#include "presets-aapxs.h"
#include "parameters-aapxs.h"

aap::xs::AAPXSDefinition_Parameters parameters;
aap::xs::AAPXSDefinition_Presets presets;

aap::xs::AAPXSDefinitionRegistry::AAPXSDefinitionRegistry() {}

aap::xs::AAPXSDefinitionRegistry::AAPXSDefinitionRegistry(std::vector<AAPXSDefinition> items) {
    std::function<void(AAPXSDefinition)> add = [&](AAPXSDefinition d) { this->add(d, d.uri); };

    for (auto item : items)
        add(item);
}

aap::xs::AAPXSDefinitionRegistry standard_extensions{std::vector<AAPXSDefinition>({
    parameters.asPublic(),
    presets.asPublic()
})};

aap::xs::AAPXSDefinitionRegistry *aap::xs::AAPXSDefinitionRegistry::getStandardExtensions() {
    return &standard_extensions;
}
