#!/bin/sh

if [ -z "$1" ]; then echo name not set! && exit; fi
DIR=$(basename $PWD)
NAME=telegram-wipe-$1

docker build -t telegram-wipe .
echo Press ^P^Q to detach container without stopping

docker volume create $NAME

set -ex

docker stop $NAME || true
docker rm $NAME || true

docker run -ti --name $NAME \
	  -v $NAME:/app/tdlib \
	  -v $PWD/out:/app/out \
	  -e TD_NAME=$1 \
	  --restart always \
	 telegram-wipe:latest
