#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginInterface.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
class BpAudioPluginInterface : public ::ndk::BpCInterface<IAudioPluginInterface> {
public:
  BpAudioPluginInterface(const ::ndk::SpAIBinder& binder);
  virtual ~BpAudioPluginInterface();

  ::ndk::ScopedAStatus create(const std::string& in_pluginId, int32_t in_sampleRate) override;
  ::ndk::ScopedAStatus isPluginAlive(bool* _aidl_return) override;
  ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_portCount) override;
  ::ndk::ScopedAStatus prepareMemory(int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override;
  ::ndk::ScopedAStatus activate() override;
  ::ndk::ScopedAStatus process(int32_t in_timeoutInNanoseconds) override;
  ::ndk::ScopedAStatus deactivate() override;
  ::ndk::ScopedAStatus getStateSize(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getState(const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override;
  ::ndk::ScopedAStatus setState(const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override;
  ::ndk::ScopedAStatus destroy() override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
