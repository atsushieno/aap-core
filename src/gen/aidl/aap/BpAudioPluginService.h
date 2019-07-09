#pragma once

#include "aidl/aap/IAudioPluginService.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace aap {
class BpAudioPluginService : public ::ndk::BpCInterface<IAudioPluginService> {
public:
  BpAudioPluginService(const ::ndk::SpAIBinder& binder);
  virtual ~BpAudioPluginService();

  ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_bufferCount, const std::vector<int64_t>& in_bufferPointers) override;
  ::ndk::ScopedAStatus activate() override;
  ::ndk::ScopedAStatus process() override;
  ::ndk::ScopedAStatus deactivate() override;
  ::ndk::ScopedAStatus getStateSize(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getState(int64_t in_pointer) override;
  ::ndk::ScopedAStatus setState(int64_t in_pointer, int32_t in_size) override;
};
}  // namespace aap
}  // namespace aidl
