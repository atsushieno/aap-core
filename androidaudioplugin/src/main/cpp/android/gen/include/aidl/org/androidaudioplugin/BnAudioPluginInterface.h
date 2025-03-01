/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl
 */
#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginInterface.h"

#include <android/binder_ibinder.h>
#include <cassert>

#ifndef __BIONIC__
#ifndef __assert2
#define __assert2(a,b,c,d) ((void)0)
#endif
#endif

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
class IAudioPluginInterfaceDelegator : public BnAudioPluginInterface {
public:
  explicit IAudioPluginInterfaceDelegator(const std::shared_ptr<IAudioPluginInterface> &impl) : _impl(impl) {
  }

  ::ndk::ScopedAStatus setCallback(const std::shared_ptr<::aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback>& in_callback) override {
    return _impl->setCallback(in_callback);
  }
  ::ndk::ScopedAStatus beginCreate(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) override {
    return _impl->beginCreate(in_pluginId, in_sampleRate, _aidl_return);
  }
  ::ndk::ScopedAStatus addExtension(int32_t in_instanceID, const std::string& in_uri, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override {
    return _impl->addExtension(in_instanceID, in_uri, in_sharedMemoryFD, in_size);
  }
  ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) override {
    return _impl->endCreate(in_instanceID);
  }
  ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) override {
    return _impl->isPluginAlive(in_instanceID, _aidl_return);
  }
  ::ndk::ScopedAStatus extension(int32_t in_instanceID, const std::string& in_uri, int32_t in_opcode) override {
    return _impl->extension(in_instanceID, in_uri, in_opcode);
  }
  ::ndk::ScopedAStatus beginPrepare(int32_t in_instanceID) override {
    return _impl->beginPrepare(in_instanceID);
  }
  ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override {
    return _impl->prepareMemory(in_instanceID, in_shmFDIndex, in_sharedMemoryFD);
  }
  ::ndk::ScopedAStatus endPrepare(int32_t in_instanceID, int32_t in_frameCount) override {
    return _impl->endPrepare(in_instanceID, in_frameCount);
  }
  ::ndk::ScopedAStatus activate(int32_t in_instanceID) override {
    return _impl->activate(in_instanceID);
  }
  ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_frameCount, int32_t in_timeoutInNanoseconds) override {
    return _impl->process(in_instanceID, in_frameCount, in_timeoutInNanoseconds);
  }
  ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) override {
    return _impl->deactivate(in_instanceID);
  }
  ::ndk::ScopedAStatus destroy(int32_t in_instanceID) override {
    return _impl->destroy(in_instanceID);
  }
protected:
private:
  std::shared_ptr<IAudioPluginInterface> _impl;
};

}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
