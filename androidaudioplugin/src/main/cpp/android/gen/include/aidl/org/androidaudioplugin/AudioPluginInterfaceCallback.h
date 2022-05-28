#pragma once

#include <android/binder_interface_utils.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
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

  static constexpr uint32_t TRANSACTION_notify = FIRST_CALL_TRANSACTION + 0;

  static std::shared_ptr<IAudioPluginInterfaceCallback> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginInterfaceCallback>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginInterfaceCallback>* instance);
  static bool setDefaultImpl(const std::shared_ptr<IAudioPluginInterfaceCallback>& impl);
  static const std::shared_ptr<IAudioPluginInterfaceCallback>& getDefaultImpl();
  virtual ::ndk::ScopedAStatus notify(int32_t in_instanceId, int32_t in_portId, int32_t in_size) = 0;
private:
  static std::shared_ptr<IAudioPluginInterfaceCallback> default_impl;
};
class IAudioPluginInterfaceCallbackDefault : public IAudioPluginInterfaceCallback {
public:
  ::ndk::ScopedAStatus notify(int32_t in_instanceId, int32_t in_portId, int32_t in_size) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
