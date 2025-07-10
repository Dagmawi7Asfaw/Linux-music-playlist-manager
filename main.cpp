#include <algorithm>   // For std::transform
#include <cctype>      // For toupper, tolower
#include <cstdlib>     // For system() for clear screen
#include <filesystem>  // For directory searching (requires C++17)
#include <iomanip>     // For setw
#include <limits>      // For numeric_limits
#include <vector>      // For storing playlists vector and active indices

#include "audio.h"  // Includes link.h again (harmless), declares playback functions
#include "link.h"  // Includes string, iostream, utilities, iomanip etc.

// Use std namespace to reduce typing, or qualify everything with std::
using namespace std;
namespace fs = std::filesystem;  // Alias for convenience

// Forward declarations
void printList(LinkedList& li);

// --- Enums for Menu Options ---
enum class MainMenuOption {
  CREATE_LIST = 1,
  MANAGE_LISTS = 2,
  SAVE_LISTS = 3,
  LOAD_LISTS = 4,
  EXIT = 5
};

enum class ManageMenuOption {
  // Playlist Management Section
  DISPLAY = 1,
  RENAME = 2,
  DELETE_LIST = 3,

  // Song Management Section
  ADD_BEGINNING = 4,
  ADD_END = 5,
  ADD_MIDDLE = 6,
  DEL_BEGINNING = 7,
  DEL_END = 8,
  DEL_MIDDLE = 9,

  // Playback Section
  PLAY_SONG = 10,
  PLAY_SEQUENTIAL = 11,
  PLAY_REPEAT = 12,
  PLAY_REVERSE = 13,

  // Organization Section
  SEARCH = 14,
  SORT = 15,

  // Navigation
  BACK = 16
};

enum class SortOption { BY_SONG = 1, BY_ARTIST = 2 };

// --- Global Variables ---
bool shouldExitProgram = false;        // Controls the main application loop
std::vector<LinkedList> playlists(3);  // Holds up to 3 playlist objects

// --- Menu UI Class ---
class MenuUI {
private:
  // Clears the console screen (platform-dependent)
  static void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    // Assume POSIX-compatible system (Linux, macOS)
    system("clear");
#endif
  }

  // Draws a horizontal line using a specified symbol
  static void drawLine(int length = 60, const string& symbol = "━") {
    cout << DOUBLE_TAB;
    for (int i = 0; i < length; ++i) cout << symbol;
    cout << endl;
  }

public:
  // Constant for consistent indentation
  static const string DOUBLE_TAB;

  // Displays the main application header and a specific menu title
  static void displayHeader(const string& title) {
    clearScreen();
    cout << endl;
    cout << "\t\t"
         << "╔══════════════════════════════════════════════════════════╗"
         << endl;
    cout << "\t\t"
         << "║                                                          ║"
         << endl;
    cout << "\t\t"
         << "║                 🎵 MUSIC PLAYLIST MANAGER 🎵              ║"
         << endl;
    cout << "\t\t"
         << "║                                                          ║"
         << endl;
    cout << "\t\t"
         << "╚══════════════════════════════════════════════════════════╝"
         << endl;
    cout << endl;
    cout << "\t\t"
         << "┌──────────────────────────────────────────────────────────┐"
         << endl;

    // Safely handle the title to prevent corruption
    string safeTitle = title;
    if (safeTitle.find('\0') != string::npos) {
      safeTitle = "[Corrupted Title]";
    } else if (safeTitle.length() > 58) {
      safeTitle = safeTitle.substr(0, 55) + "...";
    }

    cout << "\t\t" << "│ " << left << setw(58) << safeTitle << " │" << endl;
    cout << "\t\t"
         << "└──────────────────────────────────────────────────────────┘"
         << endl;
    cout << endl;
  }

  // Gets validated numeric input from the user within a specified range
  template <typename T>
  static T getValidatedInput(const string& prompt, T minVal, T maxVal) {
    T choice;
    while (true) {
      cout << prompt;
      // Try reading input of type T
      if (cin >> choice && choice >= minVal && choice <= maxVal) {
        // Input is valid and within range
        cin.ignore(numeric_limits<streamsize>::max(),
                   '\n');  // Consume rest of line (like Enter)
        return choice;
      } else {
        // Input failed (wrong type) or was out of range
        cout << DOUBLE_TAB << "⚠️ Invalid input. Please enter a value between "
             << minVal << " and " << maxVal << "." << endl;
        cin.clear();  // Clear error flags (like failbit)
        cin.ignore(numeric_limits<streamsize>::max(),
                   '\n');  // Discard bad input from buffer
      }
    }
  }

  // Convenience overload for integer input (common case)
  static int getValidatedInput(int min, int max) {
    string prompt = DOUBLE_TAB + "📌 Enter choice (" + to_string(min) + "-" +
                    to_string(max) + "): ";
    return getValidatedInput<int>(prompt, min, max);
  }

  // Pauses execution until the user presses Enter
  static void pressEnterToContinue() {
    cout << endl;
    cout << DOUBLE_TAB << "Press Enter to continue...";
    // cin.ignore might have already cleared buffer from previous
    // getValidatedInput cin.get() waits specifically for the next Enter press
    cin.get();
  }

  // Displays a success message
  static void displaySuccess(const string& message) {
    cout << DOUBLE_TAB << "✅ " << message << endl;
  }

  // Displays an error message
  static void displayError(const string& message) {
    cout << DOUBLE_TAB << "❌ " << message << endl;
  }

  // Displays an informational message
  static void displayInfo(const string& message) {
    cout << DOUBLE_TAB << "ℹ️ " << message << endl;
  }
};

// Definition of the static const member
const string MenuUI::DOUBLE_TAB = "\t\t";

// --- Utility Functions ---

// Helper function to safely get playlist name for display
std::string getSafePlaylistName(const std::string& name, int listIndex) {
  if (name.empty()) {
    return "[Unnamed List " + std::to_string(listIndex + 1) + "]";
  }

  // Check for null bytes or excessive length
  if (name.find('\0') != std::string::npos) {
    return "[Corrupted List " + std::to_string(listIndex + 1) + "]";
  }

  if (name.length() > 100) {
    return name.substr(0, 97) + "...";
  }

  return name;
}

