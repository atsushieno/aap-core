#!/bin/sh

$JUCE_DIR/Projucer --resave android-audio-plugin-framework.jucer

cd Builds/Android && ./gradlew build

