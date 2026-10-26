WGPU_VERSION := v29.0.1.1
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
  ifeq ($(UNAME_M),arm64)
    WGPU_ASSET := wgpu-macos-aarch64-release.zip
  else
    WGPU_ASSET := wgpu-macos-x86_64-release.zip
  endif
else ifeq ($(UNAME_S),Linux)
  ifeq ($(UNAME_M),aarch64)
    WGPU_ASSET := wgpu-linux-aarch64-release.zip
  else
    WGPU_ASSET := wgpu-linux-x86_64-release.zip
  endif
else
  $(error Unsupported platform $(UNAME_S)/$(UNAME_M))
endif

DEPS_DIR := deps
WGPU_DIR := $(DEPS_DIR)/wgpu
WGPU_STAMP := $(WGPU_DIR)/.stamp

CC := cc
CPP := c++
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS := $(shell pkg-config --libs glfw3 2>/dev/null)

ifeq ($(UNAME_S),Darwin)
  INC = -I. -I$(WGPU_DIR)/include -I$(WGPU_DIR)/include/webgpu -I/opt/homebrew/include $(GLFW_CFLAGS)
  CPPFLAGS = -g -Wall -std=c++20 -DGL_SILENCE_DEPRECATION $(INC)
  OBJCFLAGS = -x objective-c -fno-objc-arc -I. -I$(WGPU_DIR)/include -I$(WGPU_DIR)/include/webgpu -I/opt/homebrew/include -DGLFW_EXPOSE_NATIVE_COCOA $(GLFW_CFLAGS)
  LDFLAGS = $(WGPU_DIR)/lib/libwgpu_native.a -lglfw -lobjc -framework QuartzCore -framework Metal \
            -framework CoreVideo -framework IOKit -framework IOSurface -framework Cocoa
else
  INC = -I. -I$(WGPU_DIR)/include -I$(WGPU_DIR)/include/webgpu $(GLFW_CFLAGS)
  CPPFLAGS = -g -Wall -std=c++20 $(INC)
  CFLAGS = -std=c11 -Wall -I. -I$(WGPU_DIR)/include -I$(WGPU_DIR)/include/webgpu -D_GLFW_X11 $(GLFW_CFLAGS)
  LDFLAGS = $(WGPU_DIR)/lib/libwgpu_native.a $(GLFW_LIBS) -pthread -ldl -lm
endif

TARGET2 = t2
TARGET3 = t3
TARGET_T22 = tarefa22
TARGET_PROJ2 = proj2

all: $(TARGET2) $(TARGET3) $(TARGET_T22) $(TARGET_PROJ2)

CORE_SRC = \
  wgpucontext.cpp \
  error.cpp \
  graphicsmath.cpp \
  wgslreflect.cpp \
  uniformbuffer.cpp \
  state.cpp \
  shader.cpp \
  pipeline.cpp \
  renderer.cpp \
  algorithm.cpp \
  node.cpp \
  scene.cpp \
  transform.cpp \
  camera2d.cpp \
  camera3d.cpp \
  arcball.cpp \
  light.cpp \
  pointlight.cpp \
  directionallight.cpp \
  spotlight.cpp \
  material.cpp \
  colormaterial.cpp \
  phongmaterial.cpp \
  textureset.cpp \
  texture.cpp \
  texcube.cpp \
  texdepth.cpp \
  sampler.cpp \
  clipplane.cpp \
  grid.cpp \
  triangle.cpp \
  cube.cpp \
  quad.cpp \
  sphere.cpp \
  square.cpp \
  skybox.cpp \
  mesh.cpp

LUXOR_SRC = \
  luxor/linearinterpolator.cpp \
  luxor/cubicinterpolator.cpp \
  luxor/movement.cpp \
  luxor/animation.cpp \
  luxor/luxorengine.cpp \
  luxor/luxor.cpp

CORE_OBJ = $(addsuffix .o, $(basename $(CORE_SRC))) glfw3webgpu.o
LUXOR_OBJ = $(addsuffix .o, $(basename $(LUXOR_SRC)))

.PRECIOUS: luxor/%.o

%.o: %.cpp $(WGPU_STAMP)
	$(CPP) $(CPPFLAGS) -c $< -o $@

glfw3webgpu.o: glfw3webgpu.c $(WGPU_STAMP)
ifeq ($(UNAME_S),Darwin)
	clang $(OBJCFLAGS) -c $< -o $@
else
	$(CC) $(CFLAGS) -c $< -o $@
endif

luxor/%.o: luxor/%.cpp $(WGPU_STAMP)
	$(CPP) $(CPPFLAGS) -c $< -o $@

main_2d.o: main_2d.cpp $(WGPU_STAMP)
	$(CPP) $(CPPFLAGS) -c $< -o $@

main_3d.o: main_3d.cpp $(WGPU_STAMP)
	$(CPP) $(CPPFLAGS) -c $< -o $@

main_tarefa22.o: main_tarefa22.cpp $(WGPU_STAMP)
	$(CPP) $(CPPFLAGS) -c $< -o $@

main_proj2.o: main_proj2.cpp $(WGPU_STAMP)
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(TARGET2): $(CORE_OBJ) main_2d.o Makefile
	$(CPP) -o $@ $(CORE_OBJ) main_2d.o $(LDFLAGS)

$(TARGET3): $(CORE_OBJ) main_3d.o Makefile
	$(CPP) -o $@ $(CORE_OBJ) main_3d.o $(LDFLAGS)

$(TARGET_T22): $(CORE_OBJ) main_tarefa22.o Makefile
	$(CPP) -o $@ $(CORE_OBJ) main_tarefa22.o $(LDFLAGS)

$(TARGET_PROJ2): $(CORE_OBJ) main_proj2.o Makefile
	$(CPP) -o $@ $(CORE_OBJ) main_proj2.o $(LDFLAGS)

$(WGPU_STAMP):
	@if [ -d "../../triangle/triangle/C++/deps/wgpu" ]; then \
		echo "Reusing wgpu from triangle/triangle/C++/deps/wgpu..."; \
		mkdir -p $(DEPS_DIR) && cp -r ../../triangle/triangle/C++/deps/wgpu $(DEPS_DIR)/; \
	elif [ -d "../../clock/clock/C++/deps/wgpu" ]; then \
		echo "Reusing wgpu from clock/clock/C++/deps/wgpu..."; \
		mkdir -p $(DEPS_DIR) && cp -r ../../clock/clock/C++/deps/wgpu $(DEPS_DIR)/; \
	else \
		echo "Downloading wgpu-native $(WGPU_VERSION)..."; \
		mkdir -p $(WGPU_DIR) && \
		curl -L -o $(DEPS_DIR)/wgpu.zip https://github.com/gfx-rs/wgpu-native/releases/download/$(WGPU_VERSION)/$(WGPU_ASSET) && \
		unzip -o -q $(DEPS_DIR)/wgpu.zip -d $(WGPU_DIR) && \
		rm -f $(DEPS_DIR)/wgpu.zip; \
	fi
	touch $@

clean:
	rm -f $(TARGET2) $(TARGET3) $(TARGET_T22) $(TARGET_PROJ2) $(CORE_OBJ) $(LUXOR_OBJ) main_2d.o main_3d.o main_tarefa22.o main_proj2.o

distclean: clean
	rm -rf $(DEPS_DIR)
