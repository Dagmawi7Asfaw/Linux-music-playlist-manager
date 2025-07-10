#ifndef LINK_H
#define LINK_H

#include <algorithm>   // Needed for std::equal, std::swap, std::transform
#include <cctype>      // Needed for ::tolower
#include <filesystem>  // For directory operations
#include <iostream>
#include <regex>  // For regex operations
#include <string>
#include <vector>  // For storing file lists

// For convenience
namespace fs = std::filesystem;

// --- Node Structure ---
struct node {
  std::string song;    // Full path to the song file
  std::string artist;  // Artist name
  node* next;          // Pointer to the next node in the circular list

  // Constructor for convenient node creation
  node(const std::string& s = "", const std::string& a = "", node* n = nullptr)
      : song(s), artist(a), next(n) {}
};

// --- LinkedList Class ---
class LinkedList {
public:        // Data members kept public as per original design for simplicity
  node* head;  // Pointer to the first node (or nullptr if empty)
  std::string listName;  // Name of the playlist
  int len;               // Number of songs currently in the list
  bool taken;            // Flag indicating if this list slot is in use

  // --- Constructor & Destructor ---
  LinkedList();   // Default constructor
  ~LinkedList();  // Destructor (cleans up nodes)

  // --- Modification Methods ---
  void add_beg(const std::string& song, const std::string& artist);
  void add_end(const std::string& song, const std::string& artist);
  void add_at(const std::string& song, const std::string& artist, int pos);
  void del_beg();
  void del_end();
  void del_at(int pos);
  void clear();  // Removes all songs from the list

  // --- Information & Utility Methods ---
  bool isEmpty() const;  // Checks if the list is empty
  void display()
      const;  // Displays the list content (const indicates no modification)
  void search(
      const std::string& searchTerm) const;  // Searches for a song (const)

  // --- Sorting Methods ---
  void sortBySong();    // Sorts by song title (case-insensitive)
  void sortByArtist();  // Sorts by artist name (case-insensitive)

  // --- Persistence Methods ---
  bool saveToFile(
      const std::string& filename) const;          // Saves list to file (const)
  bool loadFromFile(const std::string& filename);  // Loads list from file

private:
  // Helper to swap data between two nodes (used by sorting)
  void swapNodesData(node* a, node* b);
};

// --- Stack Node Structure (for reverse playback) ---
struct stackNode {
  node* item;       // Pointer to a node *within* a LinkedList
  stackNode* next;  // Pointer to the next node in the stack

  // Constructor
  stackNode(node* i = nullptr, stackNode* n = nullptr) : item(i), next(n) {}
};

// --- Stack Class (LIFO structure) ---
class Stack {
public:
  stackNode* topPtr;  // Pointer to the top node of the stack (nullptr if empty)

  // --- Constructor & Destructor ---
  Stack();   // Default constructor
  ~Stack();  // Destructor (cleans up stackNode memory, not LinkedList nodes)

  // --- Standard Stack Operations ---
  bool isEmpty() const;  // Checks if the stack is empty (const)
  void push(
      node* item);  // Adds an item (pointer to LinkedList node) to the top
  node* pop();  // Removes and returns the top item (pointer to LinkedList node)
};

// Forward declaration of function from main.cpp
void ensureMusicDirectoryExists();

// --- Utility Function Definitions (inline in header) ---

// Helper for case-insensitive string comparison (Equality Check)
// Note: For sorting, direct comparison of lowercase strings is used in link.cpp
inline bool caseInsensitiveCompareEqual(const std::string& str1,
                                        const std::string& str2) {
  // Ensure comparison doesn't go out of bounds if strings differ in length
  return (str1.size() == str2.size()) &&
         std::equal(str1.begin(), str1.end(), str2.begin(),
                    // Lambda function for case-insensitive char comparison
                    [](unsigned char c1, unsigned char c2) {
                      return std::tolower(c1) == std::tolower(c2);
                    });
}

