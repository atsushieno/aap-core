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
  static constexpr uint32_t TRANSACTION_beginCreate = FIRST_CALL_TRANSACTION + 0;
  static constexpr uint32_t TRANSACTION_addExtension = FIRST_CALL_TRANSACTION + 1;
  static constexpr uint32_t TRANSACTION_endCreate = FIRST_CALL_TRANSACTION + 2;
  static constexpr uint32_t TRANSACTION_isPluginAlive = FIRST_CALL_TRANSACTION + 3;
  static constexpr uint32_t TRANSACTION_getStateSize = FIRST_CALL_TRANSACTION + 4;
  static constexpr uint32_t TRANSACTION_getState = FIRST_CALL_TRANSACTION + 5;
  static constexpr uint32_t TRANSACTION_setState = FIRST_CALL_TRANSACTION + 6;
  static constexpr uint32_t TRANSACTION_extension = FIRST_CALL_TRANSACTION + 7;
  static constexpr uint32_t TRANSACTION_prepare = FIRST_CALL_TRANSACTION + 8;
  static constexpr uint32_t TRANSACTION_prepareMemory = FIRST_CALL_TRANSACTION + 9;
  static constexpr uint32_t TRANSACTION_activate = FIRST_CALL_TRANSACTION + 10;
  static constexpr uint32_t TRANSACTION_process = FIRST_CALL_TRANSACTION + 11;
  static constexpr uint32_t TRANSACTION_deactivate = FIRST_CALL_TRANSACTION + 12;
  static constexpr uint32_t TRANSACTION_destroy = FIRST_CALL_TRANSACTION + 13;

  static std::shared_ptr<IAudioPluginInterface> fromBinder(const ::ndk::SpAIBinder& binder);
  static binder_status_t writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginInterface>& instance);
  static binder_status_t readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginInterface>* instance);
  static bool setDefaultImpl(const std::shared_ptr<IAudioPluginInterface>& impl);
  static const std::shared_ptr<IAudioPluginInterface>& getDefaultImpl();
  virtual ::ndk::ScopedAStatus beginCreate(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus addExtension(int32_t in_instanceID, const std::string& in_uri, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) = 0;
  virtual ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) = 0;
  virtual ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getStateSize(int32_t in_instanceID, int32_t* _aidl_return) = 0;
  virtual ::ndk::ScopedAStatus getState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) = 0;
  virtual ::ndk::ScopedAStatus setState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) = 0;
  virtual ::ndk::ScopedAStatus extension(int32_t in_instanceID, const std::string& in_uri, int32_t in_size) = 0;
  virtual ::ndk::ScopedAStatus prepare(int32_t in_instanceID, int32_t in_frameCount, int32_t in_portCount) = 0;
  virtual ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) = 0;
  virtual ::ndk::ScopedAStatus activate(int32_t in_instanceID) = 0;
  virtual ::ndk::ScopedAStatus process(int32_t in_instanceID, int32_t in_timeoutInNanoseconds) = 0;
  virtual ::ndk::ScopedAStatus deactivate(int32_t in_instanceID) = 0;
  virtual ::ndk::ScopedAStatus destroy(int32_t in_instanceID) = 0;
private:
  static std::shared_ptr<IAudioPluginInterface> default_impl;
};
class IAudioPluginInterfaceDefault : public IAudioPluginInterface {
public:
  ::ndk::ScopedAStatus beginCreate(const std::string& in_pluginId, int32_t in_sampleRate, int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus addExtension(int32_t in_instanceID, const std::string& in_uri, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override;
  ::ndk::ScopedAStatus endCreate(int32_t in_instanceID) override;
  ::ndk::ScopedAStatus isPluginAlive(int32_t in_instanceID, bool* _aidl_return) override;
  ::ndk::ScopedAStatus getStateSize(int32_t in_instanceID, int32_t* _aidl_return) override;
  ::ndk::ScopedAStatus getState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override;
  ::ndk::ScopedAStatus setState(int32_t in_instanceID, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD, int32_t in_size) override;
  ::ndk::ScopedAStatus extension(int32_t in_instanceID, const std::string& in_uri, int32_t in_size) override;
  ::ndk::ScopedAStatus prepare(int32_t in_instanceID, int32_t in_frameCount, int32_t in_portCount) override;
  ::ndk::ScopedAStatus prepareMemory(int32_t in_instanceID, int32_t in_shmFDIndex, const ::ndk::ScopedFileDescriptor& in_sharedMemoryFD) override;
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
