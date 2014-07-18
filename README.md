rh_audio
========

mkdir build
cd build
RH_AOUT="ALSA" cmake -DCMAKE_BUILD_TYPE=Release ..
make

Other outputs are RH_AOUT="OpenSLES" and RH_AOUT="Embedded".

