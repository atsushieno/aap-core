#include "aap/core/android/android-application-context.h"
#include "ALooperMessage.h"
#include <aap/unstable/logging.h>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace aap {

// Android-specific API. Not sure if we would like to keep it in the host API - it is for plugins.
JavaVM *android_vm{nullptr};
jobject application_context{nullptr};
// It is globally allocated, assuming that there is only one AudioPluginService object within an app.
// and thus it is the only loop runner within an app.
// We will have to change this assumption if there could be more than one service in an app...
std::unique_ptr<NonRealtimeLoopRunner> non_rt_loop_runner{nullptr};

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
	android_vm = vm;
	return JNI_VERSION_1_6;
}

void unset_application_context(JNIEnv *env) {
	if (application_context)
		env->DeleteGlobalRef(application_context);
}

void set_application_context(JNIEnv *env, jobject jobjectApplicationContext) {
    if (application_context)
        unset_application_context(env);
    if (!android_vm)
	    env->GetJavaVM(&android_vm);
    application_context = env->NewGlobalRef((jobject) jobjectApplicationContext);
}

JavaVM *get_android_jvm() { return android_vm; }

jobject get_android_application_context() { return application_context; }

NonRealtimeLoopRunner* get_non_rt_loop_runner() { return non_rt_loop_runner.get(); }

#include <pthread.h>
// Shared by the loop thread and the stopping thread, so the loop can return and the thread can end.
std::atomic<bool> non_rt_loop_quit{false};
std::mutex non_rt_loop_mutex;
std::condition_variable non_rt_loop_exited;
bool non_rt_loop_running{false};

void prepare_non_rt_event_looper() {
    assert(!non_rt_loop_runner);

	non_rt_loop_quit = false;
	auto looper = ALooper_prepare(0);
	ALooper_acquire(looper);
	// probably we need this constant adjustable...
	non_rt_loop_runner = std::make_unique<NonRealtimeLoopRunner>(looper, 65536);
}

void start_non_rt_event_looper() {
	{
		std::lock_guard<std::mutex> lock{non_rt_loop_mutex};
		non_rt_loop_running = true;
	}
	while(!non_rt_loop_quit) {
		int fd, events;
		void* data;
		ALooper_pollOnce(-1, &fd, &events, &data);
	}
	std::lock_guard<std::mutex> lock{non_rt_loop_mutex};
	non_rt_loop_running = false;
	non_rt_loop_exited.notify_all();
}

void stop_non_rt_event_looper() {
    if (!non_rt_loop_runner)
        return; // already stopped
    auto looper = (ALooper*) non_rt_loop_runner->getLooper();
    // Wake the loop and wait until it returns, so nothing polls the looper while it is torn down.
    non_rt_loop_quit = true;
    ALooper_wake(looper);
    {
        std::unique_lock<std::mutex> lock{non_rt_loop_mutex};
        non_rt_loop_exited.wait(lock, [] { return !non_rt_loop_running; });
    }
	non_rt_loop_runner.reset(nullptr);
	ALooper_release(looper);
}

AAssetManager *get_android_asset_manager(JNIEnv* env) {
	if (!application_context)
		return nullptr;
	auto appClass = env->GetObjectClass(application_context);
	auto getAssetsID = env->GetMethodID(appClass, "getAssets", "()Landroid/content/res/AssetManager;");
	auto assetManagerJ = env->CallObjectMethod(application_context, getAssetsID);
	return AAssetManager_fromJava(env, assetManagerJ);
}

}
