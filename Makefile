# SagePico Build System for Feather RP2350 (Hazard3 RISC-V)
SAGE := $(CURDIR)/deps/sagelang/sage
SAGEVM := $(CURDIR)/deps/SageVM/sagevm
PICO_SDK := $(CURDIR)/deps/pico-sdk
TOOLCHAIN := $(CURDIR)/deps/pico-sdk-tools/build/riscv-install/bin

.PHONY: all clean run test sagevm sr compile uf2

all: uf2

# Run Sage interpreter (desktop)
run:
	@$(SAGE) src/hello.sage

# Test SageVM bytecode (desktop)
test: sagevm
	@$(SAGEVM) run src/hello.sgvm

# Compile to SageVM bytecode
sagevm:
	@$(SAGEVM) compile src/hello.sage
	@echo "SGVM bytecode: src/hello.sgvm"

# Compile to SRVM (RISC-V) bytecode
sr:
	@$(SAGEVM) compile --riscv src/hello.sage
	@echo "SRVM bytecode: src/hello.sgrv"

# Generate Pico C code only
c:
	@$(SAGE) --emit-pico-c src/hello.sage -o .tmp/hello/hello.c
	@echo "C code: .tmp/hello/hello.c"

# Full build: C -> CMake -> UF2
uf2:
	@./build.sh

# Clean build artifacts
clean:
	@rm -rf .tmp build src/hello.sgvm src/hello.sgrv
	@echo "Cleaned build artifacts"
