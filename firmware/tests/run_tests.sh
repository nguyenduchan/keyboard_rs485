#!/bin/sh
# Biên dịch và chạy unit test trên PC (gcc bất kỳ). Dùng: sh tests/run_tests.sh [basic|modbus|display|engine|ui]
set -e
cd "$(dirname "$0")/.."
mkdir -p build
SRC="crc rtcfg datafmt storage mb_slave mb_master display keys buzzer cfg cfgsrv engine ui menu app"
FILES=""
for f in $SRC; do FILES="$FILES App/Src/$f.c"; done
gcc -std=gnu11 -Wall -Wextra -g -O0 -IApp/Inc -Itests -o build/tests.exe tests/*.c $FILES -lm
./build/tests.exe "$@"
