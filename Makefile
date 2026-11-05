# Test della logica del protocollo sul PC (nessuna scheda necessaria).
#   make test
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -Ilib/voicecore -Itests

CORE := $(wildcard lib/voicecore/*.cpp)
HDRS := $(wildcard lib/voicecore/*.h tests/*.h)
TESTS := packet stream node button audio output pins battery settings link

.PHONY: test clean
test: $(addprefix build/test_,$(TESTS))
	@for t in $^; do ./$$t || exit 1; done

build/test_%: tests/test_%.cpp $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $< $(CORE)

clean:
	rm -rf build