// Function to ensure the music directory exists, creating it if necessary
void ensureMusicDirectoryExists() {
  const std::string musicDir = "music";
  if (!fs::exists(musicDir)) {
    try {
      fs::create_directory(musicDir);
      MenuUI::displayInfo(
          "Created 'music' directory. Please place your audio files inside.");
    } catch (const fs::filesystem_error& e) {
      MenuUI::displayError("Failed to create music directory: " +
                           std::string(e.what()));
      MenuUI::displayInfo(
          "You can manually create a 'music' folder in the same location as "
          "the executable.");
    }
  } else if (!fs::is_directory(musicDir)) {
    MenuUI::displayError(
        "'music' exists but is not a directory. Please remove this file and "
        "restart.");
    MenuUI::displayInfo(
        "The program expects a 'music' directory to store audio files.");
  }
}

// Basic check for invalid characters in a filename *part* (not the whole path)
bool isValidNamePart(const string& name) {
  if (name.empty() || name == "." || name == "..") return false;
  // Disallow common problematic characters for filenames across OSes
  const string invalidChars = "\\/:*?\"<>|";
  // Check if any invalid character is present
  if (name.find_first_of(invalidChars) != string::npos) {
    return false;
  }
  // Optional: Add checks for reserved names (CON, PRN etc. on Windows) if
  // needed Optional: Check if name ends with space or period (problematic on
  // Windows)
  return true;
}

// --- Helper Function for Case-Insensitive File Search ---
// Tries to find a file in 'dirPath' whose name (without extension) matches
// 'baseName' case-insensitively. Returns the full path with correct casing if
// found, otherwise returns an empty string.
string findFileCaseInsensitive(const string& dirPath, const string& baseName) {
  // Ensure the music directory exists
  if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
    // If specifically looking in the music directory, try to create it
    if (dirPath == "music") {
      ensureMusicDirectoryExists();
      // Check again after attempted creation
      if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return "";  // Directory still doesn't exist
      }
    } else {
      // Display error only once if directory is missing, not every time
      static bool dirErrorDisplayed = false;
      if (!dirErrorDisplayed) {
        MenuUI::displayError("Directory '" + dirPath +
                             "' not found or is not a directory!");
        MenuUI::displayInfo("Please create the '" + dirPath +
                            "' directory next to the executable.");
        dirErrorDisplayed = true;
      }
      return "";  // Directory doesn't exist
    }
  }

  // Prepare lowercase version of the name we're searching for
  string lowerBaseName;
  lowerBaseName.reserve(baseName.size());
  transform(baseName.begin(), baseName.end(), back_inserter(lowerBaseName),
            [](unsigned char c) { return std::tolower(c); });

  vector<fs::path> matches;  // Store paths of files that match

  try {
    // Iterate through the specified directory
    for (const auto& entry : fs::directory_iterator(dirPath)) {
      // Consider only regular files
      if (entry.is_regular_file()) {
        fs::path currentPath = entry.path();
        string currentStem =
            currentPath.stem().string();  // Filename without extension
        string currentExt = currentPath.extension().string();  // File extension

        // Check if it's a common audio extension (case-insensitive check)
        string lowerExt;
        transform(currentExt.begin(), currentExt.end(), back_inserter(lowerExt),
                  ::tolower);
        if (lowerExt == ".mp3" || lowerExt == ".wav" || lowerExt == ".flac" ||
            lowerExt == ".ogg") {
          // Prepare lowercase version of the current file's stem
          string lowerCurrentStem;
          lowerCurrentStem.reserve(currentStem.size());
          transform(currentStem.begin(), currentStem.end(),
                    back_inserter(lowerCurrentStem),
                    [](unsigned char c) { return std::tolower(c); });

          // Compare lowercase stem with lowercase search name
          if (lowerCurrentStem == lowerBaseName) {
            matches.push_back(currentPath);  // Add to list of matches
          }
        }
      }
    }
  } catch (const fs::filesystem_error& e) {
    // Handle potential errors during directory iteration (e.g., permissions)
    MenuUI::displayError("Filesystem error accessing '" + dirPath +
                         "': " + e.what());
    return "";
  }

  // Process the results
  if (matches.empty()) {
    return "";  // No match found
  } else if (matches.size() == 1) {
    return matches[0]
        .string();  // Exactly one match, return its full path string
  } else {
    // Multiple files matched case-insensitively
    MenuUI::displayError("Ambiguous song title! Multiple files match '" +
                         baseName + "' (case-insensitive):");
    // List the conflicting filenames
    for (size_t i = 0; i < matches.size(); ++i) {
      cout << MenuUI::DOUBLE_TAB << (i + 1) << ". "
           << matches[i].filename().string() << endl;
    }
    MenuUI::displayError(
        "Please ensure unique filenames (ignoring case and extension) in the "
        "'" +
        dirPath + "' directory, or use a more specific title.");
    return "";  // Indicate ambiguity error by returning empty string
  }
}

// --- Handler Functions for Menu Actions ---

