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
class IAudioPluginInterfaceCallback : public ::ndk::ICInterface {
public:
  static const char* descriptor;
  IAudioPluginInterfaceCallback();
  virtual ~IAudioPluginInterfaceCallback();

  static constexpr uint32_t TRANSACTION_hostExtension = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_requestProcess = FIRST_CALL_TRANSACTION + 1;

  static std::shared_ptr<IAudioPluginInterfaceCallback> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginInterfaceCallback>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginInterfaceCallback>* instance);
  static bool setDefaultImpl(const std::shared_ptr<IAudioPluginInterfaceCallback>& impl);
  static const std::shared_ptr<IAudioPluginInterfaceCallback>& getDefaultImpl();
  virtual ::ndk::ScopedAStatus hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) = 0;
  virtual ::ndk::ScopedAStatus requestProcess(int32_t in_instanceId) = 0;
private:
  static std::shared_ptr<IAudioPluginInterfaceCallback> default_impl;
};
class IAudioPluginInterfaceCallbackDefault : public IAudioPluginInterfaceCallback {
public:
  ::ndk::ScopedAStatus hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) override;
  ::ndk::ScopedAStatus requestProcess(int32_t in_instanceId) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
