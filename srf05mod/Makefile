##
# Simple toplevel Makefile to build SRF05 kmod package 
#
# Stefan Wendler, sw@kaltpost.de 
##

all: build 

build: 
	(cd ./src && make)

deploy: build
	mkdir -p ./deploy/opt/app/modules
	cp ./src/srf05.ko ./deploy/opt/app/modules/.
	(cd ./deploy && tar -zcvf srf05mod.tgz opt/) 

clean:
	(cd ./src && make clean)
	test -d ./deploy && rm -fr ./deploy
