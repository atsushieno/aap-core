/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: /Users/atsushi/Library/Android/sdk/build-tools/35.0.1/aidl --lang=ndk -o /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen -h /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/cpp/android/gen/include -I /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/ /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterface.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginInterfaceCallback.aidl /Users/atsushi/sources/AAP/aap-core/androidaudioplugin/src/main/aidl/org/androidaudioplugin/AudioPluginExtensionCallback.aidl
 */
#include "aidl/org/androidaudioplugin/AudioPluginExtensionCallback.h"

#include <android/binder_parcel_utils.h>
#include <aidl/org/androidaudioplugin/BnAudioPluginExtensionCallback.h>
#include <aidl/org/androidaudioplugin/BpAudioPluginExtensionCallback.h>

namespace aidl {
namespace org {
namespace androidaudioplugin {
static binder_status_t _aidl_org_androidaudioplugin_AudioPluginExtensionCallback_onTransact(AIBinder* _aidl_binder, transaction_code_t _aidl_code, const AParcel* _aidl_in, AParcel* _aidl_out) {
  (void)_aidl_in;
  (void)_aidl_out;
  binder_status_t _aidl_ret_status = STATUS_UNKNOWN_TRANSACTION;
  std::shared_ptr<BnAudioPluginExtensionCallback> _aidl_impl = std::static_pointer_cast<BnAudioPluginExtensionCallback>(::ndk::ICInterface::asInterface(_aidl_binder));
  switch (_aidl_code) {
    case (FIRST_CALL_TRANSACTION + 0 /*completed*/): {
      int32_t in_instanceId;
      int32_t in_requestId;
      std::string in_errorMessage;

      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_instanceId);
      if (_aidl_ret_status != STATUS_OK) break;

      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_requestId);
      if (_aidl_ret_status != STATUS_OK) break;

      _aidl_ret_status = ::ndk::AParcel_readData(_aidl_in, &in_errorMessage);
      if (_aidl_ret_status != STATUS_OK) break;

      ::ndk::ScopedAStatus _aidl_status = _aidl_impl->completed(in_instanceId, in_requestId, in_errorMessage);
      _aidl_ret_status = STATUS_OK;
      break;
    }
  }
  return _aidl_ret_status;
}

static AIBinder_Class* _g_aidl_org_androidaudioplugin_AudioPluginExtensionCallback_clazz = ::ndk::ICInterface::defineClass(IAudioPluginExtensionCallback::descriptor, _aidl_org_androidaudioplugin_AudioPluginExtensionCallback_onTransact);

BpAudioPluginExtensionCallback::BpAudioPluginExtensionCallback(const ::ndk::SpAIBinder& binder) : BpCInterface(binder) {}
BpAudioPluginExtensionCallback::~BpAudioPluginExtensionCallback() {}

::ndk::ScopedAStatus BpAudioPluginExtensionCallback::completed(int32_t in_instanceId, int32_t in_requestId, const std::string& in_errorMessage) {
  binder_status_t _aidl_ret_status = STATUS_OK;
  ::ndk::ScopedAStatus _aidl_status;
  ::ndk::ScopedAParcel _aidl_in;
  ::ndk::ScopedAParcel _aidl_out;

  _aidl_ret_status = AIBinder_prepareTransaction(asBinder().get(), _aidl_in.getR());
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_in.get(), in_instanceId);
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_in.get(), in_requestId);
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = ::ndk::AParcel_writeData(_aidl_in.get(), in_errorMessage);
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_ret_status = AIBinder_transact(
    asBinder().get(),
    (FIRST_CALL_TRANSACTION + 0 /*completed*/),
    _aidl_in.getR(),
    _aidl_out.getR(),
    FLAG_ONEWAY
    #ifdef BINDER_STABILITY_SUPPORT
    | FLAG_PRIVATE_LOCAL
    #endif  // BINDER_STABILITY_SUPPORT
    );
  if (_aidl_ret_status == STATUS_UNKNOWN_TRANSACTION && IAudioPluginExtensionCallback::getDefaultImpl()) {
    _aidl_status = IAudioPluginExtensionCallback::getDefaultImpl()->completed(in_instanceId, in_requestId, in_errorMessage);
    goto _aidl_status_return;
  }
  if (_aidl_ret_status != STATUS_OK) goto _aidl_error;

  _aidl_error:
  _aidl_status.set(AStatus_fromStatus(_aidl_ret_status));
  _aidl_status_return:
  return _aidl_status;
}
// Source for BnAudioPluginExtensionCallback
BnAudioPluginExtensionCallback::BnAudioPluginExtensionCallback() {}
BnAudioPluginExtensionCallback::~BnAudioPluginExtensionCallback() {}
::ndk::SpAIBinder BnAudioPluginExtensionCallback::createBinder() {
  AIBinder* binder = AIBinder_new(_g_aidl_org_androidaudioplugin_AudioPluginExtensionCallback_clazz, static_cast<void*>(this));
  #ifdef BINDER_STABILITY_SUPPORT
  AIBinder_markCompilationUnitStability(binder);
  #endif  // BINDER_STABILITY_SUPPORT
  return ::ndk::SpAIBinder(binder);
}
// Source for IAudioPluginExtensionCallback
const char* IAudioPluginExtensionCallback::descriptor = "org.androidaudioplugin.AudioPluginExtensionCallback";
IAudioPluginExtensionCallback::IAudioPluginExtensionCallback() {}
IAudioPluginExtensionCallback::~IAudioPluginExtensionCallback() {}


