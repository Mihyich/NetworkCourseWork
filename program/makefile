override CC := gcc

BUILD ?= release

CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LINKFLAGS :=


override OUT_PATH := .out
override OUT_RELEASE_PATH := $(OUT_PATH)/Release
override OUT_RELEASE_OBJ_PATH := $(OUT_RELEASE_PATH)/Obj
override OUT_RELEASE_DEP_PATH := $(OUT_RELEASE_PATH)/Dep
override OUT_DEBUG_PATH := $(OUT_PATH)/Debug
override OUT_DEBUG_OBJ_PATH := $(OUT_DEBUG_PATH)/Obj
override OUT_DEBUG_DEP_PATH := $(OUT_DEBUG_PATH)/Dep
override OUT_DST_OBJ_PATH :=
override OUT_DST_DEP_PATH :=

override BUILD_PATH := .build
override BUILD_RELEASE_PATH := $(BUILD_PATH)/Release
override BUILD_DEBUG_PATH := $(BUILD_PATH)/Debug
override BUILD_DST_PATH :=


ifeq ($(BUILD), release)
  CFLAGS += -DNDEBUG -g0 -O3
#   LINKFLAGS += -s
  override OUT_DST_OBJ_PATH = $(OUT_RELEASE_OBJ_PATH)
  override OUT_DST_DEP_PATH = $(OUT_RELEASE_DEP_PATH)
  override BUILD_DST_PATH = $(BUILD_RELEASE_PATH)
else ifeq ($(BUILD), debug)
  CFLAGS += -DDEBUG -g3
  override OUT_DST_OBJ_PATH = $(OUT_DEBUG_OBJ_PATH)
  override OUT_DST_DEP_PATH = $(OUT_DEBUG_DEP_PATH)
  override BUILD_DST_PATH = $(BUILD_DEBUG_PATH)
else
  $(error Unexpected value for flag BUILD: '$(BUILD)')
endif


override INC_PATH := ./inc
override SRC_PATH := ./src


override SRC_FILES := $(wildcard $(SRC_PATH)/*.c)

override OBJ_FILES := $(patsubst %.c,$(OUT_DST_OBJ_PATH)/%.o,$(notdir $(SRC_FILES)))


override DEP_FILES := $(patsubst $(OUT_DST_OBJ_PATH)/%.o,$(OUT_DST_DEP_PATH)/%.d,$(OBJ_FILES))
-include $(DEP_FILES)

CFLAGS += -I$(INC_PATH)


all:
	@echo "SRC_FILES: $(SRC_FILES)"
	@echo "OBJ_FILES: $(OBJ_FILES)"


app: $(OBJ_FILES) | build_folder
	$(CC) $(LINKFLAGS) $^ -o $(BUILD_DST_PATH)/app -lpthread
.PHONY: app


$(OUT_DST_OBJ_PATH)/%.o : $(SRC_PATH)/%.c | out_folder
	$(CC) $(CFLAGS) -MMD -MP -MF $(OUT_DST_DEP_PATH)/$*.d -c $< -o $@


out_folder:
	-mkdir -p $(OUT_PATH)
	-mkdir -p $(OUT_RELEASE_PATH)
	-mkdir -p $(OUT_RELEASE_OBJ_PATH)
	-mkdir -p $(OUT_RELEASE_DEP_PATH)
	-mkdir -p $(OUT_DEBUG_PATH)
	-mkdir -p $(OUT_DEBUG_OBJ_PATH)
	-mkdir -p $(OUT_DEBUG_DEP_PATH)
.PHONY: out_folder


build_folder:
	-mkdir -p $(BUILD_PATH)
	-mkdir -p $(BUILD_RELEASE_PATH)
	-mkdir -p $(BUILD_DEBUG_PATH)
.PHONY: build_folder


clean:
	-rm -f $(OUT_DST_OBJ_PATH)/*.o
	-rm -f $(BUILD_DST_PATH)/*.a $(BUILD_DST_PATH)/*.so
.PHONY: clean


clean_all:
	-rm -rf $(OUT_PATH) $(BUILD_PATH)
.PHONY: clean_all
