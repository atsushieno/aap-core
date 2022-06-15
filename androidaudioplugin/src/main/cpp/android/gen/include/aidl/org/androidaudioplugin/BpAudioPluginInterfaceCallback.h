#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
class BpAudioPluginInterfaceCallback : public ::ndk::BpCInterface<IAudioPluginInterfaceCallback> {
public:
  explicit BpAudioPluginInterfaceCallback(const ::ndk::SpAIBinder& binder);
  virtual ~BpAudioPluginInterfaceCallback();

  ::ndk::ScopedAStatus notify(int32_t in_instanceId, int32_t in_portId, int32_t in_size) override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl