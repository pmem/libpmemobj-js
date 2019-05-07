#!/bin/bash

###############################################################
#
# Top level build script to cleanup all QA dependent frameworks
#
###############################################################

CUR_DIR=`pwd`
BLD_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
cd ${BLD_DIR}

# Enumerate all sub directories and call build.sh correspondingly
for sub in */; do
		echo "*********** cleaning...${sub} **************"
		if [ -e ${sub}/clean.sh ]
		then
				bash ${sub}/clean.sh
		fi
done

cd ${CUR_DIR}
