#!/bin/bash

###################################################################
#
#  A transitional new devtest that runs a combination of the
#  new python-based test suite as well as the legacy perl-based
#  iCAT test suite.
#
###################################################################

# detect E-iRODS root directory
EIRODSROOT=$( dirname $( cd $(dirname $0) ; pwd -P ) )

# exit early if a command fails
set -e

# show commands as they are run
set -x

# restart the server, to exercise that code
cd $EIRODSROOT
$EIRODSROOT/iRODS/irodsctl restart

# run RENCI developed python-based devtest suite
# ( equivalent of original icommands and irules )
cd $EIRODSROOT/tests/pydevtest
python -m unittest -v -b -f test_eirods_resource_types

# run DICE developed perl-based devtest suite
cd $EIRODSROOT
$EIRODSROOT/iRODS/irodsctl devtesty

# done
exit 0
