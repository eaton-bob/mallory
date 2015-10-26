#!/bin/sh

gcc -std=c11 -D_SVID_SOURCE -lmlm -lczmq -lzmq src/alerts.c -o alerts
