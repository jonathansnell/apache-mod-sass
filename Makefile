

all: build

build:
	apxs  -L/usr/local/lib/ -lsass -c mod_sass.c

install: 
	apxs -i -a mod_sass.la

clean: 
	@rm -f mod_sass.lo mod_sass.la mod_sass.slo
	@rm -Rf .libs
