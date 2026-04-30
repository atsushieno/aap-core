/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginExtensionCallback.aidl
 */
#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginExtensionCallback.h"

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
class BnAudioPluginExtensionCallback : public ::ndk::BnCInterface<IAudioPluginExtensionCallback> {
public:
  BnAudioPluginExtensionCallback();
  virtual ~BnAudioPluginExtensionCallback();
protected:
  ::ndk::SpAIBinder createBinder() override;
private:
};
class IAudioPluginExtensionCallbackDelegator : public BnAudioPluginExtensionCallback {
public:
  explicit IAudioPluginExtensionCallbackDelegator(const std::shared_ptr<IAudioPluginExtensionCallback> &impl) : _impl(impl) {
  }

  ::ndk::ScopedAStatus completed(int32_t in_instanceId, int32_t in_requestId, const std::string& in_errorMessage) override {
    return _impl->completed(in_instanceId, in_requestId, in_errorMessage);
  }
protected:
private:
  std::shared_ptr<IAudioPluginExtensionCallback> _impl;
};

}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
