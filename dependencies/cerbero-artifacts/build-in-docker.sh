#!/bin/bash

docker run -t --rm -v $(pwd):/src bitriseio/android-ndk /bin/bash -ci "cd /src && bash setup.sh && make prepare build"

