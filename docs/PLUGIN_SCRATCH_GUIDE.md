
# Building AAP: First Step Guide

This is a brief introduction for plugin developers to create audio plugin
for Android using AAP.

AAP project consists of fairly complicated set of files, so ideally there
should be some project template. Unfortunately Android Studio is too general
for that purpose to provide additional template for AAP. At this state,
it's best to cope our simplest example `AAPBareBoneSamplePlugin` and
rewrite wherever appropriate.

## Where to make changes

- `build.gradle`
  - It's best to create Android Application project with the template
    that brings in C++ code. It will add `externalNativeBuild` sections.
  - Add `org.androidaudioplugin:androidaudioplugin:(version)` package in `dependencies`
    TODO: there is actually no package published there.
  - As part of "prefab"-based packaging transition, we are based on the
    "share the same libc++_shared.so" strategy. That means, your STL settings
    by ANDROID_STL=libc++_static (which is the default in Android Gradle Plugin)
    may result in some inconsistency issue. Our recommendation is to build
    your application (host/plugin) code with `ANDROID_STL=c++_shared` in your
    `build.gradle`: ` android { defaultConfig { externalNativeBuild { cmake { arguments "-DANDROID_STL=c++_shared" } } } }`
    - Note that prefab is not enabled in this repository yet (due to way too many existing known issues, see Google issuetracker for details).
- `CMakeLists.txt`
  - It's best to start with automatically generated file from C++ based
    project template.
  - Add AAP include directory to `target_include_directories` list.
    TODO: how do we resolve C/C++ headers in Android NDK world?
- `src/main/AndroidManifest.xml` - described later.
- `src/main/jni/*.cpp` - implement plugin here.
- `src/main/java/[your]/[package]/[here]/MainActivity.kt` - in case 
  you would like to show some configuration settings or list plugins
  (you can have multiple plugins in the service in the app).
- `src/main/res/xml/aap-metadata.xml` - described later.


### AndroidManifest.xml

Add `<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />` .

A service needs to be described there:

```
        <service android:name="org.androidaudioplugin.AudioPluginService"
            android:label="AAPBareBoneSamplePlugin">
            <intent-filter>
                <action
                    android:name="org.androidaudioplugin.AudioPluginService" />
            </intent-filter>
            <meta-data
                android:name="org.androidaudioplugin.AudioPluginService#Plugins"
                android:resource="@xml/aap_metadata"
                />
        </service>
```

If you have your own specific class that derives from 
`org.androidaudiopligin.AudioPluginService`, alter `android:name` on
`service`. Keep the one in `intent-filter` because it is the name used
by AAP hosts to query plugins.

If you have different meta-data XML file than `res/xml/aap-metadata.xml`
then alter `@xml/aap-metadata` too.

### aap-metadata.xml

This is the entire content for bare bone project:

```
<plugins>
  <plugin name="Flat Filter" category="Effect" author="atsushieno" manufacturer="atsushieno" unique-id="urn:org.androidaudioplugin/samples/aapbarebonepluginsample/FlatFilter" library="libaapbareboneplugin.so">
    <ports>
      <port direction="input" content="audio" name="Left In" />
      <port direction="input" content="audio" name="Right In" />
      <port direction="output" content="audio" name="Left Out" />
      <port direction="output" content="audio" name="Right Out" />
    </ports>
  </plugin>
</plugins>
```

Give appropriate name, category, author, manufacturer, and unique-id.
The `library` name should be indicated at `CMakeLists.txt` in the project.

