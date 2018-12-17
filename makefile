CXX = g++

TARGET_BIN = bin/david
TARGET_LIB = lib/libdavid.a

SRCS_CPP = $(shell find src -name *.cpp -and ! -name main.cpp)
SRCS_C = $(shell find src -name *.c)
OBJS_BIN = $(SRCS_CPP:.cpp=.o) $(SRCS_C:.c=.o) src/main.o
OBJS_LIB = $(SRCS_CPP:.cpp=.lib.o) $(SRCS_C:.c=.lib.o)

OPTS = -std=c++11 -lpthread
IDFLAGS =
LDFLAGS =

# USE-LP-SOLVE
ifneq (,$(findstring lpsolve,$(solver)))
	OPTS += -DUSE_LPSOLVE
	LDFLAGS += -llpsolve55
endif

# USE-GUROBI
ifneq (,$(findstring gurobi,$(solver)))
	OPTS += -DUSE_GUROBI
	LDFLAGS += -lgurobi_c++ -lgurobi65
endif

# USE CBC
ifneq (,$(findstring cbc,$(solver)))
	OPTS += -DUSE_CBC
	IDFLAGS += -I$(CBC_HOME)/include
	LDFLAGS += -L$(CBC_HOME)/lib -lOsi -lCoinUtils -lOsiClp -lCbc
endif

# USE SCIP
ifneq (,$(findstring scip,$(solver)))
	OPTS += -DUSE_SCIP 
	IDFLAGS += -I$(SCIP_HOME)/include
	LDFLAGS += -L$(SCIP_HOME)/lib -lscip -lzimpl-pic -lzimpl -lsoplex -lsoplex-pic  -lgmp -lreadline -lz
endif

OPTS_DEBUG = $(OPTS) -pg -g -D_DEBUG
OPTS_NDEBUG = $(OPTS) -O2 -DNDEBUG

ifeq ($(mode), debug)
	OPTS_BIN = $(OPTS_DEBUG)
	OPTS_LIB = $(OPTS_DEBUG)
else
	OPTS_BIN = $(OPTS_NDEBUG)
	OPTS_LIB = $(OPTS_NDEBUG)
endif


all: $(OBJS_BIN)
	mkdir -p bin
	$(CXX) $(OPTS_BIN) $(OBJS_BIN) $(IDFLAGS) $(LDFLAGS) -o $(TARGET_BIN)

.cpp.o:
	$(CXX) $(OPTS_BIN) $(IDFLAGS) -c -o $(<:.cpp=.o) $<


lib: $(OBJS_LIB)
	mkdir -p lib
	ar rcs $(TARGET_LIB) $(OBJS_LIB)

%.lib.o:%.cpp
	$(CXX) $(OPTS_LIB) $(IDFLAGS) -fPIC -c -o $(<:.cpp=.lib.o) $<

clean:
	rm -f $(TARGET_BIN)
	rm -f $(TARGET_LIB)
	rm -f $(OBJS_BIN)
	rm -f $(OBJS_LIB)
	rm -f $(OBJS_GTEST)