// Modified handler for adding songs with file browser
void handleAddSong(LinkedList& li, bool addToBeginning) {
  std::string currentPath = "music";
  std::vector<std::string> breadcrumbs = {"music"};
  bool exitBrowser = false;

  while (!exitBrowser) {
    // Get subdirectories and music files from current directory
    auto subdirectories = getSubdirectories(currentPath);
    auto musicFiles = getMusicFiles(currentPath);

    // Display header with breadcrumb trail
    std::string breadcrumbTrail = "";
    for (size_t i = 0; i < breadcrumbs.size(); ++i) {
      breadcrumbTrail += breadcrumbs[i];
      if (i < breadcrumbs.size() - 1) {
        breadcrumbTrail += " > ";
      }
    }

    MenuUI::displayHeader("Browse Music: " + breadcrumbTrail);

    // Display navigation options
    std::cout << MenuUI::DOUBLE_TAB
              << "┌─ NAVIGATION ─────────────────────────────────┐"
              << std::endl;
    std::cout << MenuUI::DOUBLE_TAB
              << "│ 0. Cancel                                    │"
              << std::endl;

    if (breadcrumbs.size() > 1) {
      std::cout << MenuUI::DOUBLE_TAB
                << "│ B. Go back to parent directory               │"
                << std::endl;
    }

    // Add option to add all files from current directory if files exist
    if (!musicFiles.empty()) {
      std::cout << MenuUI::DOUBLE_TAB
                << "│ A. Add all files from current directory      │"
                << std::endl;
      std::cout << MenuUI::DOUBLE_TAB
                << "│ S. Select multiple files (comma separated)   │"
                << std::endl;
    }

    std::cout << MenuUI::DOUBLE_TAB
              << "└──────────────────────────────────────────────┘"
              << std::endl;
    std::cout << std::endl;

    // Display directories if available
    if (!subdirectories.empty()) {
      std::cout << MenuUI::DOUBLE_TAB
                << "┌─ DIRECTORIES ────────────────────────────────┐"
                << std::endl;
      int dirIndex = 1;
      for (const auto& dir : subdirectories) {
        std::string displayName = dir.second;

        // Format for display: limit length and add ellipsis if too long
        const int maxDisplayLength = 45;
        if (displayName.length() > maxDisplayLength) {
          displayName = displayName.substr(0, maxDisplayLength - 3) + "...";
        }

        std::cout << MenuUI::DOUBLE_TAB << "│ D" << std::left << std::setw(2)
                  << dirIndex++ << ". " << std::setw(45) << displayName << " │"
                  << std::endl;
      }
      std::cout << MenuUI::DOUBLE_TAB
                << "└──────────────────────────────────────────────┘"
                << std::endl;
      std::cout << std::endl;
    }

    // Display audio files if available
    if (!musicFiles.empty()) {
      std::cout << MenuUI::DOUBLE_TAB
                << "┌─ AUDIO FILES ────────────────────────────────┐"
                << std::endl;
      int fileIndex = 1;
      for (const auto& file : musicFiles) {
        // Get the clean name
        std::string displayName = file.second;

        // Format for display: limit length and add ellipsis if too long
        const int maxDisplayLength = 45;
        if (displayName.length() > maxDisplayLength) {
          displayName = displayName.substr(0, maxDisplayLength - 3) + "...";
        }

        std::cout << MenuUI::DOUBLE_TAB << "│ F" << std::left << std::setw(2)
                  << fileIndex++ << ". " << std::setw(45) << displayName << " │"
                  << std::endl;
      }
      std::cout << MenuUI::DOUBLE_TAB
                << "└──────────────────────────────────────────────┘"
                << std::endl;
    } else if (subdirectories.empty() && musicFiles.empty()) {
      std::cout << MenuUI::DOUBLE_TAB << "📂 This directory is empty."
                << std::endl;
    }

    std::cout << std::endl;

    // Get user input
    std::string choice;
    std::cout << MenuUI::DOUBLE_TAB << "Enter your choice (0 to cancel";
    if (breadcrumbs.size() > 1) std::cout << ", B to go back";
    if (!musicFiles.empty())
      std::cout << ", A to add all, S to select multiple";
    if (!subdirectories.empty()) std::cout << ", D# for directory";
    if (!musicFiles.empty()) std::cout << ", F# for file";
    std::cout << "): ";

    std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(),
                    '\n');  // Clear input buffer

    // Process choice
    if (choice == "0") {
      MenuUI::displayInfo("Song addition canceled.");
      return;
    } else if (choice == "B" || choice == "b") {
      if (breadcrumbs.size() > 1) {
        breadcrumbs.pop_back();            // Remove current directory
        currentPath = breadcrumbs.back();  // Set path to parent directory
      }
    } else if ((choice == "A" || choice == "a") && !musicFiles.empty()) {
      // Add all songs from current directory
      std::string artistName;
      std::cout
          << MenuUI::DOUBLE_TAB
          << "Enter artist name for all songs (leave empty for '[Unknown]'): ";
      getline(std::cin, artistName);

      if (artistName.empty()) {
        artistName = "[Unknown Artist]";
      }

      int addedCount = 0;
      for (const auto& file : musicFiles) {
        if (addToBeginning) {
          li.add_beg(file.first, artistName);
        } else {
          li.add_end(file.first, artistName);
        }
        addedCount++;
      }

      MenuUI::displaySuccess(
          "Added " + std::to_string(addedCount) + " songs from " +
          breadcrumbs.back() + " directory to playlist '" +
          (li.listName.empty() ? "[Unnamed]" : li.listName) + "'.");
      exitBrowser = true;
    } else if ((choice == "S" || choice == "s") && !musicFiles.empty()) {
      // Select multiple files
      std::cout << MenuUI::DOUBLE_TAB
                << "Enter file numbers separated by commas (e.g., 1,3,5): ";
      std::string selections;
      getline(std::cin, selections);

      std::vector<int> selectedIndices;
      std::stringstream ss(selections);
      std::string item;

      // Parse comma-separated list
      while (std::getline(ss, item, ',')) {
        // Trim whitespace from the item
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);

        if (item.empty()) continue;  // Skip empty items

        try {
          int index = std::stoi(item);
          if (index >= 1 && index <= static_cast<int>(musicFiles.size())) {
            selectedIndices.push_back(index);
          } else {
            MenuUI::displayError("Invalid file number: " + item + " (ignored)");
          }
        } catch (const std::exception& e) {
          MenuUI::displayError("Invalid input: " + item + " (ignored)");
        }
      }

      if (selectedIndices.empty()) {
        MenuUI::displayError("No valid file numbers entered.");
        MenuUI::pressEnterToContinue();
        continue;
      }

      // Get artist name
      std::string artistName;
      std::cout << MenuUI::DOUBLE_TAB
                << "Enter artist name for selected songs (leave empty for "
                   "'[Unknown]'): ";
      getline(std::cin, artistName);

      if (artistName.empty()) {
        artistName = "[Unknown]";
      }

      // Add selected songs
      int addedCount = 0;
      for (int index : selectedIndices) {
        const auto& file = musicFiles[index - 1];
        if (addToBeginning) {
          li.add_beg(file.first, artistName);
        } else {
          li.add_end(file.first, artistName);
        }
        addedCount++;
      }

      MenuUI::displaySuccess(
          "Added " + std::to_string(addedCount) + " songs to playlist '" +
          (li.listName.empty() ? "[Unnamed]" : li.listName) + "'.");
      exitBrowser = true;
    } else if (choice.length() >= 2 && (choice[0] == 'D' || choice[0] == 'd') &&
               std::isdigit(choice[1])) {
      // Directory navigation
      int dirNum = std::stoi(choice.substr(1));
      if (dirNum >= 1 && dirNum <= static_cast<int>(subdirectories.size())) {
        currentPath = subdirectories[dirNum - 1].first;  // Full path
        breadcrumbs.push_back(
            subdirectories[dirNum - 1].second);  // Directory name
      } else {
        MenuUI::displayError("Invalid directory number.");
        MenuUI::pressEnterToContinue();
      }
    } else if (choice.length() >= 2 && (choice[0] == 'F' || choice[0] == 'f') &&
               std::isdigit(choice[1])) {
      // File selection
      int fileNum = std::stoi(choice.substr(1));
      if (fileNum >= 1 && fileNum <= static_cast<int>(musicFiles.size())) {
        // Get the selected song
        std::string selectedFilePath = musicFiles[fileNum - 1].first;
        std::string selectedSongName = musicFiles[fileNum - 1].second;

        // Get artist name
        std::string artistName;
        std::cout << MenuUI::DOUBLE_TAB << "Selected: " << selectedSongName
                  << std::endl;
        std::cout << MenuUI::DOUBLE_TAB << "Enter artist name: ";
        getline(std::cin, artistName);

        if (artistName.empty()) {
          MenuUI::displayInfo("Artist name left empty, using '[Unknown]'.");
          artistName = "[Unknown]";
        }

        // Add the song to the playlist
        if (addToBeginning) {
          li.add_beg(selectedFilePath, artistName);
          MenuUI::displaySuccess(
              "Song '" + selectedSongName + "' added to beginning of '" +
              (li.listName.empty() ? "[Unnamed]" : li.listName) + "'.");
        } else {
          li.add_end(selectedFilePath, artistName);
          MenuUI::displaySuccess(
              "Song '" + selectedSongName + "' added to end of '" +
              (li.listName.empty() ? "[Unnamed]" : li.listName) + "'.");
        }
        exitBrowser = true;
      } else {
        MenuUI::displayError("Invalid file number.");
        MenuUI::pressEnterToContinue();
      }
    } else {
      MenuUI::displayError("Invalid choice. Please try again.");
      MenuUI::pressEnterToContinue();
    }
  }
}

