BINFOLDER = bin
SRCFOLDER = src/main
INCLUDESFOLDER = src/main

CXX = clang++
CXXFLAGS = -I${INCLUDESFOLDER} \
	-std=c++17 \
	-g \
	-stdlib=libc++ \
	-lglfw \
	-framework CoreVideo \
	-framework OpenGL \
	-framework IOKit \
	-framework Cocoa \
	-framework Carbon

SOURCES = main.cpp
SRC = $(addprefix ${SRCFOLDER}/, ${SOURCES})

main:
	mkdir -p ${BINFOLDER}
	$(CXX) $(CXXFLAGS) -o ${BINFOLDER}/main ${SRC}