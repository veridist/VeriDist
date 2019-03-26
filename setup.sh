#!/bin/bash

echo "###################################### Installing Dependencies #####################################"
sudo apt install -y cmake g++ libboost-dev python3

cd ..

echo "###################################### Setting up SimGrid #####################################"
wget http://gforge.inria.fr/frs/download.php/file/37758/SimGrid-3.21.tar.gz
tar xf SimGrid-3.21.tar.gz
cd SimGrid-3.21/

cmake -DCMAKE_INSTALL_PREFIX=/opt/simgrid -Denable_documentation=off .
make
sudo make install

echo "###################################### Cleaning up #####################################"
rm -rf SimGrid-3.21/
rm SimGrid-3.21.tar.gz

cd -

echo "###################################### Setting up VeriDist #####################################"
mkdir build
cd build
cmake ..
make
cd -

