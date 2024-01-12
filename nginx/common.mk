
ifeq ($(DEBUG),true)
CC = g++ -g
VERSION = debug
else 
CC = g++
VERSION = release
endif

SRCS = $(wildcard *.cxx)
OBJS = $(SRCS:.cxx=.o)
DEPS = $(SRCS:.cxx=.d)

#BUILD_ROOT在config.mk中定义
BIN := $(addprefix $(BUILD_ROOT)/,$(BIN))

LINK_OBJ_DIR = $(BUILD_ROOT)/app/link_obj
DEP_DIR = $(BUILD_ROOT)/app/dep

$(shell mkdir -p $(LINK_OBJ_DIR))
$(shell mkdir -p $(DEP_DIR))

OBJS := $(addprefix $(LINK_OBJ_DIR)/,$(OBJS))
DEPS := $(addprefix $(DEP_DIR)/,$(DEPS))

#找到目录中所有的.o文件(编译出来的)
LINK_OBJ = $(wildcard $(LINK_OBJ_DIR)/*.o)
LINK_OBJ += $(OBJS)

all:$(DEPS) $(OBJS) $(BIN)

ifneq ("$(wildcard $(DEPS))","")
include $(DEPS)
endif

$(BIN):$(LINK_OBJ)
	@echo "----------------build $(VERSION) mode---------------"

# $@：目标     $^：所有目标依赖
	$(CC) -o $@ $^


$(LINK_OBJ_DIR)/%.o:%.cxx 
	$(CC) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cxx,$^)

$(DEP_DIR)/%.d:%.cxx 
	echo -n $(LINK_OBJ_DIR)/ > $@
	gcc -I$(INCLUDE_PATH) -MM $^ >> $@




