if [ "$1" = "SP" ]
then
  echo "Load target $1"
  BOLOS_SDK=$NANOSP_SDK make load
elif [ "$1" = "S" ]
then
  echo "Load target $1"
  BOLOS_SDK=$NANOS_SDK make load
elif [ "$1" = "X" ]
then
  echo "Load target $1"
  BOLOS_SDK=$NANOX_SDK make load
else
  echo "Target is wrong, try again."
fi