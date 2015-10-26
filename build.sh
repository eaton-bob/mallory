#!/bin/sh

gcc -std=c11 -D_SVID_SOURCE -lmlm -lczmq -lzmq src/alerts.c -o alerts
g++ -std=c++11 -lmlm -lczmq -lzmq src/ui.cc -o ui
