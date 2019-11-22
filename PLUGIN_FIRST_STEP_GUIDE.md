
# Building AAP: First Step Guide

## Steps From Scratch

- `build.gradle`
  - It's best to create Android Application project with the template
    that brings in C++ code. It will add `externalNativeBuild` sections.
  - Add `org.androidaudioplugin` package in `dependencies`
    TODO: there is actually no package published there.
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
  <plugin name="Flat Filter" category="Effect" author="atsushieno" manufacturer="atsushieno" unique-id="urn:org.androidaudioplugin/samples/aapbarebonepluginsample/FlatFilter">
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

