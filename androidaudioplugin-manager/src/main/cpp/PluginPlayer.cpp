
#include "PluginPlayer.h"

void aap::PluginPlayer::start() {
    assert(instance != nullptr);
    instance->activate();

    assert(false); // TODO
}
