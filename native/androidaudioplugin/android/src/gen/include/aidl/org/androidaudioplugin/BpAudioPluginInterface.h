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

  ::ndk::ScopedAStatus create(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) override;
  ::ndk::ScopedAStatus prepare(int32_t in_instanceID, int32_t in_frameCount, int32_t in_portCount) override;
  ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override;
  ::ndk::ScopedAStatus activate(int32_t in_instanceID) override;
  ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_timeoutInNanoseconds) override;
  ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) override;
  ::ndk::ScopedAStatus getStateSize(int32_t in_instanceID, int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override;
  ::ndk::ScopedAStatus setState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override;
  ::ndk::ScopedAStatus destroy(int32_t in_instanceID) override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
