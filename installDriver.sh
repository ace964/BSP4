#!/bin/bash

function error_exit {
	echo "Error: "$1
	exit 1
}

function print_help {
	cat <<EOF
Usage: driver_install [ret_val_time [ret_val_number]]

ret_val_time is the default value displayed for time by the module when reading before writing at least two lines to it.

ret_val_number is the default value displayed for length by the module when reading before writing at least two lines to it.

The arguments have to be specified in the given order.
EOF
exit 0
}

[ $# -ge 1 ] && [ $1 = "-h" ] && print_help

DEV_FNAME=/dev/tzm
MODULE_NAME=tzm

# if file exists and is character file: remove
[ -e $DEV_FNAME -a -c $DEV_FNAME ] && { echo "Removing device file"; sudo rm $DEV_FNAME; }
[ -e $DEV_FNAME ] && error_exit "File $DEV_NAME could not be removed"

# if module is loaded: unload
if grep --quiet $MODULE_NAME /proc/modules
    then
	echo "Unloading module"
	sudo rmmod $MODULE_NAME
fi
grep --quiet $MODULE_NAME /proc/modules && error_exit "Module $MODULE_NAME could not be unloaded"

	
# build module
echo "Building module"
make || error_exit "Build failed"

echo "Loading module"
# retrieve module parameters
number_regex="^-?[0-9]+$"
[ $# -ge 1 ] && [[ $1 =~ $number_regex ]] && module_params="ret_val_time=$1"
[ $# -ge 2 ] && [[ $2 =~ $number_regex ]] && module_params="$module_params ret_val_number=$2"
# load module
echo $module_params
sudo insmod ./$MODULE_NAME".ko" $module_params
grep --quiet $MODULE_NAME /proc/modules || error_exit "Module $MODULE_NAME could not be loaded"

# get major device number
major=$(grep $MODULE_NAME /proc/devices | cut -d " " -f 1 -s)

echo "Creating device node"
sudo mknod $DEV_FNAME c $major 0
[ -e $DEV_FNAME -a -c $DEV_FNAME ] || error_exit "Device node could not be created"

echo "Setting access rights"
sudo chmod 666 $DEV_FNAME

echo "done"