// Specific handler for adding to beginning
void handleAddBeginning(LinkedList& li) {
  handleAddSong(li, true);
}
// Specific handler for adding to end
void handleAddEnd(LinkedList& li) {
  handleAddSong(li, false);
}

// Handles sorting the list 'li'
void handleSort(LinkedList& li) {
  if (li.head == nullptr) {
    MenuUI::displayInfo("List '" +
                        (li.listName.empty() ? "[Unnamed]" : li.listName) +
                        "' is empty, nothing to sort.");
    return;
  }
  cout << MenuUI::DOUBLE_TAB << "Sort list '"
       << (li.listName.empty() ? "[Unnamed]" : li.listName) << "' by:" << endl;
  cout << MenuUI::DOUBLE_TAB << "1. Song Title" << endl;
  cout << MenuUI::DOUBLE_TAB << "2. Artist Name" << endl;

  int choiceVal = MenuUI::getValidatedInput(1, 2);
  SortOption choice = static_cast<SortOption>(choiceVal);

  switch (choice) {
    case SortOption::BY_SONG:
      li.sortBySong();
      MenuUI::displaySuccess("List sorted by Song Title.");
      break;
    case SortOption::BY_ARTIST:
      li.sortByArtist();
      MenuUI::displaySuccess("List sorted by Artist Name.");
      break;
  }
}

