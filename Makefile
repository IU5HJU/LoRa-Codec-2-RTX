# Test della logica del protocollo sul PC (nessuna scheda necessaria).
#   make test
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -Ilib/voicecore -Itests

CORE := $(wildcard lib/voicecore/*.cpp)
HDRS := $(wildcard lib/voicecore/*.h tests/*.h)
TESTS := packet stream node button audio output pins battery settings link level tone decode amp

.PHONY: test test-page test-py lcr_decode pdf clean
test: $(addprefix build/test_,$(TESTS))
	@for t in $^; do ./$$t || exit 1; done

build/test_%: tests/test_%.cpp $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $< $(CORE)

clean:
	rm -rf build

# Rigenera docs/formato-pacchetto.pdf dal codice (serve reportlab: pip install reportlab).
pdf:
	python3 tools/make_formato_pacchetto_pdf.py

# Prova della pagina web con un dispositivo seriale finto (serve Node e il modulo playwright con Chromium).
test-page:
	node tests/page/run.js

# Decodificatore offline dei pacchetti catturati con un SDR (tools/lcr_decode.cpp): build/lcr_decode
lcr_decode: build/lcr_decode

build/lcr_decode: tools/lcr_decode.cpp tools/lcr_decode_core.h $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -O2 -Ilib/voicecore -o $@ $< $(CORE)

# Prova della parte Python del ricevitore SDR (non serve GNU Radio).
test-py:
	python3 tests/sdr/test_assembler.py
	python3 -m py_compile tools/sdr/lora_rx.py
