#pragma once

#include "aidl/org/androidaudiopluginframework/AudioPluginInterface.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudiopluginframework {
class BnAudioPluginInterface : public ::ndk::BnCInterface<IAudioPluginInterface> {
public:
  BnAudioPluginInterface();
  virtual ~BnAudioPluginInterface();
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
}  // namespace androidaudiopluginframework
}  // namespace org
}  // namespace aidl
