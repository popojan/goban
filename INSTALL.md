# Build on clean Ubuntu 16.04
```
sudo apt-get update
sudo add-apt-repository universe
sudo apt-get update
sudo apt-get install git cmake libtool autoconf \
  libfreetype6-dev libgl1-mesa-dev libglew-dev  \
  libncurses5-dev libglm-dev libboost-all-dev   \
  gnugo
git clone https://github.com/popojan/goban.git
cd goban
git checkout automate-linux-build
make
rm config/engines.enabled/pachi-fast.txt
./goban
```
