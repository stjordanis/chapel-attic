#!/usr/bin/env bash
#
# Test default configuration on mac os x.

CWD=$(cd $(dirname $0) ; pwd)
source $CWD/common.bash

export CHPL_NIGHTLY_TEST_CONFIG_NAME="gnu.darwin"

export CHPL_HOST_COMPILER=gnu
$CWD/nightly -cron