// Modified handler for adding at a specific position
void handleAddMiddle(LinkedList& li) {
  int currentLen = li.len;
  int position = MenuUI::getValidatedInput<int>(
      MenuUI::DOUBLE_TAB + "Enter position to insert song (1-" +
          std::to_string(currentLen + 1) + "): ",
      1, currentLen + 1);

  // Use the same file browser as handleAddSong, but with position insertion
  std::string currentPath = "music";
  std::vector<std::string> breadcrumbs = {"music"};
  bool exitBrowser = false;

  while (!exitBrowser) {
    // Get subdirectories and music files from current directory
    auto subdirectories = getSubdirectories(currentPath);
    auto musicFiles = getMusicFiles(currentPath);

    // Display header with breadcrumb trail
    std::string breadcrumbTrail = "";
    for (size_t i = 0; i < breadcrumbs.size(); ++i) {
      breadcrumbTrail += breadcrumbs[i];
      if (i < breadcrumbs.size() - 1) {
        breadcrumbTrail += " > ";
      }
    }

    MenuUI::displayHeader("Browse Music: " + breadcrumbTrail);

    // Display navigation options
    std::cout << MenuUI::DOUBLE_TAB
              << "┌─ NAVIGATION ─────────────────────────────────┐"
              << std::endl;
    std::cout << MenuUI::DOUBLE_TAB
              << "│ 0. Cancel                                    │"
              << std::endl;

    if (breadcrumbs.size() > 1) {
      std::cout << MenuUI::DOUBLE_TAB
                << "│ B. Go back to parent directory               │"
                << std::endl;
    }

    std::cout << MenuUI::DOUBLE_TAB
              << "└──────────────────────────────────────────────┘"
              << std::endl;
    std::cout << std::endl;

    // Display directories if available
    if (!subdirectories.empty()) {
      std::cout << MenuUI::DOUBLE_TAB
                << "┌─ DIRECTORIES ────────────────────────────────┐"
                << std::endl;
      int dirIndex = 1;
      for (const auto& dir : subdirectories) {
        std::string displayName = dir.second;

        // Format for display: limit length and add ellipsis if too long
        const int maxDisplayLength = 45;
        if (displayName.length() > maxDisplayLength) {
          displayName = displayName.substr(0, maxDisplayLength - 3) + "...";
        }

        std::cout << MenuUI::DOUBLE_TAB << "│ D" << std::left << std::setw(2)
                  << dirIndex++ << ". " << std::setw(45) << displayName << " │"
                  << std::endl;
      }
      std::cout << MenuUI::DOUBLE_TAB
                << "└──────────────────────────────────────────────┘"
                << std::endl;
      std::cout << std::endl;
    }

    // Display audio files if available
    if (!musicFiles.empty()) {
      std::cout << MenuUI::DOUBLE_TAB
                << "┌─ AUDIO FILES ────────────────────────────────┐"
                << std::endl;
      int fileIndex = 1;
      for (const auto& file : musicFiles) {
        // Get the clean name
        std::string displayName = file.second;

        // Format for display: limit length and add ellipsis if too long
        const int maxDisplayLength = 45;
        if (displayName.length() > maxDisplayLength) {
          displayName = displayName.substr(0, maxDisplayLength - 3) + "...";
        }

        std::cout << MenuUI::DOUBLE_TAB << "│ F" << std::left << std::setw(2)
                  << fileIndex++ << ". " << std::setw(45) << displayName << " │"
                  << std::endl;
      }
      std::cout << MenuUI::DOUBLE_TAB
                << "└──────────────────────────────────────────────┘"
                << std::endl;
    } else if (subdirectories.empty() && musicFiles.empty()) {
      std::cout << MenuUI::DOUBLE_TAB << "📂 This directory is empty."
                << std::endl;
    }

    std::cout << std::endl;

    // Get user input
    std::string choice;
    std::cout << MenuUI::DOUBLE_TAB << "Enter your choice (0 to cancel";
    if (breadcrumbs.size() > 1) std::cout << ", B to go back";
    if (!subdirectories.empty()) std::cout << ", D# for directory";
    if (!musicFiles.empty()) std::cout << ", F# for file";
    std::cout << "): ";

    std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(),
                    '\n');  // Clear input buffer

    // Process choice
    if (choice == "0") {
      MenuUI::displayInfo("Song addition canceled.");
      return;
    } else if (choice == "B" || choice == "b") {
      if (breadcrumbs.size() > 1) {
        breadcrumbs.pop_back();            // Remove current directory
        currentPath = breadcrumbs.back();  // Set path to parent directory
      }
    } else if (choice.length() >= 2 && (choice[0] == 'D' || choice[0] == 'd') &&
               std::isdigit(choice[1])) {
      // Directory navigation
      int dirNum = std::stoi(choice.substr(1));
      if (dirNum >= 1 && dirNum <= static_cast<int>(subdirectories.size())) {
        currentPath = subdirectories[dirNum - 1].first;  // Full path
        breadcrumbs.push_back(
            subdirectories[dirNum - 1].second);  // Directory name
      } else {
        MenuUI::displayError("Invalid directory number.");
        MenuUI::pressEnterToContinue();
      }
    } else if (choice.length() >= 2 && (choice[0] == 'F' || choice[0] == 'f') &&
               std::isdigit(choice[1])) {
      // File selection
      int fileNum = std::stoi(choice.substr(1));
      if (fileNum >= 1 && fileNum <= static_cast<int>(musicFiles.size())) {
        // Get the selected song
        std::string selectedFilePath = musicFiles[fileNum - 1].first;
        std::string selectedSongName = musicFiles[fileNum - 1].second;

        // Get artist name
        std::string artistName;
        std::cout << MenuUI::DOUBLE_TAB << "Selected: " << selectedSongName
                  << std::endl;
        std::cout << MenuUI::DOUBLE_TAB << "Enter artist name: ";
        getline(std::cin, artistName);

        if (artistName.empty()) {
          MenuUI::displayInfo("Artist name left empty, using '[Unknown]'.");
          artistName = "[Unknown]";
        }

        // Add song to the specified position
        li.add_at(selectedFilePath, artistName, position);
        MenuUI::displaySuccess("Song '" + selectedSongName +
                               "' added at position " +
                               std::to_string(position) + ".");
        exitBrowser = true;
      } else {
        MenuUI::displayError("Invalid file number.");
        MenuUI::pressEnterToContinue();
      }
    } else {
      MenuUI::displayError("Invalid choice. Please try again.");
      MenuUI::pressEnterToContinue();
    }
  }
}

// Handles deleting a song from a specific position in list 'li'
void handleDeleteMiddle(LinkedList& li) {
  if (li.head == nullptr) {
    MenuUI::displayError("List is empty. Nothing to delete.");
    return;
  }
  cout << MenuUI::DOUBLE_TAB << "Current list contents:" << endl;
  li.display();
  int position = MenuUI::getValidatedInput<int>(
      MenuUI::DOUBLE_TAB + "Enter position of song to delete (1-" +
          to_string(li.len) + "): ",
      1, li.len);
  li.del_at(position);
  MenuUI::displaySuccess("Song deleted successfully from position " +
                         to_string(position) + ".");
}

// Handles searching for a song in list 'li'
void handleSearch(LinkedList& li) {
  if (li.head == nullptr) {
    MenuUI::displayError("List is empty. Nothing to search.");
    return;
  }
  string searchTerm;
  cout << MenuUI::DOUBLE_TAB
       << "Enter song title (or part of it) to search for: ";
  getline(cin >> ws, searchTerm);
  if (searchTerm.empty()) {
    MenuUI::displayError("Search term cannot be empty.");
    return;
  }
  li.search(searchTerm);
}

// Handles renaming the list 'li'
void handleRename(LinkedList& li) {
  string newName;
  cout << MenuUI::DOUBLE_TAB << "Current list name: "
       << (li.listName.empty() ? "[Unnamed]" : li.listName) << endl;
  cout << MenuUI::DOUBLE_TAB << "Enter new name for the list: ";
  getline(cin >> ws, newName);
  if (newName.empty()) {
    MenuUI::displayError("List name cannot be empty.");
    return;
  }
  if (!isValidNamePart(newName)) {
    MenuUI::displayError(
        "List name contains invalid characters (e.g., \\ / : * ? \" < > | ).");
    return;
  }
  li.listName = newName;
  MenuUI::displaySuccess("List renamed successfully to '" + li.listName + "'.");
}

