#!/bin/sh

CORE="`sed -n 's|^CORE=||p' Makefile`"
export LD_LIBRARY_PATH="$CORE/.libs"
make
malamute "$CORE"/tools/malamute.cfg &
sleep 2
./lua_sf_poc &
sleep 1
echo
echo "=== Up & running ==="
echo
sleep 1
./generate_measurement test test_a W 4
sleep 2
./generate_measurement test test_b W 4
sleep 2
./generate_measurement test test_a W 5
sleep 2
./generate_measurement test test_b W 6
sleep 2
./generate_measurement test test_a W 6
sleep 2
./generate_measurement test test_b W 8
sleep 2
./generate_measurement test test_b W 5
sleep 2
./generate_measurement test test_a W 5
echo "=== Gonna die ==="
killall lua_sf_poc
killall malamute
