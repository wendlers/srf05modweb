##
# Simple toplevel Makefile to build SRF05web package 
#
# Stefan Wendler, sw@kaltpost.de 
##

all: build 

build: 
	make -C ./src

deploy: build
	mkdir -p ./deploy/opt/app/bin
	mkdir -p ./deploy/opt/app/html/history
	cp ./src/srf05web ./deploy/opt/app/bin
	cp ./html/* ./deploy/opt/app/html/.
	(cd ./deploy && tar -zcvf srf05web.tgz opt/) 

clean:
	make -C ./src clean
	test -d ./deploy && rm -fr ./deploy
