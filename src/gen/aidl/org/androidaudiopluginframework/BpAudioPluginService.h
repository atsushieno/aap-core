#pragma once

#include "aidl/org/androidaudiopluginframework/AudioPluginService.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudiopluginframework {
class BpAudioPluginService : public ::ndk::BpCInterface<IAudioPluginService> {
public:
  BpAudioPluginService(const ::ndk::SpAIBinder& binder);
  virtual ~BpAudioPluginService();

  ::ndk::ScopedAStatus create(const std::string& in_pluginId, int32_t in_sampleRate) override;
  ::ndk::ScopedAStatus isPluginAlive(bool* _aidl_return) override;
  ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_bufferCount, const std::vector<int64_t>& in_bufferPointers) override;
  ::ndk::ScopedAStatus activate() override;
  ::ndk::ScopedAStatus process(int32_t in_timeoutInNanoseconds) override;
  ::ndk::ScopedAStatus deactivate() override;
  ::ndk::ScopedAStatus getStateSize(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getState(int64_t in_pointer) override;
  ::ndk::ScopedAStatus setState(int64_t in_pointer, int32_t in_size) override;
  ::ndk::ScopedAStatus destroy() override;
};
}  // namespace androidaudiopluginframework
}  // namespace org
}  // namespace aidl
