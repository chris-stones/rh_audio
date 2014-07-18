#!/bin/bash
mkdir alsa_release
cd alsa_release
RH_AOUT="ALSA" cmake -DCMAKE_BUILD_TYPE=Release ..
make
sudo make install


