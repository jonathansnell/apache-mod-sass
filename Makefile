

all: build

build:
	apxs -Ilibsass/include/ -L/usr/local/lib/ -lsass -lstdc++ -c mod_sass.c

install:
	apxs -i -a mod_sass.la

clean:
	@rm -f mod_sass.lo mod_sass.la mod_sass.slo
	@rm -Rf .libs
