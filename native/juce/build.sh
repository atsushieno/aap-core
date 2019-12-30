#!/bin/sh

$JUCE_DIR/Projucer --resave android-audio-plugin-framework.jucer

# There is no way to generate those files in Projucer.
cp project-override.build.gradle Builds/Android/build.gradle
cp copy-into-generated-project.gradle.properties Builds/Android/gradle.properties

cd Builds/Android && ./gradlew build

