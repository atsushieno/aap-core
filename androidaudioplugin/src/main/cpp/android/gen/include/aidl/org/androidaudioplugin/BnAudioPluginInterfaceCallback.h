#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
class BnAudioPluginInterfaceCallback : public ::ndk::BnCInterface<IAudioPluginInterfaceCallback> {
public:
  BnAudioPluginInterfaceCallback();
  virtual ~BnAudioPluginInterfaceCallback();
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
