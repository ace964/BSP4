#!/bin/bash

DeviceName=/dev/tzm
ModuleName=tzm

# make module
echo "Building module"
make || error_exit "Build failed"

echo "Loading module"
# get parameters of module
number_regex="^-?[0-9]+$"
module_params="ret_val_time=$1 ret_val_number=$2"

# load module
sudo insmod ./$ModuleName".ko" $module_params

# get major number of device
major=$(grep $ModuleName /proc/devices | cut -d " " -f 1 -s)

echo "Creating device node"
sudo mknod $DeviceName $major 0
[ -e $DeviceName -a -c $DeviceName ] 

echo "Setting access rights"
sudo chmod 666 $DeviceName

echo "OK"
