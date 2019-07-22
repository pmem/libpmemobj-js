#!/bin/bash
CUR_DIR=`pwd`
BLD_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

cd ${BLD_DIR}

# download googletest from github
git clone https://github.com/pmem/pmdk.git

# install libfabric
sudo ./pmdk/utils/docker/images/install-libfabric.sh
      
# install libndctl
sudo ./pmdk/utils/docker/images/install-libndctl.sh

# make & make install
cd pmdk && make && sudo make install

cd ${CUR_DIR}
