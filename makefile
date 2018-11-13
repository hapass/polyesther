BINFOLDER = bin
SRCFOLDER = src/main
INCLUDESFOLDER = src/main

CXX = clang++
CXXFLAGS = -I${INCLUDESFOLDER} -std=c++17 -g

SOURCES = main.cpp
SRC = $(addprefix ${SRCFOLDER}/, ${SOURCES})

main: bin
	$(CXX) $(CXXFLAGS) -o ${BINFOLDER}/main ${SRC}

bin:
	mkdir -p ${BINFOLDER}