/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl
 */
#include "aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.h"

#include <android/binder_parcel_utils.h>
#include <aidl/org/androidaudioplugin/BnAudioPluginInterfaceCallback.h>
#include <aidl/org/androidaudioplugin/BpAudioPluginInterfaceCallback.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
static binder_status_t _aidl_org_androidaudioplugin_AudioPluginInterfaceCallback_onTransact(AIBinder* _aidl_binder, transaction_code_t _aidl_code, const AParcel* _aidl_in, AParcel* _aidl_out) {
  (void)_aidl_in;
  (void)_aidl_out;
  binder_status_t _aidl_ret_status = STATUS_UNKNOWN_TRANSACTION;
  std::shared_ptr<BnAudioPluginInterfaceCallback> _aidl_impl = std::static_pointer_cast<BnAudioPluginInterfaceCallback>(::ndk::ICInterface::asInterface(_aidl_binder));
  switch (_aidl_code) {
    case (FIRST_CALL_TRANSACTION + 0 /*hostExtension*/): {
      int32_t in_instanceId;
      std::string in_uri;
      int32_t in_opcode;

      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_instanceId);
      if (_aidl_ret_status != STATUS_OK) break;

      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_uri);
      if (_aidl_ret_status != STATUS_OK) break;

      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_opcode);
      if (_aidl_ret_status != STATUS_OK) break;

      ::ndk::ScopedAStatus _aidl_status = _aidl_impl->hostExtension(in_instanceId, in_uri, in_opcode);
      _aidl_ret_status = STATUS_OK;
      break;
    }
    case (FIRST_CALL_TRANSACTION + 1 /*requestProcess*/): {
      int32_t in_instanceId;

      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_instanceId);
      if (_aidl_ret_status != STATUS_OK) break;

      ::ndk::ScopedAStatus _aidl_status = _aidl_impl->requestProcess(in_instanceId);
      _aidl_ret_status = STATUS_OK;
      break;
    }
  }
  return _aidl_ret_status;
}

static AIBinder_Class* _g_aidl_org_androidaudioplugin_AudioPluginInterfaceCallback_clazz = ::ndk::ICInterface::defineClass(IAudioPluginInterfaceCallback::descriptor, _aidl_org_androidaudioplugin_AudioPluginInterfaceCallback_onTransact);

BpAudioPluginInterfaceCallback::BpAudioPluginInterfaceCallback(const ::ndk::SpAIBinder& binder) : BpCInterface(binder) {}
BpAudioPluginInterfaceCallback::~BpAudioPluginInterfaceCallback() {}

::ndk::ScopedAStatus BpAudioPluginInterfaceCallback::hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) {
  binder_status_t _aidl_ret_status = STATUS_OK;
  ::ndk::ScopedAStatus _aidl_status;
  ::ndk::ScopedAParcel _aidl_in;
  ::ndk::ScopedAParcel _aidl_out;

  _aidl_ret_status = AIBinder_prepareTransaction(asBinder().get(), _aidl_in.getR());
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_in.get(), in_instanceId);
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_in.get(), in_uri);
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_in.get(), in_opcode);
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = AIBinder_transact(
    asBinder().get(),
    (FIRST_CALL_TRANSACTION + 0 /*hostExtension*/),
    _aidl_in.getR(),
    _aidl_out.getR(),
    FLAG_ONEWAY
    #ifdef BINDER_STABILITY_SUPPORT
    | FLAG_PRIVATE_LOCAL
    #endif  // BINDER_STABILITY_SUPPORT
    );
  if (_aidl_ret_status == STATUS_UNKNOWN_TRANSACTION && IAudioPluginInterfaceCallback::getDefaultImpl()) {
    _aidl_status = IAudioPluginInterfaceCallback::getDefaultImpl()->hostExtension(in_instanceId, in_uri, in_opcode);
    goto _aidl_status_return;
  }
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_error:
  _aidl_status.set(AStatus_fromStatus(_aidl_ret_status));
  _aidl_status_return:
  return _aidl_status;
}
::ndk::ScopedAStatus BpAudioPluginInterfaceCallback::requestProcess(int32_t in_instanceId) {
  binder_status_t _aidl_ret_status = STATUS_OK;
  ::ndk::ScopedAStatus _aidl_status;
  ::ndk::ScopedAParcel _aidl_in;
  ::ndk::ScopedAParcel _aidl_out;

  _aidl_ret_status = AIBinder_prepareTransaction(asBinder().get(), _aidl_in.getR());
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_in.get(), in_instanceId);
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = AIBinder_transact(
    asBinder().get(),
    (FIRST_CALL_TRANSACTION + 1 /*requestProcess*/),
    _aidl_in.getR(),
    _aidl_out.getR(),
    FLAG_ONEWAY
    #ifdef BINDER_STABILITY_SUPPORT
    | FLAG_PRIVATE_LOCAL
    #endif  // BINDER_STABILITY_SUPPORT
    );
  if (_aidl_ret_status == STATUS_UNKNOWN_TRANSACTION && IAudioPluginInterfaceCallback::getDefaultImpl()) {
    _aidl_status = IAudioPluginInterfaceCallback::getDefaultImpl()->requestProcess(in_instanceId);
    goto _aidl_status_return;
  }
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_error:
  _aidl_status.set(AStatus_fromStatus(_aidl_ret_status));
  _aidl_status_return:
  return _aidl_status;
}
// Source for BnAudioPluginInterfaceCallback
BnAudioPluginInterfaceCallback::BnAudioPluginInterfaceCallback() {}
BnAudioPluginInterfaceCallback::~BnAudioPluginInterfaceCallback() {}
::ndk::SpAIBinder BnAudioPluginInterfaceCallback::createBinder() {
  AIBinder* binder = AIBinder_new(_g_aidl_org_androidaudioplugin_AudioPluginInterfaceCallback_clazz, static_cast<void*>(this));
  #ifdef BINDER_STABILITY_SUPPORT
  AIBinder_markCompilationUnitStability(binder);
  #endif  // BINDER_STABILITY_SUPPORT
  return ::ndk::SpAIBinder(binder);
}
// Source for IAudioPluginInterfaceCallback
const char* IAudioPluginInterfaceCallback::descriptor = "org.androidaudioplugin.AudioPluginInterfaceCallback";
IAudioPluginInterfaceCallback::IAudioPluginInterfaceCallback() {}
IAudioPluginInterfaceCallback::~IAudioPluginInterfaceCallback() {}


