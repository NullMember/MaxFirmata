OSXOPTIONS := -it --rm -v $(CURDIR):/workspace -w /workspace -e CROSS_TRIPLE=osx64 multiarch/crossbuild:dev
OSXCMD := cc -arch i386 -arch x86_64 -shared src/maxfirmata.c -DMAC_VERSION \
-I libs/max/c74support/max-includes -I libs/max/c74support/max-includes/x64 \
-F libs/max/c74support/max-includes @libs/max/c74support/max-includes/c74_linker_flags.txt \
-framework MaxAPI -o Package/maxfirmata/externals/maxfirmata.mxo/maxfirmata

WIN32OPTIONS := -it --rm -v $(CURDIR):/workspace -w /workspace -e CROSS_TRIPLE=win32 multiarch/crossbuild:dev
WIN32CMD := cc -shared src/maxfirmata.c -DWIN_VERSION -DWIN_EXT_VERSION \
-I libs/max/c74support/max-includes -L libs/max/c74support/max-includes \
-l MaxAPI -o Package/maxfirmata/externals/maxfirmata.mxe

WIN64OPTIONS := -it --rm -v $(CURDIR):/workspace -w /workspace -e CROSS_TRIPLE=win64 multiarch/crossbuild:dev
WIN64CMD := cc -shared src/maxfirmata.c -DWIN_VERSION -DWIN_EXT_VERSION \
-I libs/max/c74support/max-includes -I libs/max/c74support/max-includes/x64 \
-L libs/max/c74support/max-includes/x64 -l MaxAPI -o Package/maxfirmata/externals/maxfirmata.mxe64

all: clean build

build:
	mkdir Package/maxfirmata/externals
	mkdir Package/maxfirmata/externals/maxfirmata.mxo
	cp LICENSE Package/maxfirmata/LICENSE
	cp README.md Package/maxfirmata/README.md
	docker run $(OSXOPTIONS) $(OSXCMD)
	docker run $(WIN32OPTIONS) $(WIN32CMD)
	docker run $(WIN64OPTIONS) $(WIN64CMD)

clean:
	rm -f -r externals
	rm -f -r Package/maxfirmata/externals
	rm -f Package/maxfirmata/LICENSE
	rm -f Package/maxfirmata/README.md