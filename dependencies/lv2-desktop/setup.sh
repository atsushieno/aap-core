#!/bin/bash

git clone https://github.com/atsushieno/serd.git -b android --recursive
cd serd
./waf configure --no-utils --debug --prefix=../dist
./waf
./waf install
cd ..

git clone https://github.com/atsushieno/sord.git -b android --recursive
cd sord
./waf configure --debug --prefix=../dist
./waf
./waf install
cd ..

git clone https://github.com/drobilla/lv2.git --recursive
cd lv2
./waf configure --debug --prefix=../dist
./waf
./waf install
cd ..

git clone https://github.com/drobilla/sratom.git -b v0.6.2 --recursive
cd sratom
./waf configure --debug --prefix=../dist
./waf
./waf install
cd ..

git clone https://github.com/atsushieno/lilv.git -b android-0.24.4 --recursive
cd lilv
./waf configure --debug --prefix=../dist
./waf
./waf install
cd ..

git clone http://git.drobilla.net/mda.lv2.git --recursive
cd mda.lv2
./waf configure --debug --prefix=../dist
./waf
./waf install
cd ..
