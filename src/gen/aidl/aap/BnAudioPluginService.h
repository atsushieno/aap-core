#pragma once

#include "aidl/aap/IAudioPluginService.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace aap {
class BnAudioPluginService : public ::ndk::BnCInterface<IAudioPluginService> {
public:
  BnAudioPluginService();
  virtual ~BnAudioPluginService();
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
}  // namespace aap
}  // namespace aidl