std::shared_ptr<IAudioPluginInterfaceCallback> IAudioPluginInterfaceCallback::fromBinder(const ::ndk::SpAIBinder& binder) {
  if (!AIBinder_associateClass(binder.get(), _g_aidl_org_androidaudioplugin_AudioPluginInterfaceCallback_clazz)) {
    #if __ANDROID_API__ >= 31
    const AIBinder_Class* originalClass = AIBinder_getClass(binder.get());
    if (originalClass == nullptr) return nullptr;
    if (0 == strcmp(AIBinder_Class_getDescriptor(originalClass), descriptor)) {
      return ::ndk::SharedRefBase::make<BpAudioPluginInterfaceCallback>(binder);
    }
    #endif
    return nullptr;
  }
  std::shared_ptr<::ndk::ICInterface> interface = ::ndk::ICInterface::asInterface(binder.get());
  if (interface) {
    return std::static_pointer_cast<IAudioPluginInterfaceCallback>(interface);
  }
  return ::ndk::SharedRefBase::make<BpAudioPluginInterfaceCallback>(binder);
}

binder_status_t IAudioPluginInterfaceCallback::writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginInterfaceCallback>& instance) {
  return AParcel_writeStrongBinder(parcel, instance ? instance->asBinder().get() : nullptr);
}
binder_status_t IAudioPluginInterfaceCallback::readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginInterfaceCallback>* instance) {
  ::ndk::SpAIBinder binder;
  binder_status_t status = AParcel_readStrongBinder(parcel, binder.getR());
  if (status != STATUS_OK) return status;
  *instance = IAudioPluginInterfaceCallback::fromBinder(binder);
  return STATUS_OK;
}
bool IAudioPluginInterfaceCallback::setDefaultImpl(const std::shared_ptr<IAudioPluginInterfaceCallback>& impl) {
  // Only one user of this interface can use this function
  // at a time. This is a heuristic to detect if two different
  // users in the same process use this function.
  assert(!IAudioPluginInterfaceCallback::default_impl);
  if (impl) {
    IAudioPluginInterfaceCallback::default_impl = impl;
    return true;
  }
  return false;
}
const std::shared_ptr<IAudioPluginInterfaceCallback>& IAudioPluginInterfaceCallback::getDefaultImpl() {
  return IAudioPluginInterfaceCallback::default_impl;
}
std::shared_ptr<IAudioPluginInterfaceCallback> IAudioPluginInterfaceCallback::default_impl = nullptr;
::ndk::ScopedAStatus IAudioPluginInterfaceCallbackDefault::hostExtension(int32_t /*in_instanceId*/, const std::string& /*in_uri*/, int32_t /*in_opcode*/) {
  ::ndk::ScopedAStatus _aidl_status;
  _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
  return _aidl_status;
}
::ndk::ScopedAStatus IAudioPluginInterfaceCallbackDefault::requestProcess(int32_t /*in_instanceId*/) {
  ::ndk::ScopedAStatus _aidl_status;
  _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
  return _aidl_status;
}
::ndk::SpAIBinder IAudioPluginInterfaceCallbackDefault::asBinder() {
  return ::ndk::SpAIBinder();
}
bool IAudioPluginInterfaceCallbackDefault::isRemote() {
  return false;
}
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