// Handles deleting the entire list object's content and freeing the slot
void handleDeleteList(LinkedList& li, int listIndex) {
  string currentName = li.listName.empty()
                           ? ("[Unnamed List " + to_string(listIndex + 1) + "]")
                           : li.listName;
  char confirm;
  cout << MenuUI::DOUBLE_TAB
       << "⚠️ WARNING: This will permanently delete all songs" << endl;
  cout << MenuUI::DOUBLE_TAB << "   in the list '" << currentName
       << "' and free up slot " << (listIndex + 1) << "." << endl;
  cout << MenuUI::DOUBLE_TAB << "   Are you sure you want to proceed? (Y/N): ";
  cin >> confirm;
  cin.ignore(numeric_limits<streamsize>::max(), '\n');

  if (toupper(static_cast<unsigned char>(confirm)) == 'Y') {
    li.clear();
    li.listName = "";
    li.taken = false;
    MenuUI::displaySuccess("List '" + currentName +
                           "' deleted successfully. Slot " +
                           to_string(listIndex + 1) + " is now available.");
  } else {
    MenuUI::displayInfo("List deletion cancelled.");
  }
}

// --- Main Menu Flow Functions ---

// Creates a new list in the passed (available) LinkedList object 'li'
void createListObject(LinkedList& li) {
  cout << MenuUI::DOUBLE_TAB << "Enter name for the new playlist: ";
  getline(cin >> ws, li.listName);
  if (li.listName.empty()) {
    li.listName = "[Unnamed]";
    MenuUI::displayInfo("List name set to '[Unnamed]'.");
  } else if (!isValidNamePart(li.listName)) {
    MenuUI::displayError(
        "List name contains invalid characters. Using '[Unnamed]'.");
    li.listName = "[Unnamed]";
  }

  int num_songs = MenuUI::getValidatedInput<int>(
      MenuUI::DOUBLE_TAB + "How many songs to add initially (0-50)? ", 0, 50);

  for (int i = 0; i < num_songs; ++i) {
    cout << endl
         << MenuUI::DOUBLE_TAB << "--- Adding Song " << (i + 1) << " of "
         << num_songs << " ---" << endl;
    handleAddEnd(li);  // Uses file searching handler
  }

  li.taken = true;
  MenuUI::displayHeader("New Playlist Creation Finished");
  MenuUI::displayInfo(
      "List '" + (li.listName.empty() ? "[Unnamed]" : li.listName) +
      "' created with " + to_string(li.len) + " successfully added songs.");
}

// Handles the "Create New Playlist" menu option
void createMenuOption() {
  MenuUI::displayHeader("Create New Playlist");
  int availableSlot = -1;
  for (size_t i = 0; i < playlists.size(); ++i) {
    if (!playlists[i].taken) {
      availableSlot = i;
      break;
    }
  }
  if (availableSlot == -1) {
    MenuUI::displayError("All " + to_string(playlists.size()) +
                         " playlist slots are currently in use.");
    MenuUI::pressEnterToContinue();
    return;
  }
  MenuUI::displayInfo("Creating playlist in available slot #" +
                      to_string(availableSlot + 1) + "...");
  createListObject(playlists[availableSlot]);
  MenuUI::pressEnterToContinue();
}

