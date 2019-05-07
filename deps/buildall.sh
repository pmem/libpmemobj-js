#!/bin/bash

###############################################################
#
# Top level build script to get all QA dependent frameworks
#
###############################################################

CUR_DIR=`pwd`
BLD_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd ${BLD_DIR}

# Enumerate all sub directories and call build.sh correspondingly
for sub in */; do
		echo "*********** building...${sub} **************"
		if [ -e ${sub}/build.sh ]
		then
				bash ${sub}/build.sh
		fi
done

cd ${CUR_DIR}
