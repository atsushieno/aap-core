
#include "presets-aapxs.h"

aap::xs::AAPXSDefinition_Presets presets;

aap::xs::AAPXSDefinitionRegistry::AAPXSDefinitionRegistry() {
    std::function<void(AAPXSDefinition&)> add = [&](AAPXSDefinition& d) { this->add(d, d.uri); };

    add(presets.asPublic());
}

aap::xs::AAPXSDefinitionRegistry standard_extensions{};

aap::xs::AAPXSDefinitionRegistry *aap::xs::AAPXSDefinitionRegistry::getStandardExtensions() {
    return &standard_extensions;
}
