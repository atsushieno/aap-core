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

  ::ndk::ScopedAStatus notify(int32_t in_instanceId, int32_t in_portId, int32_t in_size) override {
    return _impl->notify(in_instanceId, in_portId, in_size);
  }
protected:
private:
  std::shared_ptr<IAudioPluginInterfaceCallback> _impl;
};

}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
