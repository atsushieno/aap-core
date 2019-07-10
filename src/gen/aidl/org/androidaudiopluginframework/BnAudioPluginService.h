#pragma once

#include "aidl/org/androidaudiopluginframework/AudioPluginService.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudiopluginframework {
class BnAudioPluginService : public ::ndk::BnCInterface<IAudioPluginService> {
public:
  BnAudioPluginService();
  virtual ~BnAudioPluginService();
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
}  // namespace androidaudiopluginframework
}  // namespace org
}  // namespace aidl
