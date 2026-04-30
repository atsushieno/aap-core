/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginExtensionCallback.aidl
 */
#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginExtensionCallback.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
class BpAudioPluginExtensionCallback : public ::ndk::BpCInterface<IAudioPluginExtensionCallback> {
public:
  explicit BpAudioPluginExtensionCallback(const ::ndk::SpAIBinder& binder);
  virtual ~BpAudioPluginExtensionCallback();

  ::ndk::ScopedAStatus completed(int32_t in_instanceId, int32_t in_requestId, const std::string& in_errorMessage) override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
