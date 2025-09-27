# ----------------------------
# Compiler and flags
# ----------------------------
CXX      := g++-13
CXXFLAGS := -std=c++20 -fmodules-ts -O2 -Wall -Wextra
LDFLAGS  := -lpthread

# ----------------------------
# Project structure
# ----------------------------
SRC_DIR  := httpRestServerAsCPP20Module
OUT_DIR  := build
BMI_DIR  := gcm.cache
EXE      := $(OUT_DIR)/httpRestServer

# ----------------------------
# Sources
# ----------------------------
CPPS     := $(wildcard $(SRC_DIR)/*.cpp)
CPP_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OUT_DIR)/%.o,$(CPPS))
IXXS     := $(wildcard $(SRC_DIR)/*.ixx)

# ----------------------------
# Default target
# ----------------------------
all: setup modules cpp link

# ----------------------------
# Step 0: create directories and set module cache
# ----------------------------
setup:
	mkdir -p $(OUT_DIR)
	mkdir -p $(BMI_DIR)
	export GCC_MODULE_CACHE=$(PWD)/$(BMI_DIR)

# ----------------------------
# Step 1: compile module interfaces (.ixx)
# ----------------------------
modules:
	@echo "Compiling modules..."
	@export GCC_MODULE_CACHE=$(PWD)/$(BMI_DIR); \
	for f in $(IXXS); do \
		echo "Compiling $$f"; \
		$(CXX) $(CXXFLAGS) -c $$f; \
	done

# ----------------------------
# Step 2: compile .cpp sources (.cpp -> .o)
# ----------------------------
cpp: $(CPP_OBJS)

$(OUT_DIR)/%.o: $(SRC_DIR)/%.cpp | setup modules
	@echo "Compiling $$<"
	@export GCC_MODULE_CACHE=$(PWD)/$(BMI_DIR); \
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

# ----------------------------
# Step 3: link all .o files
# ----------------------------
link: $(CPP_OBJS)
	@echo "Linking executable..."
	$(CXX) $(CXXFLAGS) $^ -o $(EXE) $(LDFLAGS)

# ----------------------------
# Clean build
# ----------------------------
clean:
	rm -rf $(OUT_DIR) $(BMI_DIR)

.PHONY: all setup modules cpp link clean
