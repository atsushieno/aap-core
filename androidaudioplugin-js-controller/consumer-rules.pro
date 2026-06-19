# Keep the automation entry points reachable from JNI / reflection (adb broadcast path).
-keep class org.androidaudioplugin.js.AapAutomationRuntime { *; }
-keep class org.androidaudioplugin.js.AapAutomationReceiver { *; }
