BINFOLDER = bin
SRCFOLDER = src/main
INCLUDESFOLDER = src/main

CXX = clang++
CXXFLAGS = -I${INCLUDESFOLDER}

SOURCES = main.cpp
SRC = $(addprefix ${SRCFOLDER}/, ${SOURCES})

main: bin
	$(CXX) $(CXXFLAGS) -o ${BINFOLDER}/main ${SRC}

bin:
	mkdir -p ${BINFOLDER}