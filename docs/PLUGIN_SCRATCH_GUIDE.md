
# Building AAP: First Step Guide

This is a brief introduction for plugin developers to create audio plugin
for Android using AAP. A more comprehensive documentation is available at [DEVELOPERS.md](DEVELOPERS.md).

AAP project consists of fairly complicated set of files, so ideally there
should be some project template. Unfortunately Android Studio is too general
for that purpose to provide additional template for AAP. At this state,
it's best to cope our simplest example `AAPBareBoneSamplePlugin` and
rewrite wherever appropriate.

## Where to make changes

- `build.gradle.kts`
  - If you create your project from a template in Android Studio, it's best to create
    Android Application project with the template that brings in C++ code. It will
    contain `externalNativeBuild` sections.
  - Add `org.androidaudioplugin:androidaudioplugin:(version)` package in `dependencies`.
    (Note that at this stage we haven't published those Maven artifacts, so you will
    have to submodule this `aap-core` and run
    `./gradlew build publishToMavenLocal`, add `mavenLocal()` to the
    `repositories { ... }` block in `build.gradle(.kts)`.)
  - As part of "prefab"-based packaging transition, we are based on the
    "share the same libc++_shared.so" strategy. That means, your STL settings
    by ANDROID_STL=libc++_static (which is the default in Android Gradle Plugin)
    will be detected and rejected by Android Gradle Plugin. You will have to build
    your application (host/plugin) code with `ANDROID_STL=c++_shared` in your
    `build.gradle.kts`: ` android { defaultConfig { externalNativeBuild { cmake { arguments ("-DANDROID_STL=c++_shared") } } } }`
  - AAP libraries are built for [Prefab packaging system](https://developer.android.com/studio/build/dependencies?agpversion=4.1#native-dependencies-aars).
    To consume it in your project, add `buildFeatures { prefab true }` in your `build.gradle(.kts)`.
- `src/main/cpp/CMakeLists.txt`
  - It's best to start with automatically generated file from C++ based
    project template.
  - If you enabled Prefab, add `find_package(androidaudioplugin REQUIRED CONFIG)`,
    and then add `androidaudioplugin::androidaudioplugin` in `target_link_libraries{}`.
    Otherwise, add AAP include directory (have it somehow, e.g. a git submodule) to `target_include_directories` list.
- `src/main/AndroidManifest.xml` - described later.
- `src/main/cpp/*.cpp` - implement plugin here.
- `src/main/java/[your]/[package]/[here]/MainActivity.kt` - in case 
  you would like to show some configuration settings or list plugins
  (you can have multiple plugins in the service in the app). It is totally optional.
- `src/main/res/xml/aap-metadata.xml` - described later.


### AndroidManifest.xml

Add `<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />` .

A service needs to be described there:

```
        <service android:name="org.androidaudioplugin.AudioPluginService"
            android:label="AAPBareBoneSamplePlugin">
            <intent-filter>
                <action
                    android:name="org.androidaudioplugin.AudioPluginService.V2" />
            </intent-filter>
            <meta-data
                android:name="org.androidaudioplugin.AudioPluginService.V2#Plugins"
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
      <port id="0" direction="input" content="audio" name="Left In" />
      <port id="1" direction="input" content="audio" name="Right In" />
      <port id="2" direction="output" content="audio" name="Left Out" />
      <port id="3" direction="output" content="audio" name="Right Out" />
      <port id="4" direction="input" content="midi2" name="MIDI In" />
      <port id="5" direction="output" content="midi2" name="MIDI Out" />
    </ports>
    <parameters xmlns="urn://androidaudioplugin.org/extensions/parameters">
      <parameter id="0" name="Output Volume L" default="0.5" minimum="0" maximum="1" />
      <parameter id="1" name="Output Volume R" default="0.5" minimum="0" maximum="1" />
      <parameter id="2" name="Delay L" default="0" minimum="0" maximum="2048" />
      <parameter id="3" name="Delay R" default="256" minimum="0" maximum="2048" />
    </parameters>
  </plugin>
</plugins>
```

Give appropriate name, category, author, manufacturer, and unique-id.
For `category` attribute, specify `Instrument` if it is.
The `library` name should be indicated at `CMakeLists.txt` in the project.

For `parameter`, `id` is an integer.
`default`, `minimum` and `maximum` specify a float value for each.