// Handles the "Manage List" sub-menu for a specifically chosen list 'li'
void manageListMenu(LinkedList& li, int listIndex) {
  bool backToMainMenu = false;
  while (!backToMainMenu) {
    string title =
        "Manage List: " + getSafePlaylistName(li.listName, listIndex);
    MenuUI::displayHeader(title);
    cout << MenuUI::DOUBLE_TAB << "Songs in list: " << li.len << endl << endl;

    // Display Manage Menu Options with organized sections
    // Playlist Management Section
    cout << MenuUI::DOUBLE_TAB << "┌─ PLAYLIST MANAGEMENT ────────────────────┐"
         << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  1. 📋 Display Playlist                   │" << endl;
    cout << MenuUI::DOUBLE_TAB << "│  2. ✏️  Rename Playlist                   │"
         << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  3. 🗑️  Delete Entire Playlist            │" << endl;
    cout << MenuUI::DOUBLE_TAB << "└────────────────────────────────────────┘"
         << endl
         << endl;

    // Song Management Section
    cout << MenuUI::DOUBLE_TAB
         << "┌─ SONG MANAGEMENT ─────────────────────────┐" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  4. ⤴️  Add Song to Beginning              │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  5. ⤵️  Add Song to End                    │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  6. ↩️  Add Song at Position               │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  7. 🗑️  Delete Song from Beginning         │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  8. 🗑️  Delete Song from End               │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│  9. 🗑️  Delete Song at Position            │" << endl;
    cout << MenuUI::DOUBLE_TAB << "└────────────────────────────────────────┘"
         << endl
         << endl;

    // Playback Section (all with controls)
    cout << MenuUI::DOUBLE_TAB
         << "┌─ PLAYBACK (WITH CONTROLS) ─────────────────┐" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│ 10. 🎵 Play Specific Song                   │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│ 11. ▶️  Play Sequentially                   │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│ 12. 🔁 Play with Repeat...                  │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│ 13. ◀️  Play in Reverse                     │" << endl;
    cout << MenuUI::DOUBLE_TAB << "└────────────────────────────────────────┘"
         << endl
         << endl;

    // Organization Section
    cout << MenuUI::DOUBLE_TAB << "┌─ ORGANIZATION ───────────────────────────┐"
         << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│ 14. 🔍 Search for Song                    │" << endl;
    cout << MenuUI::DOUBLE_TAB
         << "│ 15. 🔤 Sort Playlist                      │" << endl;
    cout << MenuUI::DOUBLE_TAB << "└────────────────────────────────────────┘"
         << endl
         << endl;

    // Navigation
    cout << MenuUI::DOUBLE_TAB << "┌─ NAVIGATION ─────────────────────────────┐"
         << endl;
    cout << MenuUI::DOUBLE_TAB << "│ 16. ↩️  Back to Main Menu                 │"
         << endl;
    cout << MenuUI::DOUBLE_TAB << "└────────────────────────────────────────┘"
         << endl;

    int choiceVal = MenuUI::getValidatedInput(1, 16);
    ManageMenuOption choice = static_cast<ManageMenuOption>(choiceVal);
    bool requiresPause = true;

    switch (choice) {
      // Playlist Management
      case ManageMenuOption::DISPLAY:
        cout << endl;
        printList(li);
        break;
      case ManageMenuOption::RENAME:
        handleRename(li);
        break;
      case ManageMenuOption::DELETE_LIST:
        handleDeleteList(li, listIndex);
        if (!li.taken) {
          backToMainMenu = true;
        }
        requiresPause = false;
        break;

      // Song Management
      case ManageMenuOption::ADD_BEGINNING:
        handleAddBeginning(li);
        break;
      case ManageMenuOption::ADD_END:
        handleAddEnd(li);
        break;
      case ManageMenuOption::ADD_MIDDLE:
        handleAddMiddle(li);
        break;
      case ManageMenuOption::DEL_BEGINNING:
        if (li.head) {
          li.del_beg();
          MenuUI::displaySuccess("Deleted song from beginning.");
        } else
          MenuUI::displayError("List is already empty.");
        break;
      case ManageMenuOption::DEL_END:
        if (li.head) {
          li.del_end();
          MenuUI::displaySuccess("Deleted song from end.");
        } else
          MenuUI::displayError("List is already empty.");
        break;
      case ManageMenuOption::DEL_MIDDLE:
        handleDeleteMiddle(li);
        break;

      // Playback - All use controls now
      case ManageMenuOption::PLAY_SONG:
        if (li.head == nullptr) {
          MenuUI::displayError("List is empty.");
          break;
        }
        MenuUI::displayHeader("Play Specific Song: " + li.listName);
        playWithControlsSingle(li);
        requiresPause = false;
        break;
      case ManageMenuOption::PLAY_SEQUENTIAL:
        if (li.head == nullptr) {
          MenuUI::displayError("List is empty.");
          break;
        }
        MenuUI::displayHeader("Play List Sequentially: " + li.listName);
        playWithControls(li);
        requiresPause = false;
        break;
      case ManageMenuOption::PLAY_REPEAT: {
        if (li.head == nullptr) {
          MenuUI::displayError("List is empty.");
          break;
        }
        int rounds = MenuUI::getValidatedInput<int>(
            MenuUI::DOUBLE_TAB + "Enter number of times to repeat (1-10): ", 1,
            10);
        MenuUI::displayHeader("Play List (Repeat " + to_string(rounds) +
                              " times): " + li.listName);
        playWithControlsRepeat(li, rounds);
        requiresPause = false;
      } break;
      case ManageMenuOption::PLAY_REVERSE:
        if (li.head == nullptr) {
          MenuUI::displayError("List is empty.");
          break;
        }
        MenuUI::displayHeader("Play List (Reverse): " + li.listName);
        playWithControlsReverse(li);
        requiresPause = false;
        break;

      // Organization
      case ManageMenuOption::SEARCH:
        handleSearch(li);
        break;
      case ManageMenuOption::SORT:
        handleSort(li);
        break;

      // Navigation
      case ManageMenuOption::BACK:
        backToMainMenu = true;
        requiresPause = false;
        break;
    }

    if (requiresPause && !backToMainMenu) {
      MenuUI::pressEnterToContinue();
    }
  }
}

// Handles the "Manage Playlists" menu option (shows active lists for selection)
void manageCoordinatingMenu() {
  MenuUI::displayHeader("Manage Playlists");
  vector<int> activeListIndices;
  for (size_t i = 0; i < playlists.size(); ++i) {
    if (playlists[i].taken) {
      activeListIndices.push_back(i);
    }
  }
  if (activeListIndices.empty()) {
    MenuUI::displayInfo(
        "No playlists available to manage. Please create or load one first.");
    MenuUI::pressEnterToContinue();
    return;
  }
  cout << MenuUI::DOUBLE_TAB << "Select a list to manage:" << endl;
  cout << MenuUI::DOUBLE_TAB << "------------------------------------" << endl;
  for (size_t i = 0; i < activeListIndices.size(); ++i) {
    int actualIndex = activeListIndices[i];
    cout << MenuUI::DOUBLE_TAB << (i + 1) << ". "
         << getSafePlaylistName(playlists[actualIndex].listName, actualIndex)
         << " (" << playlists[actualIndex].len << " songs)" << endl;
  }
  cout << MenuUI::DOUBLE_TAB << "------------------------------------" << endl;
  int choice =
      MenuUI::getValidatedInput(1, static_cast<int>(activeListIndices.size()));
  int selectedListIndex = activeListIndices[choice - 1];
  manageListMenu(playlists[selectedListIndex], selectedListIndex);
}

// Handles the "Save Playlists" menu option
void handleSaveLists() {
  MenuUI::displayHeader("Save Active Playlists");
  int savedCount = 0;
  int activeCount = 0;
  for (size_t i = 0; i < playlists.size(); ++i) {
    if (playlists[i].taken) {
      activeCount++;
      string filename = "playlist" + to_string(i + 1) + ".json";
      string listDisplayName = playlists[i].listName.empty()
                                   ? ("[Unnamed List " + to_string(i + 1) + "]")
                                   : playlists[i].listName;
      MenuUI::displayInfo("Attempting to save '" + listDisplayName + "' to '" +
                          filename + "'...");
      if (playlists[i].saveToFile(filename)) {
        MenuUI::displaySuccess("Saved '" + listDisplayName + "' successfully.");
        savedCount++;
      } else {
        MenuUI::displayError("Failed to save '" + listDisplayName + "'.");
      }
    }
  }
  if (activeCount == 0) {
    MenuUI::displayInfo("No active playlists to save.");
  } else {
    MenuUI::displayInfo(to_string(savedCount) + "/" + to_string(activeCount) +
                        " active list(s) saved.");
  }
  MenuUI::pressEnterToContinue();
}

