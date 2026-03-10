CXX = g++
CXXFLAGS = -O3 -Wall -pthread `pkg-config --cflags opencv4`

INCLUDES = -Isrc/model -Isrc/viewmodel -Isrc/view -I/usr/local/cuda-10.2/include -I/usr/include/aarch64-linux-gnu

LIBS = -L/usr/local/cuda-10.2/lib64 `pkg-config --cflags --libs opencv4` -lnvinfer -lcudart

SRCS = src/main.cpp src/model/vision_engine.cpp src/viewmodel/boxing_logic.cpp src/view/ui_render.cpp src/model/hardware_sensors.cpp

# Add Obj folder: Map src/ paths to obj/ paths (src/main.cpp to obj/main.o)
OBJS = $(patsubst src/%.cpp, obj/%.o, $(SRCS))
TARGET = bin/ai_coach

# Build Rules
all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	@echo "Linking everything into $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean now removes entire obj/ folder
clean:
	@echo "Cleaning up..."
	rm -rf obj $(TARGET)

run: $(TARGET)
	@echo "Starting the AI Coach..."
	./$(TARGET)
