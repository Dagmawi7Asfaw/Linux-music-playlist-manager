






















#include "link.h"  // Includes string, iostream, algorithm, cctype, iomanip etc.

#include <fstream>  // For file input/output streams (ofstream, ifstream)
#include <iomanip>  // Needed for std::setw
#include <nlohmann/json.hpp>  // For JSON handling

// For convenience
using json = nlohmann::json;

// Utility functions (caseInsensitiveCompareEqual, getCleanSongName) are defined
// inline in link.h

// --- LinkedList Implementation ---

// Constructor: Initializes an empty list
LinkedList::LinkedList() : head(nullptr), listName(""), len(0), taken(false) {}

// Destructor: Ensures all nodes are deleted when the list object is destroyed
LinkedList::~LinkedList() {
  clear();
}

// --- Stack Implementation ---

// Constructor: Initializes an empty stack
Stack::Stack() : topPtr(nullptr) {}

// Destructor: Deletes all stackNode structures
Stack::~Stack() {
  while (!isEmpty()) {
    stackNode* temp = topPtr;
    topPtr = topPtr->next;
    delete temp;  // Delete the stack node
  }
}

// Checks if the stack is empty
bool Stack::isEmpty() const {
  return topPtr == nullptr;
}

// Pushes a pointer to a LinkedList node onto the stack
void Stack::push(node* item) {
  stackNode* newStackNode = new stackNode(item, topPtr);
  topPtr = newStackNode;
}

// Pops a pointer to a LinkedList node from the stack
node* Stack::pop() {
  if (isEmpty()) {
    return nullptr;  // Cannot pop from empty stack
  }
  stackNode* temp = topPtr;           // Store top stackNode to delete
  node* itemToReturn = topPtr->item;  // Get pointer to LinkedList node
  topPtr = topPtr->next;              // Move stack top down
  delete temp;                        // Delete the stackNode structure
  return itemToReturn;                // Return pointer to LinkedList node data
}

// Adds a song to the beginning of the list
void LinkedList::add_beg(const std::string& song, const std::string& artist) {
  node* newNode = new node(song, artist);  // Allocate memory for the new node

  if (head == nullptr) {  // List is currently empty
    head = newNode;
    newNode->next = head;  // Point to itself to make it circular
  } else {                 // List has existing nodes
    node* last = head;
    // Find the current last node (the one pointing back to head)
    while (last->next != head) {
      last = last->next;
    }
    newNode->next = head;  // New node points to the old head
    head = newNode;        // Update the list's head pointer
    last->next = head;     // Make the old last node point to the new head
  }
  len++;  // Increment the song count
}

// Adds a song to the end of the list
void LinkedList::add_end(const std::string& song, const std::string& artist) {
  node* newNode = new node(song, artist);  // Allocate memory

  if (head == nullptr) {  // List is empty
    head = newNode;
    newNode->next = head;
  } else {  // List has existing nodes
    node* temp = head;
    // Find the current last node
    while (temp->next != head) {
      temp = temp->next;
    }
    temp->next = newNode;  // Old last node points to the new node
    newNode->next =
        head;  // New node points back to the head to maintain circularity
  }
  len++;  // Increment count
}

// Adds a song at a specific 1-based position
void LinkedList::add_at(const std::string& song, const std::string& artist,
                        int pos) {
  // Validate position (1 to len+1 allowed, where len+1 means adding at the end)
  if (pos < 1 || pos > len + 1) {
    std::cerr << "\t\tWarning: Add position " << pos << " out of range (1-"
              << len + 1 << ")" << std::endl;
    return;
  }

  if (pos == 1) {
    add_beg(song, artist);  // Use existing function for adding at beginning
  } else if (pos == len + 1) {
    add_end(song, artist);  // Use existing function for adding at end
  } else {                  // Inserting somewhere in the middle
    node* newNode = new node(song, artist);
    node* temp = head;
    // Traverse to the node *before* the desired insertion point (pos-1)
    for (int i = 1; i < pos - 1; ++i) {
      // Safety check, should not happen with valid pos range
      if (temp->next == head) break;
      temp = temp->next;
    }
    newNode->next = temp->next;  // New node points to what temp was pointing to
    temp->next = newNode;  // Temp (node before) now points to the new node
    len++;                 // Increment count
  }
}

