# AAP JUCE audio processor and client modules

This is a place where we have JUCE modules for AAP, both plugins and hosts.

- Host sample
  - JuceAAPAudioPluginHost
- Plugin sample
  - andes

## Build Instruction

This is what @atsushieno usually runs:

```
$ JUCE_DIR=/path/to/JUCE_sdk ./samples/JuceAAPAudioPluginHost/build.sh
$ cd ./samples/andes && JUCE_DIR=/path/to/JUCE_sdk APPNAME=Andes_1 ../../build.sh
$ cd ./samples/SARAH && JUCE_DIR=/path/to/JUCE_sdk APPNAME=SARAH ../../build.sh
```

Depending on the NDK setup you might also have to rewrite `Builds/Android/local.properties` to point to the right NDK location. Then run `cd Builds/Android && ./gradlew build` instead of `./build.sh`.

You have to build `androidaudioplugin` from the top directory before running
the scripts above.
Otherwise cmake will fail to resolve it and result in NOTFOUND.

## Under the hood

JUCE itself already supports JUCE apps running on Android and there is still
no need to make any changes to the upstream JUCE. We use 5.4.7 release.

Though Projucer is not capable of supporting arbitrary plugin format and
thus we make additional changes to the generated Android Gradle project.
It is mostly done by `fixup-project.sh`.

## Generating aap_metadata.xml

It is already done as part of `fixup-project.sh` but in case you would like
to run it manually...

To import JUCE audio plugins into AAP world, we have to create plugin
descriptor (`aap_metadata.xml`). We have a supplemental tool source to
help generating it automatically from JUCE plugin binary (shared code).

The command line below builds `aap-metadata-generator` under
`Builds/LinuxMakefile/build` and generates `aap_metadata.xml` at the plugin
top level directory:

```
APP=__yourapp__ gcc (__thisdir__)/tools/aap-metadata-generator.cpp \
	Builds/LinuxMakefile/build/$APP.a \
	-lstdc++ -ldl -lm -lpthread \
	`pkg-config --libs alsa x11 xinerama xext freetype2 libcurl webkit2gtk-4.0` \
	-o aap-metadata-generator \
	&&./aap-metadata-generator aap_metadata.xml
```

## Porting other JUCE-based audio plugins

Here are the porting steps that we had. Note that this applies only to samples built under this `samples` directory:

- Run Projucer and open the project `.jucer` file.
- open project settings and ensure to build at least one plugin format (`Standalone` works)
- Ensure that `JUCEPROJECT` has `name` attribute value only with `_0-9A-Za-z` characters. That should be handled by Projucer but its hands are still too short.
- Go to Modules and add module `juceaap_audio_plugin_client` (via path, typically)
- Go to Android exporter section and make following changes:
  - Module Dependencies: add list below
  - minSdkVersion 29
  - targetSdkVersion 29
  - Custom Manifest XML content: listed below
  - Gradle Version 6.1-rc-1
  - Android Plug-in Version 4.0.0-alpha09

Copy `sample-project-build.gradle` as the project top-level `build.gradle` in `Builds/Android`. In ideal world, Projucer can generate required content, but Projucer is not capable enough.

For module dependenciesm add below:

```
implementation project(':androidaudioplugin')
implementation "org.jetbrains.kotlin:kotlin-stdlib-jdk7:1.3.61"
```

For custom Manifest XML content, replace `!!!APPNAME!!!` part with your own (this is verified with JUCE 5.4.7 and may change in any later versions):

```
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="org.androidaudioplugin.aappluginsample" android:versionCode="1" android:versionName="0.1">
  <uses-permission android:name="android.permission.FOREGROUND_SERVICE"/>
  <application android:allowBackup="true" android:label="@string/app_name"
               android:supportsRtl="true" android:name="com.roli.juce.JuceApp" android:hardwareAccelerated="false">
    <service android:name="org.androidaudioplugin.AudioPluginService" android:label="!!!APPNAME!!!Plugin">
      <intent-filter>
        <action android:name="org.androidaudioplugin.AudioPluginService"/>
      </intent-filter>
      <meta-data android:name="org.androidaudioplugin.AudioPluginService#Plugins" android:resource="@xml/aap_metadata"/>
    </service>
  </application>
</manifest>
```


## Code origin and license

The sample app code under `samples/JuceAAPAudioPluginHost/Source` directory is slightly modified version of
JUCE [AudioPluginHost](https://github.com/WeAreROLI/JUCE/tree/master/extras/AudioPluginHost)
which is distributed under the GPLv3 license.

The sample app code under `samples/SARAH` directory is slightly modified version of [SARAH](https://github.com/getdunne/SARAH) which is distributed under the MIT license (apart from JUCE under GPLv3).

The sample app code under `samples/andes` directory is slightly modified version of [andes](https://github.com/artfwo/andes) which is distributed under the GPLv3 license.
