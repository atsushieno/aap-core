/*
  ==============================================================================

    AndroidAudioUnit.cpp
    Created: 9 May 2019 3:09:22am
    Author:  atsushieno

  ==============================================================================
*/

#include "AndroidAudioUnit.h"
#include "android/asset_manager.h"


namespace aap
{

AndroidAudioPlugin* AndroidAudioPluginManager::instantiatePlugin(AndroidAudioPluginDescriptor *descriptor)
{
	return new AndroidAudioPlugin(descriptor);
}

} // namespace
