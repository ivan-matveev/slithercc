#!/bin/bash

set -e
set -o pipefail

./slithercc_replay_server 2>&1 | tee slithercc_replay_server.log

