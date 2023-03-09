if [ "$1" = "SP" ]
then
  echo "Build target $1"
  BOLOS_SDK=$NANOSP_SDK make clean
  BOLOS_SDK=$NANOSP_SDK make -j
elif [ "$1" = "S" ]
then
  echo "Build target $1"
  BOLOS_SDK=$NANOS_SDK make clean
  BOLOS_SDK=$NANOS_SDK make -j
elif [ "$1" = "STAX" ]
then
  echo "Build target $1"
  BOLOS_SDK=$STAX_SDK make clean
  BOLOS_SDK=$STAX_SDK make -j
elif [ "$1" = "X" ]
then
  echo "Build target $1"
  BOLOS_SDK=$NANOX_SDK make clean
  BOLOS_SDK=$NANOX_SDK make -j
elif [ "$1" = "all" ]
then
  echo "Build target $1"
  BOLOS_SDK=$NANOX_SDK make clean
  BOLOS_SDK=$NANOX_SDK make -j
  BOLOS_SDK=$NANOS_SDK make -j
  BOLOS_SDK=$NANOSP_SDK make -j
  BOLOS_SDK=$STAX_SDK make -j
else
  echo "Target is wrong, try again."
fi
