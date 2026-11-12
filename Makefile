# Test della logica del protocollo sul PC (nessuna scheda necessaria).
#   make test
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O1 -g -fsanitize=address,undefined -Ilib/voicecore -Itests

CORE := $(wildcard lib/voicecore/*.cpp)
HDRS := $(wildcard lib/voicecore/*.h tests/*.h)
TESTS := packet stream node button audio output pins battery settings link level tone decode amp profile menu position morse nmea live repeater repeater_link message_store inbox text_picker rx_monitor meshcom_frame text_template choice_picker tx_scheduler sha256 sysop

.PHONY: test test-page test-py lcr_decode lcr_live codec_compare pdf clean
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
	node tests/page/run_wifi.js

# Decodificatore offline dei pacchetti catturati con un SDR (tools/lcr_decode.cpp): build/lcr_decode
lcr_decode: build/lcr_decode

# Confronto dei modi del Codec2 sulla stessa registrazione (tools/codec_compare.c): build/codec_compare. Usa gli stessi
# sorgenti del Codec2 che gira sulle schede (third_party/esp32_codec2-1.0.7); serve solo un compilatore C.
CODEC2_DIR := third_party/esp32_codec2-1.0.7/src
CODEC2_SRC := codec2.c codec2_fft.c kiss_fft.c kiss_fftr.c lpc.c lsp.c nlp.c pack.c phase.c postfilter.c quantise.c sine.c \
  interp.c mbest.c newamp1.c newamp2.c filter.c codebook.c codebookd.c codebookge.c codebookjmv.c codebookjvm.c \
  codebooklspmelvq.c codebookmel.c codebooknewamp1.c codebooknewamp1_energy.c codebooknewamp2.c codebooknewamp2_energy.c \
  dump.c mpdecode_core.c phi0.c golay23.c
codec_compare: build/codec_compare

build/codec_compare: tools/codec_compare.c $(addprefix $(CODEC2_DIR)/,$(CODEC2_SRC))
	@mkdir -p build
	$(CC) -O2 -w -I$(CODEC2_DIR) -o $@ $^ -lm

# Ricezione in tempo reale (tools/lcr_live.cpp): build/lcr_live
lcr_live: build/lcr_live

build/lcr_live: tools/lcr_live.cpp tools/lcr_live_core.h tools/lcr_decode_core.h $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -O2 -Ilib/voicecore -Itools -o $@ $< $(CORE)

build/lcr_decode: tools/lcr_decode.cpp tools/lcr_decode_core.h $(CORE) $(HDRS)
	@mkdir -p build
	$(CXX) -std=c++17 -Wall -Wextra -O2 -Ilib/voicecore -o $@ $< $(CORE)

# Prova della parte Python del ricevitore SDR (non serve GNU Radio).
test-py:
	python3 tests/sdr/test_assembler.py
	python3 tests/site/test_versions.py
	$(MAKE) --no-print-directory codec_compare
	python3 tests/codec/test_compare.py
	python3 -m py_compile tools/sdr/lora_rx.py
