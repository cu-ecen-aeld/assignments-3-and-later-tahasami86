#! /bin/sh

NAME=aesdsocket
STOP_SIGNAL = SIGTERM

case "$1" in
    start)
    echo -n "Starting daemon: "$NAME
    start-stop-daemon --start --name $NAME --background --startas /usr/bin/aesdsocket -- -d
    echo "."
    ;;
    stop)
    echo -n "Stoping daemon: "$NAME
    start-stop-daemon --stop -n $NAME --signal $STOP_SIGNAL 
    echo "."
    ;;

    *)
    echo "Usage: "$0" {Start|Stop}"
    exit 1

esac

exit 0