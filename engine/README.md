#!/bin/bash

echo GnuGO 3.8
echo -- please install system package --


echo KataGo v1.4.2 opencl linux x64

src=https://github.com/lightvector/KataGo/releases/download/v1.4.2
file=katago-v1.4.2-opencl-linux-x64.zip
dir=katago

mkdir "$dir"
cd "$dir"
wget "$src/$file"
unzip "$file"
chmod +x katago
sed -i "s/^logToStderr = false/logToStderr = true/" default_gtp.cfg
sed -i "s/^rules = tromp-taylor/rules = japanese/" default_gtp.cfg

src=https://github.com/lightvector/KataGo/releases/download/v1.4.0
file=g170-b20c256x2-s4384473088-d968438914.bin.gz
dir=models

mkdir models
cd models
wget "$src/$file"

cd ../..
