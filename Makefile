PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

OBJS = MID.o url_parser.o MID_http.o MID_socket.o MID_structures.o MID_functions.o MID_ssl_socket.o MID_interfaces.o MID_unit.o  MID_arguments.o
LIBS = -lssl -lcrypto -lpthread

ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g
else ifeq ($(BUILD_MODE),run)
	CFLAGS += -O2
else
	$(error Build mode $(BUILD_MODE) not supported by this Makefile)
endif

all:	MID

MID:	$(OBJS) 
	$(CC) -o $@ $^  $(LIBS)
%.o:	$(PROJECT_ROOT)%.cpp
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

%.o:	$(PROJECT_ROOT)%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -fr MID $(OBJS)