std::shared_ptr<IAudioPluginExtensionCallback> IAudioPluginExtensionCallback::fromBinder(const ::ndk::SpAIBinder& binder) {
  if (!AIBinder_associateClass(binder.get(), _g_aidl_org_androidaudioplugin_AudioPluginExtensionCallback_clazz)) {
    #if __ANDROID_API__ >= 31
    const AIBinder_Class* originalClass = AIBinder_getClass(binder.get());
    if (originalClass == nullptr) return nullptr;
    if (0 == strcmp(AIBinder_Class_getDescriptor(originalClass), descriptor)) {
      return ::ndk::SharedRefBase::make<BpAudioPluginExtensionCallback>(binder);
    }
    #endif
    return nullptr;
  }
  std::shared_ptr<::ndk::ICInterface> interface = ::ndk::ICInterface::asInterface(binder.get());
  if (interface) {
    return std::static_pointer_cast<IAudioPluginExtensionCallback>(interface);
  }
  return ::ndk::SharedRefBase::make<BpAudioPluginExtensionCallback>(binder);
}

binder_status_t IAudioPluginExtensionCallback::writeToParcel(AParcel* parcel, const std::shared_ptr<IAudioPluginExtensionCallback>& instance) {
  return AParcel_writeStrongBinder(parcel, instance ? instance->asBinder().get() : nullptr);
}
binder_status_t IAudioPluginExtensionCallback::readFromParcel(const AParcel* parcel, std::shared_ptr<IAudioPluginExtensionCallback>* instance) {
  ::ndk::SpAIBinder binder;
  binder_status_t status = AParcel_readStrongBinder(parcel, binder.getR());
  if (status != STATUS_OK) return status;
  *instance = IAudioPluginExtensionCallback::fromBinder(binder);
  return STATUS_OK;
}
bool IAudioPluginExtensionCallback::setDefaultImpl(const std::shared_ptr<IAudioPluginExtensionCallback>& impl) {
  // Only one user of this interface can use this function
  // at a time. This is a heuristic to detect if two different
  // users in the same process use this function.
  assert(!IAudioPluginExtensionCallback::default_impl);
  if (impl) {
    IAudioPluginExtensionCallback::default_impl = impl;
    return true;
  }
  return false;
}
const std::shared_ptr<IAudioPluginExtensionCallback>& IAudioPluginExtensionCallback::getDefaultImpl() {
  return IAudioPluginExtensionCallback::default_impl;
}
std::shared_ptr<IAudioPluginExtensionCallback> IAudioPluginExtensionCallback::default_impl = nullptr;
::ndk::ScopedAStatus IAudioPluginExtensionCallbackDefault::completed(int32_t /*in_instanceId*/, int32_t /*in_requestId*/, const std::string& /*in_errorMessage*/) {
  ::ndk::ScopedAStatus _aidl_status;
  _aidl_status.set(AStatus_fromStatus(STATUS_UNKNOWN_TRANSACTION));
  return _aidl_status;
}
::ndk::SpAIBinder IAudioPluginExtensionCallbackDefault::asBinder() {
  return ::ndk::SpAIBinder();
}
bool IAudioPluginExtensionCallbackDefault::isRemote() {
  return false;
}
}  // namespace androidaudioplugin
}  // namespace org
}  // namespace aidl
