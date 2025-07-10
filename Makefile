# Compiler and Flags
CXX = g++
# Ensure C++17/C++23 standard is used for <filesystem>
CXXFLAGS = -std=c++17 -Wall -Wextra -g
# Linker flags for audio libraries and JSON (filesystem usually doesn't need explicit linking with modern g++)
LDFLAGS = -lmpg123 -lpulse-simple -lpulse -lFLAC -lvorbisfile -lvorbis -logg -lsndfile

# Source and Object Files
SOURCES = main.cpp link.cpp audio.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Executable Name
EXECUTABLE = music_playlist

# Default target: Build the executable
all: $(EXECUTABLE)

# Rule to link the executable
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(EXECUTABLE) $(LDFLAGS)

# Rule to compile .cpp files into .o files
# Added <filesystem> header dependency implicitly via main.cpp including it
%.o: %.cpp link.h audio.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Target to clean up build files
clean:
	#rm -f $(OBJECTS) $(EXECUTABLE) playlist*.json
	rm -f $(OBJECTS) $(EXECUTABLE)

# Phony targets (not actual files)
.PHONY: all clean