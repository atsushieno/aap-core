/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl
 */
#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.h"

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
class BnAudioPluginInterfaceCallback : public ::ndk::BnCInterface<IAudioPluginInterfaceCallback> {
public:
  BnAudioPluginInterfaceCallback();
  virtual ~BnAudioPluginInterfaceCallback();
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class IAudioPluginInterfaceCallbackDelegator : public BnAudioPluginInterfaceCallback {
public:
  explicit IAudioPluginInterfaceCallbackDelegator(const std::shared_ptr<IAudioPluginInterfaceCallback> &impl) : _impl(impl) {
  }

  ::ndk::ScopedAStatus hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) override {
    return _impl->hostExtension(in_instanceId, in_uri, in_opcode);
  }
  ::ndk::ScopedAStatus requestProcess(int32_t in_instanceId) override {
    return _impl->requestProcess(in_instanceId);
  }
protected:
private:
  std::shared_ptr<IAudioPluginInterfaceCallback> _impl;
};

}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
