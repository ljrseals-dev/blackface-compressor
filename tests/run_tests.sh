#!/bin/sh
# Host-less DSP / parameter / stability tests (no SDK needed).
set -e
cd "$(dirname "$0")/.."
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Isource -Isource/dsp -o /tmp/bf_engine_test tests/engine_test.cpp
${CXX:-g++} -std=c++17 -O2 -Isource -Isource/dsp -o /tmp/bf_stab_test tests/stability_test.cpp
/tmp/bf_engine_test
/tmp/bf_stab_test