// Handles the "Load Playlists" menu option
void handleLoadLists() {
  MenuUI::displayHeader("Load Playlists From Files");

  MenuUI::displayInfo(
      "Attempting to load playlist1.json, playlist2.json, etc., into available "
      "slots.");
  int loadedCount = 0;
  int attemptedLoads = 0;
  for (size_t i = 0; i < playlists.size(); ++i) {
    string filename =
        "playlist" + to_string(i + 1) + ".json";  // Changed from .txt to .json
    if (!playlists[i].taken) {
      attemptedLoads++;
      MenuUI::displayInfo("Checking slot " + to_string(i + 1) +
                          " (free) for file '" + filename + "'...");
      if (playlists[i].loadFromFile(filename)) {
        string loadedName =
            playlists[i].listName.empty() ? "[Unnamed]" : playlists[i].listName;
        MenuUI::displaySuccess("Loaded '" + loadedName + "' (" +
                               to_string(playlists[i].len) +
                               " songs) into slot " + to_string(i + 1) + ".");
        loadedCount++;
      } else {
        MenuUI::displayInfo("Could not load '" + filename + "' into slot " +
                            to_string(i + 1) +
                            ". (File not found or invalid).");
      }
    } else {
      MenuUI::displayInfo("Slot " + to_string(i + 1) + " occupied by '" +
                          (playlists[i].listName.empty()
                               ? "[Unnamed]"
                               : playlists[i].listName) +
                          "'. Skipping load.");
    }
  }
  if (attemptedLoads == 0) {
    MenuUI::displayInfo("All playlist slots are full.");
  } else if (loadedCount == 0) {
    MenuUI::displayInfo(
        "No playlists loaded. Check if .json files exist/are valid.");
  } else {
    MenuUI::displayInfo(to_string(loadedCount) +
                        " list(s) loaded successfully.");
  }
  MenuUI::pressEnterToContinue();
}

// --- Main Application Loop ---

// Displays the main menu, gets user choice, and calls appropriate handler.
// Returns true if the user chose to Exit, false otherwise.
bool displayMainMenu() {
  MenuUI::displayHeader("Main Menu");
  // Display Menu Options
  cout << MenuUI::DOUBLE_TAB << "┌─────────────────────────────────────────┐"
       << endl;
  cout << MenuUI::DOUBLE_TAB << "│  1. 📝 Create New Playlist              │"
       << endl;
  cout << MenuUI::DOUBLE_TAB << "│  2. 🎛️ Manage Playlists                 │"
       << endl;
  cout << MenuUI::DOUBLE_TAB << "│  3. 💾 Save Active Playlists            │"
       << endl;
  cout << MenuUI::DOUBLE_TAB << "│  4. 📂 Load Playlists from Files        │"
       << endl;
  cout << MenuUI::DOUBLE_TAB << "│  5. 🚪 Exit                             │"
       << endl;
  cout << MenuUI::DOUBLE_TAB << "└─────────────────────────────────────────┘"
       << endl;
  cout << endl;
  // Display Playlist Status
  int activeCount = 0;
  cout << MenuUI::DOUBLE_TAB << "Playlist Slots Status:" << endl;
  for (size_t i = 0; i < playlists.size(); ++i) {
    cout << MenuUI::DOUBLE_TAB << "  Slot " << (i + 1) << ": ";
    if (playlists[i].taken) {
      cout << "Active - '" << getSafePlaylistName(playlists[i].listName, i)
           << "' (" << playlists[i].len << " songs)";
      activeCount++;
    } else {
      cout << "Available";
    }
    cout << endl;
  }
  cout << MenuUI::DOUBLE_TAB << "Total Active: " << activeCount << "/"
       << playlists.size() << endl
       << endl;
  // Get Input
  int choiceVal = MenuUI::getValidatedInput(1, 5);
  MainMenuOption choice = static_cast<MainMenuOption>(choiceVal);
  bool shouldExit = false;
  // Process Choice
  switch (choice) {
    case MainMenuOption::CREATE_LIST:
      createMenuOption();
      break;
    case MainMenuOption::MANAGE_LISTS:
      manageCoordinatingMenu();
      break;
    case MainMenuOption::SAVE_LISTS:
      handleSaveLists();
      break;
    case MainMenuOption::LOAD_LISTS:
      handleLoadLists();
      break;
    case MainMenuOption::EXIT:
      cout << endl
           << MenuUI::DOUBLE_TAB << "Exiting... Goodbye! 👋" << endl
           << endl;
      shouldExit = true;
      break;
  }
  return shouldExit;
}

// --- Program Entry Point ---
int main() {
  // Ensure music directory exists
  ensureMusicDirectoryExists();

  while (!shouldExitProgram) {
    shouldExitProgram = displayMainMenu();
  }

  // Clean up before exit
  for (auto& playlist : playlists) {
    if (playlist.taken) {
      playlist.clear();
    }
  }

  return 0;
}

void printList(LinkedList& li) {
  if (li.isEmpty()) {
    MenuUI::displayInfo("Playlist is empty.");
    return;
  }

  node* temp = li.head;
  int songCount = 0;

  MenuUI::displayHeader("Playlist: " + li.listName);

  std::cout << MenuUI::DOUBLE_TAB
            << "┌─ PLAYLIST CONTENTS ─────────────────────────────────────────┐"
            << std::endl;

  do {
    // Get clean song name
    std::string songName = getCleanSongName(temp->song);

    // Get artist (already stored in the node)
    std::string artistName = temp->artist;

    // Format display string
    const int maxSongLength = 30;
    const int maxArtistLength = 15;

    if (songName.length() > maxSongLength) {
      songName = songName.substr(0, maxSongLength - 3) + "...";
    }

    if (artistName.length() > maxArtistLength) {
      artistName = artistName.substr(0, maxArtistLength - 3) + "...";
    }

    // Format output with song name and artist
    std::cout << MenuUI::DOUBLE_TAB << "│ " << std::left << std::setw(3)
              << (++songCount) << ". " << std::setw(maxSongLength) << songName
              << " - " << std::setw(maxArtistLength) << artistName << " │"
              << std::endl;
    temp = temp->next;
  } while (temp != li.head);

  std::cout << MenuUI::DOUBLE_TAB
            << "└───────────────────────────────────────────────────────────┘"
            << std::endl;
  std::cout << MenuUI::DOUBLE_TAB << "Total songs: " << songCount << std::endl;
}