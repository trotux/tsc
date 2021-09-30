#!/bin/sh

build () {
    echo "Building project"
    cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=YES -DCMAKE_VERBOSE_MAKEFILE=YES
    cmake --build build
    ln -sf build/compile_commands.json
}

install() {
    echo "Not implemented"
}

clean() {
    echo "Cleaning project"
    rm -rf build
}

ACTION="build"

for i in "$@"
do
    echo ">>$i"
case $i in
    build)
        ACTION="build"
    ;;
    install)
        ACTION="install"
    ;;
    clean)
        ACTION="clean"
    ;;
    *)
    ;;
esac
done

echo "Action ${ACTION}"

if [ $ACTION = "build" ]; then
    build
elif [ $ACTION = "install" ]; then
    install
elif [ $ACTION = "clean" ]; then
    clean
else
   echo "Unknown action!"
fi


