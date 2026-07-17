CXX = g++
CXXFLAGS = -O2 -std=c++14 -Wall -Isrc

code: src/main.cpp src/ticket_system.hpp src/hashmap.hpp src/vector.hpp src/utils.hpp
	$(CXX) $(CXXFLAGS) -o code src/main.cpp

.PHONY: clean
clean:
	-del /Q code code.exe *.dat *.o 2>NUL
	-rm -f code code.exe *.dat *.o