// Deletes the song from the beginning of the list
void LinkedList::del_beg() {
  if (head == nullptr) {  // Cannot delete from an empty list
    std::cerr << "\t\tWarning: Cannot delete from empty list." << std::endl;
    return;
  }

  len--;  // Decrement count first

  if (head->next == head) {  // List has only one node
    delete head;             // Delete the node
    head = nullptr;          // List is now empty
  } else {                   // List has multiple nodes
    node* last = head;
    // Find the last node
    while (last->next != head) {
      last = last->next;
    }
    node* temp = head;  // Temporarily store the node to be deleted
    head = head->next;  // Update head to the second node
    last->next = head;  // Make the last node point to the new head
    delete temp;        // Free the memory of the old head
  }
}

// Deletes the song from the end of the list
void LinkedList::del_end() {
  if (head == nullptr) {  // Cannot delete from empty list
    std::cerr << "\t\tWarning: Cannot delete from empty list." << std::endl;
    return;
  }

  len--;  // Decrement count

  if (head->next == head) {  // Only one node
    delete head;
    head = nullptr;
  } else {  // Multiple nodes
    node* temp = head;
    node* follow = nullptr;  // Will point to the node *before* temp

    // Traverse until temp is the last node
    while (temp->next != head) {
      follow = temp;
      temp = temp->next;
    }
    // Now temp is the last node, follow is the second-to-last
    follow->next = head;  // Make second-to-last point back to head
    delete temp;          // Delete the last node
  }
}

// Deletes the song at a specific 1-based position
void LinkedList::del_at(int pos) {
  if (head == nullptr) {
    std::cerr << "\t\tWarning: Cannot delete from empty list." << std::endl;
    return;
  }
  // Validate position (1 to len allowed for deletion)
  if (pos < 1 || pos > len) {
    std::cerr << "\t\tWarning: Delete position " << pos << " out of range (1-"
              << len << ")" << std::endl;
    return;
  }

  if (pos == 1) {
    del_beg();  // Use existing function
  } else if (pos == len) {
    del_end();  // Use existing function (more efficient than traversing)
  } else {      // Deleting from the middle
    node* temp = head;
    node* follow = nullptr;  // Node before temp

    // Traverse so temp points to the node *at* the desired position 'pos'
    for (int i = 1; i < pos; ++i) {
      if (temp->next == head) break;  // Safety check
      follow = temp;
      temp = temp->next;
    }

    // Now follow points to the node before temp, temp is the one to delete
    follow->next = temp->next;  // Unlink temp from the list
    delete temp;                // Free the memory
    len--;  // Decrement count only for middle deletion case here
  }
}

// Displays the contents of the playlist
void LinkedList::display() const {
  std::cout << "\t\t\tPlaylist: " << (listName.empty() ? "[Unnamed]" : listName)
            << std::endl;
  std::cout << "\t\t\t------------------------------------" << std::endl;
  if (head == nullptr) {
    std::cout << std::endl << "\t\t\t(List is empty)" << std::endl;
  } else {
    node* temp = head;
    int count = 1;
    do {
      // Get clean name using helper from link.h
      std::string cleanName = getCleanSongName(temp->song);
      // Use std::setw for formatting (requires <iomanip>)
      std::cout << "\t\t" << std::left << std::setw(3) << count++ << ". "
                << std::setw(35)
                << cleanName.substr(0, 35)               // Truncate if too long
                << " -- " << temp->artist.substr(0, 20)  // Truncate artist
                << std::endl;
      temp = temp->next;
    } while (temp != head);  // Loop until back to the start
    std::cout << std::endl;
  }
}

// Deletes all nodes in the list, freeing memory
void LinkedList::clear() {
  if (head == nullptr) return;  // Nothing to clear

  node* current = head->next;  // Start checking from the second node
  head->next = nullptr;        // IMPORTANT: Break the circle FIRST

  // Delete all nodes except the original head
  while (current != nullptr) {
    node* nodeToDelete = current;
    current = current->next;
    delete nodeToDelete;
  }

  // Now delete the original head node
  delete head;
  head = nullptr;  // Reset head pointer
  len = 0;         // Reset length
  // Note: listName and taken status are NOT reset by clear() itself,
  // the calling function (like handleDeleteList) should handle those.
}

