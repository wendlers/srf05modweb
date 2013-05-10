##
# Simple toplevel Makefile to build SRF05 packages 
#
# Stefan Wendler, sw@kaltpost.de 
##

all: build 

build: 
	make -C ./srf05mod
	make -C ./srf05web

deploy: build
	make -C ./srf05mod deploy 
	make -C ./srf05web deploy 
	mkdir -p ./deploy
	cp -a ./srf05mod/deploy/opt ./deploy
	cp -a ./srf05web/deploy/opt ./deploy
	(cd ./deploy && tar -zcvf srf05.tgz opt/) 
	
clean:
	make -C ./srf05mod clean
	make -C ./srf05web clean
	test -d ./deploy && rm -fr ./deploy
