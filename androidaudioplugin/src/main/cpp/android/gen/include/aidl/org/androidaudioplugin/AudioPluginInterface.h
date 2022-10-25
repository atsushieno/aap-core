#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_interface_utils.h>
#include <aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace org {
namespace androidaudioplugin {
class IAudioPluginInterface : public ::ndk::ICInterface {
public:
  static const char* descriptor;
  IAudioPluginInterface();
  virtual ~IAudioPluginInterface();

  enum : int32_t { AAP_BINDER_ERROR_UNEXPECTED_INSTANCE_ID = 1 };
  enum : int32_t { AAP_BINDER_ERROR_CREATE_INSTANCE_FAILED = 2 };
  enum : int32_t { AAP_BINDER_ERROR_SHARED_MEMORY_EXTENSION = 10 };
  enum : int32_t { AAP_BINDER_ERROR_MMAP_FAILED = 11 };
  enum : int32_t { AAP_BINDER_ERROR_MMAP_NULL_RETURN = 12 };
  enum : int32_t { AAP_BINDER_ERROR_INVALID_SHARED_MEMORY_FD = 20 };
  enum : int32_t { AAP_BINDER_ERROR_CALLBACK_ALREADY_SET = 30 };
  static constexpr uint32_t TRANSACTION_setCallback = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_beginCreate = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_addExtension = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_endCreate = FIRST_CALL_TRANSACTION + 3;
  static constexpr uint32_t TRANSACTION_isPluginAlive = FIRST_CALL_TRANSACTION + 4;
  static constexpr uint32_t TRANSACTION_extension = FIRST_CALL_TRANSACTION + 5;
  static constexpr uint32_t TRANSACTION_beginPrepare = FIRST_CALL_TRANSACTION + 6;
  static constexpr uint32_t TRANSACTION_prepareMemory = FIRST_CALL_TRANSACTION + 7;
  static constexpr uint32_t TRANSACTION_endPrepare = FIRST_CALL_TRANSACTION + 8;
  static constexpr uint32_t TRANSACTION_activate = FIRST_CALL_TRANSACTION + 9;
  static constexpr uint32_t TRANSACTION_process = FIRST_CALL_TRANSACTION + 10;
  static constexpr uint32_t TRANSACTION_deactivate = FIRST_CALL_TRANSACTION + 11;
  static constexpr uint32_t TRANSACTION_destroy = FIRST_CALL_TRANSACTION + 12;

  static std::shared_ptr<IAudioPluginInterface> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginInterface>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginInterface>* instance);
  static bool setDefaultImpl(const std::shared_ptr<IAudioPluginInterface>& impl);
  static const std::shared_ptr<IAudioPluginInterface>& getDefaultImpl();
  virtual ::ndk::ScopedAStatus setCallback(const std::shared_ptr<::aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback>& in_callback) = 0;
  virtual ::ndk::ScopedAStatus beginCreate(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus addExtension(int32_t in_instanceID, const std::string& in_uri, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) = 0;
  virtual ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) = 0;
  virtual ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus extension(int32_t in_instanceID, const std::string& in_uri, int32_t in_size) = 0;
  virtual ::ndk::ScopedAStatus beginPrepare(int32_t in_instanceID) = 0;
  virtual ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) = 0;
  virtual ::ndk::ScopedAStatus endPrepare(int32_t in_instanceID, int32_t in_frameCount) = 0;
  virtual ::ndk::ScopedAStatus activate(int32_t in_instanceID) = 0;
  virtual ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_timeoutInNanoseconds) = 0;
  virtual ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) = 0;
  virtual ::ndk::ScopedAStatus destroy(int32_t in_instanceID) = 0;
private:
  static std::shared_ptr<IAudioPluginInterface> default_impl;
};
class IAudioPluginInterfaceDefault : public IAudioPluginInterface {
public:
  ::ndk::ScopedAStatus setCallback(const std::shared_ptr<::aidl::org::androidaudioplugin::IAudioPluginInterfaceCallback>& in_callback) override;
  ::ndk::ScopedAStatus beginCreate(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus addExtension(int32_t in_instanceID, const std::string& in_uri, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override;
  ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) override;
  ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) override;
  ::ndk::ScopedAStatus extension(int32_t in_instanceID, const std::string& in_uri, int32_t in_size) override;
  ::ndk::ScopedAStatus beginPrepare(int32_t in_instanceID) override;
  ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override;
  ::ndk::ScopedAStatus endPrepare(int32_t in_instanceID, int32_t in_frameCount) override;
  ::ndk::ScopedAStatus activate(int32_t in_instanceID) override;
  ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_timeoutInNanoseconds) override;
  ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) override;
  ::ndk::ScopedAStatus destroy(int32_t in_instanceID) override;
  ::ndk::SpAIBinder asBinder() override;
  bool isRemote() override;
};
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