// Checks if the list is empty
bool LinkedList::isEmpty() const {
  return head == nullptr;
}

// Searches for songs containing the searchTerm (case-insensitive)
void LinkedList::search(const std::string& searchTerm) const {
  if (head == nullptr) {
    std::cout << "\t\tList is empty. Nothing to search." << std::endl;
    return;
  }

  node* temp = head;
  bool found = false;
  int position = 1;

  // Prepare lowercase search term for case-insensitive comparison
  std::string lowerSearchTerm;
  lowerSearchTerm.reserve(searchTerm.size());
  std::transform(searchTerm.begin(), searchTerm.end(),
                 std::back_inserter(lowerSearchTerm),
                 [](unsigned char c) { return std::tolower(c); });

  std::cout << "\t\tSearching for text: \"" << searchTerm << "\"" << std::endl;

  // Iterate through the list
  do {
    // Get clean version of the current song name
    std::string cleanListSong = getCleanSongName(temp->song);

    // Prepare lowercase version of the current song name
    std::string lowerListSong;
    lowerListSong.reserve(cleanListSong.size());
    std::transform(cleanListSong.begin(), cleanListSong.end(),
                   std::back_inserter(lowerListSong),
                   [](unsigned char c) { return std::tolower(c); });

    // Check if the lowercase song name contains the lowercase search term
    if (lowerListSong.find(lowerSearchTerm) != std::string::npos) {
      std::cout << "\t\t✅ Match found at position " << position << ":"
                << std::endl;
      std::cout << "\t\t   Song: " << cleanListSong << std::endl;
      std::cout << "\t\t   Artist: " << temp->artist << std::endl;
      // Optionally show full path for debugging/clarity
      // std::cout << "\t\t   (Full Path: " << temp->song << ")" << std::endl;
      found = true;
      // Continue searching for more matches (don't break)
    }
    temp = temp->next;
    position++;
  } while (temp != head);  // Loop through the circular list

  if (!found) {
    std::cout << "\t\t❌ No songs found containing that text." << std::endl;
  }
}

// Private helper to swap the *data* fields of two nodes
void LinkedList::swapNodesData(node* a, node* b) {
  if (a && b && a != b) {  // Ensure nodes are valid and different
    std::swap(a->song, b->song);
    std::swap(a->artist, b->artist);
  }
}

// Sorts the list by song title (case-insensitive) using Bubble Sort
void LinkedList::sortBySong() {
  if (head == nullptr || head->next == head)
    return;  // Already sorted if 0 or 1 element

  bool swapped;
  do {
    swapped = false;
    node* current = head;
    // In a circular list, we iterate 'len-1' times for comparisons in one pass
    for (int i = 0; i < len - 1; ++i) {
      node* nextNode = current->next;

      // Get clean names for comparison
      std::string nameCurrent = getCleanSongName(current->song);
      std::string nameNext = getCleanSongName(nextNode->song);

      // Convert to lowercase for case-insensitive comparison
      std::string lowerCurrent, lowerNext;
      std::transform(nameCurrent.begin(), nameCurrent.end(),
                     std::back_inserter(lowerCurrent), ::tolower);
      std::transform(nameNext.begin(), nameNext.end(),
                     std::back_inserter(lowerNext), ::tolower);

      // Compare lowercase strings lexicographically
      if (lowerCurrent > lowerNext) {      // If current should come after next
        swapNodesData(current, nextNode);  // Swap their data
        swapped = true;  // Mark that a swap occurred in this pass
      }
      current = nextNode;  // Move to the next node for the next comparison
    }
    // After one full pass, the largest element is at the 'end' relative to the
    // start
  } while (swapped);  // Repeat passes until no swaps occur
}

// Sorts the list by artist name (case-insensitive) using Bubble Sort
void LinkedList::sortByArtist() {
  if (head == nullptr || head->next == head) return;

  bool swapped;
  do {
    swapped = false;
    node* current = head;
    for (int i = 0; i < len - 1; ++i) {
      node* nextNode = current->next;

      // Convert artist names to lowercase for comparison
      std::string lowerCurrentArtist, lowerNextArtist;
      std::transform(current->artist.begin(), current->artist.end(),
                     std::back_inserter(lowerCurrentArtist), ::tolower);
      std::transform(nextNode->artist.begin(), nextNode->artist.end(),
                     std::back_inserter(lowerNextArtist), ::tolower);

      // Compare lowercase strings
      if (lowerCurrentArtist >
          lowerNextArtist) {               // If current should come after next
        swapNodesData(current, nextNode);  // Swap data
        swapped = true;                    // Mark swap
      }
      current = nextNode;  // Move to next node
    }
  } while (swapped);  // Repeat if swaps occurred
}

