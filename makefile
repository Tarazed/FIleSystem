SRC = ./src
INC = ./include
OBJ = ./obj
BIN = ./bin

SRCS = $(wildcard $(SRC)/*.cpp)
OBJS = $(patsubst %cpp,$(OBJ)/%o,$(notdir $(SRCS)))

CXX = g++
CXXFLAGS = -Wall -o2 -fPIC -I$(INC)

TARGET = MyFs
BIN_TARGET = $(BIN)/$(TARGET)

$(BIN_TARGET):$(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(OBJ)/%.o: $(SRC)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $^

.PHONY:clean
clean:
	rm $(OBJ)/*.o $(BIN)/$(TARGET) mydisk.img

