#pragma once

#include <android/binder_interface_utils.h>

#include <android/binder_parcel_utils.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
class IAudioPluginInterface : public ::ndk::ICInterface {
public:
  static const char* descriptor;
  IAudioPluginInterface();
  virtual ~IAudioPluginInterface();



  static std::shared_ptr<IAudioPluginInterface> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginInterface>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginInterface>* instance);
  static bool setDefaultImpl(std::shared_ptr<IAudioPluginInterface> impl);
  static const std::shared_ptr<IAudioPluginInterface>& getDefaultImpl();
  virtual ::ndk::ScopedAStatus create(const std::string& in_pluginId, int32_t in_sampleRate) = 0;
  virtual ::ndk::ScopedAStatus isPluginAlive(bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_bufferCount, const std::vector<int64_t>& in_bufferPointers) = 0;
  virtual ::ndk::ScopedAStatus activate() = 0;
  virtual ::ndk::ScopedAStatus process(int32_t in_timeoutInNanoseconds) = 0;
  virtual ::ndk::ScopedAStatus deactivate() = 0;
  virtual ::ndk::ScopedAStatus getStateSize(int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getState(int64_t in_pointer) = 0;
  virtual ::ndk::ScopedAStatus setState(int64_t in_pointer, int32_t in_size) = 0;
  virtual ::ndk::ScopedAStatus destroy() = 0;
private:
  static std::shared_ptr<IAudioPluginInterface> default_impl;
};
class IAudioPluginInterfaceDefault : public IAudioPluginInterface {
public:
  ::ndk::ScopedAStatus create(const std::string& in_pluginId, int32_t in_sampleRate) override;
  ::ndk::ScopedAStatus isPluginAlive(bool* _aidl_return) override;
  ::ndk::ScopedAStatus prepare(int32_t in_frameCount, int32_t in_bufferCount, const std::vector<int64_t>& in_bufferPointers) override;
  ::ndk::ScopedAStatus activate() override;
  ::ndk::ScopedAStatus process(int32_t in_timeoutInNanoseconds) override;
  ::ndk::ScopedAStatus deactivate() override;
  ::ndk::ScopedAStatus getStateSize(int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getState(int64_t in_pointer) override;
  ::ndk::ScopedAStatus setState(int64_t in_pointer, int32_t in_size) override;
  ::ndk::ScopedAStatus destroy() override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
