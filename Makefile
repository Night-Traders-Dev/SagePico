# SagePico Build System for Feather RP2350
SAGE := $(CURDIR)/deps/sagelang/sage
SAGEVM := $(CURDIR)/deps/SageVM/sagevm
PICOTOOL := $(shell which picotool 2>/dev/null || echo $(HOME)/.local/bin/picotool)

.PHONY: all clean run test sagevm sr uf2 flash flash-arm flash-riscv

all: uf2-riscv uf2-arm

# --- Desktop tests ---
run:
	@$(SAGE) src/hello.sage

test: sagevm
	@$(SAGEVM) run src/hello.sgvm

sagevm:
	@$(SAGEVM) compile src/hello.sage

sr:
	@$(SAGEVM) compile --riscv src/hello.sage

# --- UF2 builds ---
uf2-riscv:
	@./build.sh riscv

uf2-arm:
	@./build.sh arm

uf2: uf2-riscv uf2-arm

# --- Flash ---
flash-riscv: uf2-riscv
	@echo "Flashing RISC-V build..."
	@$(PICOTOOL) load -f build/hello-riscv.uf2 || \
		(echo "Put board in BOOTSEL mode and run: $(PICOTOOL) load build/hello-riscv.uf2")

flash-arm: uf2-arm
	@echo "Flashing ARM build..."
	@$(PICOTOOL) load -f build/hello-arm.uf2 || \
		(echo "Put board in BOOTSEL mode and run: $(PICOTOOL) load build/hello-arm.uf2")

flash: flash-arm

clean:
	@rm -rf .tmp build src/hello.sgvm src/hello.sgrv
	@echo "Cleaned build artifacts"
