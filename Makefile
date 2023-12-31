CXX=mpiCC
CXXFLAGS:=-Wall -Wextra -pedantic -std=c++17
RELEASEFLAGS:=-O3
DEBUGFLAGS:=-g

.PHONY: all clean
all: submission

submission: main.o
	$(CXX) $(CXXFLAGS) $(RELEASEFLAGS) -o troons $^

main.o: main.cc
	$(CXX) $(CXXFLAGS) $(RELEASEFLAGS) -c $^

clean:
	$(RM) *.o troons *.out

debug: main.cc
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) -D DEBUG -o troons main.cc
