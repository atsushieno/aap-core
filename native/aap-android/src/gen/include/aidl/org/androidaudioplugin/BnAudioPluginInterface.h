#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginInterface.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
class BnAudioPluginInterface : public ::ndk::BnCInterface<IAudioPluginInterface> {
public:
  BnAudioPluginInterface();
  virtual ~BnAudioPluginInterface();
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
