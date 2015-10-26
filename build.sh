#!/bin/sh

gcc -lmlm -lczmq -lzmq src/alerts.c -o alerts
