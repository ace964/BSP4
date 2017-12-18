#!/bin/bash
#
#   USAGE
#   without parameters or with the following parameters:
#   first: ret_val_time
#   second: ret_val_number
#
function exitError {
	echo "Error: "$1
	exit 1
}

devFile=/dev/tzm
moduleName=tzm

# if file exists and is character file: remove
[ -e $devFile ] && { echo "Removing device file"; sudo rm $devFile; }
[ -e $devFile ] && exitError "Error while removing"

# unload module if needed.
if lsmod |grep --quiet $moduleName
    then
	echo "Unloading"
	sudo rmmod $moduleName
fi

lsmod |grep --quiet $moduleName && exitError "Error unloading"

	
# make module
make || exitError "error making"

echo "Loading module"
# extract module parameters from parameters
number_regex="^-?[0-9]+$"
[ $# -ge 1 ] && [[ $1 =~ $number_regex ]] && module_params="ret_val_time=$1"
[ $# -ge 2 ] && [[ $2 =~ $number_regex ]] && module_params="$module_params ret_val_number=$2"

# load module
echo $module_params
sudo insmod ./$moduleName".ko" $module_params
grep --quiet $moduleName /proc/modules || exitError "Module $moduleName could not be loaded"

# get major number
major=$(grep $moduleName /proc/devices | cut -d " " -f 1 -s)

echo "Creating device"
sudo mknod $devFile c $major 0

echo "Setting read/write access"
sudo chmod 666 $devFile

echo "install completed"

