# C++ Music Playlist Manager

A command-line application written in C++ for creating, managing, and playing music playlists using MP3 files. It utilizes `mpg123` for decoding and PulseAudio (via the simple API) for audio output on Linux systems.

## Features

*   **Multiple Playlists:** Manage up to 3 distinct playlists simultaneously.
*   **Playlist Management:**
    *   Create new playlists.
    *   Rename existing playlists.
    *   Delete entire playlists (frees up the slot).
*   **Song Management:**
    *   **Add Songs:**
        *   Add to the beginning, end, or a specific position.
        *   **Case-Insensitive File Finding:** When adding by title, the application searches the `music/` directory case-insensitively for a matching `.mp3` file and uses the correct file path. Handles ambiguous matches (e.g., `Song.mp3` and `song.mp3`).
    *   **Delete Songs:**
        *   Delete from the beginning, end, or a specific position.
*   **Playback:**
    *   Play playlists sequentially from start to end (once through).
    *   Repeat playback of a playlist a specified number of times.
    *   Play playlists in reverse order (once through).
    *   Play a specific song chosen from the list by number.
    *   Includes a text-based progress bar during playback.
*   **Information & Utilities:**
    *   Display playlist contents (song title and artist).
    *   Search for songs within a playlist (case-insensitive substring matching on the title).
    *   Sort playlists by song title or artist name (case-insensitive).
*   **Persistence:**
    *   Save active playlists to individual files (`playlist1.txt`, `playlist2.txt`, `playlist3.txt`).
    *   Load playlists from these files back into available slots upon startup or manually.
*   **User Interface:**
    *   Clear, menu-driven command-line interface.
    *   Input validation for menu choices and positions.

## Prerequisites

*   **C++ Compiler:** A compiler supporting C++17 standard (required for `<filesystem>`). `g++` or `clang++` are recommended.
*   **Make:** The `make` build utility.
*   **mpg123 Library (Development Files):**
    *   Debian/Ubuntu: `sudo apt-get update && sudo apt-get install libmpg123-dev`
    *   Fedora: `sudo dnf install mpg123-devel`
    *   Arch: `sudo pacman -S mpg123`
*   **PulseAudio Library (Development Files):**
    *   Debian/Ubuntu: `sudo apt-get install libpulse-dev`
    *   Fedora: `sudo dnf install pulseaudio-libs-devel`
    *   Arch: `sudo pacman -S pulseaudio`

## Building the Project

1.  **Save Files:** Ensure all source files (`link.h`, `link.cpp`, `audio.h`, `audio.cpp`, `main.cpp`) and the `Makefile` are in the same directory.
2.  **Open Terminal:** Navigate to that directory in your terminal.
3.  **Compile:** Run the `make` command:
    ```bash
    make
    ```
    This will compile the source files and create an executable named `music_playlist`.

## Running the Application

1.  **Create `music/` Directory:** Before running, create a directory named `music` in the *same directory* as the `music_playlist` executable.
    ```bash
    mkdir music
    ```
2.  **Add MP3 Files:** Place your `.mp3` files inside the newly created `music/` directory.
3.  **Execute:** Run the compiled program from the terminal:
    ```bash
    ./music_playlist
    ```

## Usage

The application presents a text-based menu system.

1.  **Main Menu:** Provides options to:
    *   **Create New Playlist:** Creates a list in the next available slot (up to 3). Prompts for a name and allows adding initial songs.
    *   **Manage Playlists:** Lists the currently active playlists and lets you choose one to enter the Manage List sub-menu.
    *   **Save Active Playlists:** Saves the state of all active playlists to their corresponding `playlistN.txt` files.
    *   **Load Playlists from Files:** Attempts to load `playlistN.txt` files into any *available* slots.
    *   **Exit:** Quits the application.
2.  **Manage List Sub-Menu:** Once a playlist is selected for management, this menu appears with options to:
    *   Add/Delete songs in various ways.
    *   Display the current list.
    *   Play the list (Sequentially, Repeat, Reverse, Specific Song).
    *   Search, Sort, Rename, or Delete the entire list.
    *   Go back to the Main Menu.

Follow the on-screen prompts and enter the corresponding numbers to navigate and perform actions. When adding songs, enter the song title *without* the `.mp3` extension.

## File Structure

*   `link.h` / `link.cpp`: Defines and implements the `LinkedList` and `Stack` data structures, along with node structures and utility functions (`getCleanSongName`, comparisons).
*   `audio.h` / `audio.cpp`: Declares and implements audio playback functions (`player`, `repeat`, `reverse`, etc.) using `mpg123` and `pulseaudio`.
*   `main.cpp`: Contains the main application logic, menu system (`MenuUI` class), user interaction handlers, and global playlist management.
*   `Makefile`: Used to compile the project easily.
*   `music/`: (User-created directory) Stores the `.mp3` files to be used.
*   `playlistN.txt`: (Generated on save) Stores the data for playlist in slot N.

## Dependencies

*   **mpg123:** For MP3 decoding.
*   **PulseAudio Simple API:** For audio output.
*   **C++17 Standard Library:** Specifically uses `<filesystem>`, `<string>`, `<vector>`, `<iostream>`, `<fstream>`, `<iomanip>`, `<limits>`, `<algorithm>`, `<cctype>`, etc.

## Known Issues / Limitations

*   **Fixed Playlist Limit:** The application currently supports a maximum of 3 playlists.
*   **Platform Dependency:** Relies heavily on PulseAudio for output, making it primarily Linux-focused. `system("clear")` is also POSIX-specific.
*   **Error Handling:** Basic error handling is implemented, but could be more robust (e.g., handling corrupted MP3s gracefully, more detailed file I/O errors).
*   **Audio Formats:** Only supports MP3 files due to using `mpg123`.
*   **Metadata:** Does not read or utilize ID3 tags (artist/title are entered manually).
*   **Sorting Algorithm:** Uses Bubble Sort, which can be inefficient for very large playlists.
*   **File Search:** Case-insensitive search works but requires unique base filenames (ignoring case) in the `music/` directory to avoid ambiguity errors when adding.
*   **`music/` Directory:** The location is hardcoded relative to the executable.

## Future Enhancements Ideas

*   Dynamic number of playlists.
*   Support for other audio formats (e.g., Ogg Vorbis, FLAC using different libraries).
*   Reading ID3 tags for automatic artist/title population.
*   Shuffle play mode.
*   More advanced search/filtering options.
*   Volume control.
*   Improved error reporting and recovery.
*   Cross-platform audio output layer (e.g., using SDL_mixer, PortAudio).
*   Configuration file for settings (like music directory path).
*   More efficient sorting algorithms (e.g., Merge Sort).

## License

(Optional: Add a license here, e.g., MIT)

This project is licensed under the MIT License - see the LICENSE.md file for details (if you choose to add one).

*(If no license is intended, you can state "No license specified" or remove this section)*