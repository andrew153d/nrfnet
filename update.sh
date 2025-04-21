sshpass -p "andrew" scp -r andrew@192.168.12.116:Documents/VersionControl/nrfmesh/ ~/
#scp -r andrew@192.168.12.222:Documents/VersionControl/nrfmesh/ ~/
cd ~/nrfmesh
sudo rm -rf ~/nrfmesh/build
mkdir build
cd build
cmake ..
make -j$(nproc)
sudo make install