// Saves the playlist data to a JSON file
bool LinkedList::saveToFile(const std::string& filename) const {
  try {
    json playlistJson;

    // Store list metadata
    playlistJson["listName"] = listName;
    playlistJson["length"] = len;

    // Create an array for songs
    json songsArray = json::array();

    // Add each song as a JSON object with song path and artist
    if (head != nullptr) {
      node* current = head;
      do {
        json songObj;
        songObj["song"] = current->song;
        songObj["artist"] = current->artist;
        songsArray.push_back(songObj);
        current = current->next;
      } while (current != head);
    }

    // Add songs array to the main JSON object
    playlistJson["songs"] = songsArray;

    // Open file for writing
    std::ofstream file(filename);
    if (!file.is_open()) {
      std::cerr << "\t\tError: Could not open file '" << filename
                << "' for writing." << std::endl;
      return false;
    }

    // Write pretty-printed JSON to file
    file << playlistJson.dump(4);  // 4 spaces for indentation

    // Check for errors and close file
    bool success = file.good();
    file.close();

    if (!success) {
      std::cerr << "\t\tError: A problem occurred while writing to file '"
                << filename << "'." << std::endl;
      return false;
    }

    return true;
  } catch (const json::exception& e) {
    std::cerr << "\t\tJSON error when saving playlist: " << e.what()
              << std::endl;
    return false;
  } catch (const std::exception& e) {
    std::cerr << "\t\tError when saving playlist: " << e.what() << std::endl;
    return false;
  }
}

// Loads playlist data from a JSON file
bool LinkedList::loadFromFile(const std::string& filename) {
  try {
    // Open file for reading
    std::ifstream file(filename);
    if (!file.is_open()) {
      // Fail silently - file might just not exist, which isn't necessarily an
      // error here
      return false;
    }

    // Parse JSON from file
    json playlistJson;
    try {
      file >> playlistJson;
    } catch (const json::parse_error& e) {
      std::cerr << "\t\tError: Failed to parse JSON from '" << filename
                << "': " << e.what() << std::endl;
      file.close();
      return false;
    }

    // Clear existing list
    clear();

    // Read list name
    if (playlistJson.contains("listName") &&
        playlistJson["listName"].is_string()) {
      listName = playlistJson["listName"].get<std::string>();
    } else {
      listName = "";
    }

    // Check for songs array
    if (playlistJson.contains("songs") && playlistJson["songs"].is_array()) {
      // Get the songs array
      const json& songsArray = playlistJson["songs"];

      // Process each song in the array
      for (const auto& songObj : songsArray) {
        if (songObj.contains("song") && songObj.contains("artist") &&
            songObj["song"].is_string() && songObj["artist"].is_string()) {
          std::string songPath = songObj["song"].get<std::string>();
          std::string artistName = songObj["artist"].get<std::string>();

          // Add song to the list
          add_end(songPath, artistName);
        } else {
          std::cerr
              << "\t\tWarning: Skipping improperly formatted song entry in '"
              << filename << "'." << std::endl;
        }
      }
    }

    // Additional check: Verify expected length if provided
    if (playlistJson.contains("length") &&
        playlistJson["length"].is_number_integer()) {
      int expectedLen = playlistJson["length"].get<int>();
      if (len != expectedLen) {
        std::cerr << "\t\tWarning: Expected " << expectedLen
                  << " songs, loaded " << len << " from '" << filename << "'."
                  << std::endl;
      }
    }

    file.close();
    taken = true;  // Mark list slot as taken
    return true;
  } catch (const json::exception& e) {
    std::cerr << "\t\tJSON error when loading playlist: " << e.what()
              << std::endl;
    clear();
    return false;
  } catch (const std::exception& e) {
    std::cerr << "\t\tException caught during file load '" << filename
              << "': " << e.what() << std::endl;
    clear();
    return false;
  }
}
