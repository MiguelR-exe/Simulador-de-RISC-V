CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

TARGET := rvsim
SRC := main.cpp simulator.cpp

all: $(TARGET)

$(TARGET): $(SRC) simulator.hpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean