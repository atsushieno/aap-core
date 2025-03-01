/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl
 */
#pragma once

#include "aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.h"

#include <android/binder_ibinder.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
class BpAudioPluginInterfaceCallback : public ::ndk::BpCInterface<IAudioPluginInterfaceCallback> {
public:
  explicit BpAudioPluginInterfaceCallback(const ::ndk::SpAIBinder& binder);
  virtual ~BpAudioPluginInterfaceCallback();

  ::ndk::ScopedAStatus hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) override;
  ::ndk::ScopedAStatus requestProcess(int32_t in_instanceId) override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