// Helper to get clean song name (remove path and extension)
inline std::string getCleanSongName(const std::string& fullPath) {
  if (fullPath.empty()) {
    return "[Empty Path]";  // Return something specific for empty input
  }
  std::string temp = fullPath;

  // Find last '/' or '\' (platform-independent path separator check)
  size_t lastSlash = temp.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    temp = temp.substr(lastSlash + 1);  // Get part after the last slash
  }

  // Find last '.' to remove extension
  size_t lastDot = temp.rfind('.');
  // Only remove if dot exists and is not the first character (e.g., hidden
  // files)
  if (lastDot != std::string::npos && lastDot > 0) {
    // Optional: check if it's a common audio extension
    // std::string ext = temp.substr(lastDot);
    // if (ext == ".mp3" || ext == ".wav" || ...)
    temp = temp.substr(0, lastDot);
  }
  // Handle case where the original path might have been just "." or ".."
  if (temp == "." || temp == "..")
    return fullPath;  // Avoid returning empty string for these

  // Remove numeric prefixes like "01. ", "1. ", etc.
  std::regex numericPrefix("^\\s*\\d+\\.?\\s*");
  temp = std::regex_replace(temp, numericPrefix, "");

  return temp.empty()
             ? "[Unnamed]"
             : temp;  // Return "[Unnamed]" if cleaning results in empty string
}

// Helper to get all supported audio files from the music directory and its
// subdirectories Returns a vector of pairs containing:
// - first: full path to the audio file
// - second: clean song name (without path and extension)
inline std::vector<std::pair<std::string, std::string>> getMusicFiles(
    const std::string& dirPath = "music") {
  std::vector<std::pair<std::string, std::string>> result;

  // Check if directory exists
  if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
    // If specifically looking in the music directory, try to create it
    if (dirPath == "music") {
      ensureMusicDirectoryExists();
      // Check again after attempted creation
      if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return result;  // Empty result if directory still doesn't exist
      }
    } else {
      std::cerr << "\t\tWarning: Directory '" << dirPath
                << "' not found or is not a directory!" << std::endl;
      return result;
    }
  }

  try {
    // Use directory_iterator (non-recursive) to get files only in the current
    // directory
    for (const auto& entry : fs::directory_iterator(dirPath)) {
      if (entry.is_regular_file()) {
        std::string filePath = entry.path().string();
        std::string extension = entry.path().extension().string();

        // Convert extension to lowercase for case-insensitive comparison
        std::string lowerExt = extension;
        std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        // Check if it's a supported audio file
        if (lowerExt == ".mp3" || lowerExt == ".wav" || lowerExt == ".flac" ||
            lowerExt == ".ogg") {
          std::string cleanName = getCleanSongName(filePath);
          result.push_back(std::make_pair(filePath, cleanName));
        }
      }
    }

    // Sort the result by clean song name (case insensitive)
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
      std::string lowerA = a.second;
      std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), ::tolower);

      std::string lowerB = b.second;
      std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), ::tolower);

      return lowerA < lowerB;
    });
  } catch (const fs::filesystem_error& e) {
    std::cerr << "\t\tError accessing directory: " << e.what() << std::endl;
  }

  return result;
}

// Helper to get all subdirectories in the given directory
// Returns a vector of pairs containing:
// - first: full path to the subdirectory
// - second: directory name
inline std::vector<std::pair<std::string, std::string>> getSubdirectories(
    const std::string& dirPath = "music") {
  std::vector<std::pair<std::string, std::string>> result;

  // Check if directory exists
  if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
    // If specifically looking in the music directory, try to create it
    if (dirPath == "music") {
      ensureMusicDirectoryExists();
      // Check again after attempted creation
      if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        return result;  // Empty result if directory still doesn't exist
      }
    } else {
      std::cerr << "\t\tWarning: Directory '" << dirPath
                << "' not found or is not a directory!" << std::endl;
      return result;
    }
  }

  try {
    // Use directory_iterator to get only subdirectories in the current
    // directory
    for (const auto& entry : fs::directory_iterator(dirPath)) {
      if (entry.is_directory()) {
        std::string fullPath = entry.path().string();
        std::string dirName = entry.path().filename().string();
        result.push_back(std::make_pair(fullPath, dirName));
      }
    }

    // Sort the result by directory name (case insensitive)
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
      std::string lowerA = a.second;
      std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(), ::tolower);

      std::string lowerB = b.second;
      std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(), ::tolower);

      return lowerA < lowerB;
    });
  } catch (const fs::filesystem_error& e) {
    std::cerr << "\t\tError accessing directory: " << e.what() << std::endl;
  }

  return result;
}

#endif  // LINK_H