/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginExtensionCallback.aidl
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_interface_utils.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace org {
namespace androidaudioplugin {
class IAudioPluginExtensionCallbackDelegator;

class IAudioPluginExtensionCallback : public ::ndk::ICInterface {
public:
  typedef IAudioPluginExtensionCallbackDelegator DefaultDelegator;
  static const char* descriptor;
  IAudioPluginExtensionCallback();
  virtual ~IAudioPluginExtensionCallback();

  static constexpr uint32_t TRANSACTION_completed = FIRST_CALL_TRANSACTION + 0;

  static std::shared_ptr<IAudioPluginExtensionCallback> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginExtensionCallback>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginExtensionCallback>* instance);
  static bool setDefaultImpl(const std::shared_ptr<IAudioPluginExtensionCallback>& impl);
  static const std::shared_ptr<IAudioPluginExtensionCallback>& getDefaultImpl();
  virtual ::ndk::ScopedAStatus completed(int32_t in_instanceId, int32_t in_requestId, const std::string& in_errorMessage) = 0;
private:
  static std::shared_ptr<IAudioPluginExtensionCallback> default_impl;
};
class IAudioPluginExtensionCallbackDefault : public IAudioPluginExtensionCallback {
public:
  ::ndk::ScopedAStatus completed(int32_t in_instanceId, int32_t in_requestId, const std::string& in_errorMessage) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
