LINK.o = $(LINK.cc)
CXXFLAGS = -std=c++11 -Wall -Wno-sign-compare
CPPFLAGS = -I include/afp/

OBJS = o/finder_info.o o/resource_fork.o

# also cygwin...
ifeq ($(MSYSTEM),MSYS)
	OBJS += o/remap_os_error.o
endif

ifneq ($(OS),Windows_NT)
	OBJS += o/xattr.o
endif

libafp.a : $(OBJS)
	ar rcs $@ $^

.PHONY : clean
clean :
	$(RM) libafp.a $(OBJS)

o :
	mkdir $@

o/finder_info.o : src/finder_info.cpp include/afp/finder_info.h
o/resource_fork.o : src/resource_fork.cpp include/afp/resource_fork.h
o/remap_os_error.o : src/remap_os_error.c
o/xattr.o : src/xattr.c include/afp/xattr.h

o/%.o: src/%.c | o
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

o/%.o: src/%.cpp | o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<