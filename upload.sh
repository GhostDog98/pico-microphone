#!/bin/bash

# Shamelessly stolen from diminDLL
cd ./build/
make
cd ..
name=$(find ./build/ -name "*.uf2")
echo "found: $name"
RPI="/run/media/$USER/RPI-RP2/"
echo "waiting for RPI..."
timeout=0
while [ ! -d "$RPI" ]; do
    sleep 1
    timeout=$((timeout+1))
    if [ $timeout -gt 30 ]; then
        echo "RPI not found"
        exit 1
    fi
done
echo "found $RPI"
sleep 1
cp $name /run/media/$USER/RPI-RP2/
echo "written file"
while [ -d "$RPI" ]; do
    sleep 1
    timeout=$((timeout+1))
    if [ $timeout -gt 30 ]; then
	echo "RPI still mounted after 30 seconds, program may not have uploaded successfully"
	exit 1
    fi
done
echo "RPI has unmounted, program successfully written"
