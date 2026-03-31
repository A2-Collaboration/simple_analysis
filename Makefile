CXX       := g++
SrcSuf    := cxx
ObjSuf    := o

# Executable names (no suffix)
TARGETS   := aw_test
# aw_2nd

# Source files
SRCS      := $(addsuffix .$(SrcSuf), $(TARGETS))

# Object files
OBJS      := $(SRCS:.$(SrcSuf)=.$(ObjSuf))
DEPS      := $(OBJS:.o=.d)

# ROOT config
ROOTCFLAGS := $(shell root-config --cflags)
ROOTLIBS   := $(shell root-config --libs)

# Flags
CXXFLAGS := -Wall -g -MMD -MP $(ROOTCFLAGS)
LDFLAGS  := -g
LIBS     := $(ROOTLIBS)

# Default target
all: $(TARGETS)

# Link each executable from its matching object file
%: %.$(ObjSuf)
	$(CXX) $(LDFLAGS) $< $(LIBS) -o $@
	@echo "$@ done"

# Compile
%.$(ObjSuf): %.$(SrcSuf)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Include dependencies
-include $(DEPS)

# Clean
clean:
	rm -f $(OBJS) $(DEPS) $(TARGETS)

.PHONY: all clean
