CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2
SRC = src/python_analyzer.cpp
TARGET = bin/python_analyzer

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf bin

.PHONY: all run clean
