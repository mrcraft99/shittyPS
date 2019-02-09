SRC_DIR = ..
PB_DIR = ../proto
LIB_DIR = /usr/local/lib
DEP_DIR = /usr/local/include
BOOST_ROOT = /usr/local/boost_1_67_0
BOOST_LIB_DIR = /usr/local/boost_1_67_0/stage/lib
SHITTY_PS = shittyPS.a
VPATH = $(SRC_DIR):$(PB_DIR)

CXX = g++
INCS = -I$(BOOST_ROOT) \
       -I$(SRC_DIR) \
       -I$(PB_DIR) \
       -I$(DEP_DIR)
#LDFLAGS = -Xlinker "-(" \
#	 -L$(BOOST_LIB_DIR) \
#	 -L$(LIB_DIR) \
#	 -lglog \
#	 -lgflags \
#     -lboost_system \
#     -lboost_thread \
#     -lboost_date_time \
#     -lboost_regex \
#     -lboost_serialization \
#	 -Xlinker "-)"

LDFLAGS = -L$(BOOST_LIB_DIR) \
	 -L$(LIB_DIR) \
	 -lglog \
	 -lgflags \
	 -lprotobuf \
     -lboost_system \
     -lboost_filesystem \
     -lboost_thread \
     -lboost_date_time \
     -lboost_regex \
     -lboost_serialization
CXX_FLAGS = -g -O3 -Wall -Wno-reorder $(INCS) -std=c++11 -Wno-unused-local-typedefs -Wno-unused-function

TEST_SRC = test.cpp
TEST_SYNC_SRC = test_bounded_delay.cpp
MAIN_OBJ=$(TEST_SRC:.cpp=.o)
MAIN_SYNC_OBJ=$(TEST_SYNC_SRC:.cpp=.o)
PB_SRCS = $(wildcard $(PB_DIR)/*.proto)
PB_GENS=$(PB_SRCS:.proto=.cc) $(PB_SRCS:.proto=.pb.h)
PB_OBJS=$(PB_SRCS:.proto=.pb.o)
SRCS=$(filter-out $(MAIN_SRC), $(wildcard $(SRC_DIR)/*.cpp))
OBJS = $(SRCS:.cpp=.o)
#TODO: add deps
DEPS = $(SRCS:.cpp=.d)

TEST_BIN = test
TEST_SYNC_BIN = test_bounded_delay

all: $(TEST_BIN) $(TEST_SYNC_BIN)
test1: $(TEST_BIN) 
test2: $(TEST_SYNC_BIN) 

$(TEST_BIN): $(PB_OBJS) $(MAIN_OBJ) $(OBJS)
	$(CXX) $(LDFLAGS) $(CXX_FLAGS) -o $(TEST_BIN) $(MAIN_OBJ) $(OBJS) $(PB_OBJS)

$(TEST_SYNC_BIN): $(PB_OBJS) $(MAIN_SYNC_OBJ) $(OBJS)
	$(CXX) $(LDFLAGS) $(CXX_FLAGS) -o $(TEST_SYNC_BIN) $(MAIN_SYNC_OBJ) $(OBJS) $(PB_OBJS)

%.pb.o : %.pb.cc
	$(CXX) $(CXX_FLAGS) -c $< -o $@

%.pb.cc: %.proto
	protoc -I $(PB_DIR) --cpp_out=$(PB_DIR) $< 

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c $*.cpp -o $@

clean:
	rm -rf $(OBJS) $(BINARY) $(MAIN_OBJ) $(PB_GENS) $(PB_OBJS) $(TEST_BIN) $(TEST_SYNC_BIN) $(MAIN_OBJ) $(MAIN_SYNC_OBJ)
