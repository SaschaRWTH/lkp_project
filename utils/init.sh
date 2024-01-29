# Initiation script to insert the outchef modual 
# and mount the "-.img" disk to the system
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

insmod  $SCRIPT_DIR/ouichefs.ko

# For the mount to work, qemu needs to be given the img file as 
# "-drive $path.img,format=raw" argument
mount /dev/sdc /oui