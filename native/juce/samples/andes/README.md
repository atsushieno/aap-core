[![Build Status](https://travis-ci.org/artfwo/andes.svg?branch=master)](https://travis-ci.org/artfwo/andes)
[![Build status](https://ci.appveyor.com/api/projects/status/bpfxcc005hxnn3o2/branch/master?svg=true)](https://ci.appveyor.com/project/artfwo/andes)

# andes

Andes is a digital software synthesizer based on Perlin noise. It is still
in early stages of development and doesn't have a lot of features yet.

The core of Andes is an original oscillator which uses a 1-dimensional version of
[Perlin noise](https://en.wikipedia.org/wiki/Perlin_noise) to produce sound.

The oscillator design is described in the LAC 2018 paper [Using Perlin noise for sound synthesis](https://lac.linuxaudio.org/2018/pdf/14-paper.pdf). It is not yet finalized, so please keep in mind that
things may break in the future versions. Do not use this plugin in production. :)

Copyright (C) 2017  Artem Popov <art@artfwo.net>

Distributed under GPLv3 license, please see LICENSE file in the top dir.
