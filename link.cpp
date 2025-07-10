#include "link.h"  // Includes string, iostream, algorithm, cctype, iomanip etc.
// link.h contains declarations for LinkedList and Stack classes

#include <fstream>  // For file input/output streams (ofstream, ifstream)
#include <iomanip>  // Needed for std::setw for output formatting
#include <nlohmann/json.hpp>  // External library for JSON handling

// For convenience - allows using 'json' instead of 'nlohmann::json'
using json = nlohmann::json;

// Utility functions (caseInsensitiveCompareEqual, getCleanSongName) are defined
// inline in link.h - these handle case-insensitive string operations and file
// path cleaning

// --- LinkedList Implementation ---

// Constructor: Initializes an empty list
// head = nullptr: no nodes in the list initially
// listName = "": playlist has no name initially
// len = 0: list length is zero
// taken = false: list slot is not in use initially
LinkedList::LinkedList() : head(nullptr), listName(""), len(0), taken(false) {}

// Destructor: Ensures all nodes are deleted when the list object is destroyed
// Calls clear() to free all allocated memory for nodes
LinkedList::~LinkedList() {
  clear();
}

// --- Stack Implementation ---

// Constructor: Initializes an empty stack
// topPtr = nullptr: stack is empty initially
Stack::Stack() : topPtr(nullptr) {}

// Destructor: Deletes all stackNode structures
// Iterates through stack nodes and frees memory
Stack::~Stack() {
  while (!isEmpty()) {
    stackNode* temp = topPtr;  // Store current top node
    topPtr = topPtr->next;     // Move top pointer to next node
    delete temp;               // Delete the old top node
  }
}

// Checks if the stack is empty
// Returns true if topPtr is nullptr (no nodes)
bool Stack::isEmpty() const {
  return topPtr == nullptr;
}

// Pushes a pointer to a LinkedList node onto the stack
// Creates a new stackNode that holds the item pointer and links to current top
void Stack::push(node* item) {
  stackNode* newStackNode = new stackNode(
      item, topPtr);      // Create new node with item and link to current top
  topPtr = newStackNode;  // Update top to point to new node
}

// Pops a pointer to a LinkedList node from the stack
// Returns nullptr if stack is empty, otherwise returns the stored node pointer
node* Stack::pop() {
  if (isEmpty()) {
    return nullptr;  // Cannot pop from empty stack
  }
  stackNode* temp = topPtr;           // Store current top node to delete later
  node* itemToReturn = topPtr->item;  // Get the stored LinkedList node pointer
  topPtr = topPtr->next;              // Move stack top down to next node
  delete temp;                        // Free memory of the removed stackNode
  return itemToReturn;                // Return the LinkedList node pointer
}

// Adds a song to the beginning of the list
// Handles both empty and non-empty lists, maintaining circular structure
void LinkedList::add_beg(const std::string& song, const std::string& artist) {
  node* newNode = new node(song, artist);  // Create new node with song data

  if (head == nullptr) {   // Case 1: List is currently empty
    head = newNode;        // Set head to the new node
    newNode->next = head;  // Point to itself to make it circular
  } else {                 // Case 2: List has existing nodes
    node* last = head;
    // Find the current last node (the one pointing back to head)
    while (last->next !=
           head) {  // Traverse until we find node pointing to head
      last = last->next;
    }
    newNode->next = head;  // New node points to the old head
    head = newNode;        // Update head to point to the new node
    last->next = head;     // Update last node to point to new head (maintaining
                           // circular structure)
  }
  len++;  // Increment the song count
}

// Adds a song to the end of the list
// Handles both empty and non-empty lists, maintaining circular structure
void LinkedList::add_end(const std::string& song, const std::string& artist) {
  node* newNode = new node(song, artist);  // Create new node with song data

  if (head == nullptr) {   // Case 1: List is empty
    head = newNode;        // Set head to the new node
    newNode->next = head;  // Point to itself to make it circular
  } else {                 // Case 2: List has existing nodes
    node* temp = head;
    // Find the current last node
    while (temp->next !=
           head) {  // Traverse until we find node pointing to head
      temp = temp->next;
    }
    temp->next = newNode;  // Old last node points to the new node
    newNode->next =
        head;  // New node points back to the head to maintain circularity
  }
  len++;  // Increment count
}

// Adds a song at a specific 1-based position
// Validates position and delegates to specialized functions when appropriate
void LinkedList::add_at(const std::string& song, const std::string& artist,
                        int pos) {
  // Validate position (1 to len+1 allowed, where len+1 means adding at the end)
  if (pos < 1 || pos > len + 1) {
    std::cerr << "\t\tWarning: Add position " << pos << " out of range (1-"
              << len + 1 << ")" << std::endl;
    return;  // Exit if position is invalid
  }

  if (pos == 1) {
    add_beg(song, artist);  // Special case: add at beginning
  } else if (pos == len + 1) {
    add_end(song, artist);  // Special case: add at end
  } else {                  // Case 3: Inserting somewhere in the middle
    node* newNode = new node(song, artist);  // Create new node
    node* temp = head;
    // Traverse to the node *before* the desired insertion point (pos-1)
    for (int i = 1; i < pos - 1; ++i) {  // Loop until we reach position-1
      // Safety check, should not happen with valid pos range
      if (temp->next == head) break;  // Prevent infinite loop
      temp = temp->next;              // Move to next node
    }
    newNode->next = temp->next;  // New node points to node at position
    temp->next = newNode;        // Node at position-1 points to new node
    len++;                       // Increment count
  }
}

// Deletes the song from the beginning of the list
// Handles special cases for single node and multi-node lists
void LinkedList::del_beg() {
  if (head == nullptr) {  // Cannot delete from an empty list
    std::cerr << "\t\tWarning: Cannot delete from empty list." << std::endl;
    return;  // Exit if list is empty
  }

  len--;  // Decrement count first

  if (head->next == head) {  // Case 1: List has only one node
    delete head;             // Delete the single node
    head = nullptr;          // Reset head to empty list
  } else {                   // Case 2: List has multiple nodes
    node* last = head;
    // Find the last node (the one pointing to head)
    while (last->next != head) {
      last = last->next;
    }
    node* temp = head;  // Store old head to delete
    head = head->next;  // Move head to the second node
    last->next = head;  // Update last node to point to new head
    delete temp;        // Free the memory of the old head
  }
}

// Deletes the song from the end of the list
// Handles special cases for single node and multi-node lists
void LinkedList::del_end() {
  if (head == nullptr) {  // Cannot delete from empty list
    std::cerr << "\t\tWarning: Cannot delete from empty list." << std::endl;
    return;  // Exit if list is empty
  }

  len--;  // Decrement count

  if (head->next == head) {  // Case 1: Only one node
    delete head;             // Delete the single node
    head = nullptr;          // Reset head to empty list
  } else {                   // Case 2: Multiple nodes
    node* temp = head;
    node* follow = nullptr;  // Will point to the node *before* temp

    // Traverse until temp is the last node
    while (temp->next != head) {  // Loop until temp is the last node
      follow = temp;              // Keep track of node before temp
      temp = temp->next;          // Move temp forward
    }
    // Now temp is the last node, follow is the second-to-last
    follow->next = head;  // Make second-to-last point back to head
    delete temp;          // Delete the last node
  }
}

// Deletes the song at a specific 1-based position
// Validates position and delegates to specialized functions when appropriate
void LinkedList::del_at(int pos) {
  if (head == nullptr) {
    std::cerr << "\t\tWarning: Cannot delete from empty list." << std::endl;
    return;  // Exit if list is empty
  }
  // Validate position (1 to len allowed for deletion)
  if (pos < 1 || pos > len) {
    std::cerr << "\t\tWarning: Delete position " << pos << " out of range (1-"
              << len << ")" << std::endl;
    return;  // Exit if position is invalid
  }

  if (pos == 1) {
    del_beg();  // Special case: delete at beginning
  } else if (pos == len) {
    del_end();  // Special case: delete at end (more efficient)
  } else {      // Case 3: Deleting from the middle
    node* temp = head;
    node* follow = nullptr;  // Node before temp

    // Traverse so temp points to the node *at* the desired position 'pos'
    for (int i = 1; i < pos; ++i) {   // Loop until we reach position
      if (temp->next == head) break;  // Safety check against infinite loop
      follow = temp;                  // Keep track of previous node
      temp = temp->next;              // Move temp forward
    }

    // Now follow points to the node before temp, temp is the one to delete
    follow->next = temp->next;  // Link previous node to next node
    delete temp;                // Free the memory of the deleted node
    len--;                      // Decrement count only for middle deletion
  }
}

// Displays the contents of the playlist
// Shows playlist name and all songs with formatting
void LinkedList::display() const {
  std::cout << "\t\t\tPlaylist: " << (listName.empty() ? "[Unnamed]" : listName)
            << std::endl;  // Show playlist name or [Unnamed]
  std::cout << "\t\t\t------------------------------------" << std::endl;
  if (head == nullptr) {
    std::cout << std::endl
              << "\t\t\t(List is empty)"
              << std::endl;  // Message for empty list
  } else {
    node* temp = head;  // Start at head node
    int count = 1;      // Track position for display
    do {
      // Get clean name using helper from link.h
      std::string cleanName =
          getCleanSongName(temp->song);  // Extract filename from path
      // Use std::setw for formatting (requires <iomanip>)
      std::cout << "\t\t" << std::left << std::setw(3) << count++ << ". "
                << std::setw(35)
                << cleanName.substr(0, 35)  // Truncate song name if too long
                << " -- "
                << temp->artist.substr(0,
                                       20)  // Truncate artist name if too long
                << std::endl;
      temp = temp->next;  // Move to next node
    } while (temp != head);  // Loop until back to the start (circular list)
    std::cout << std::endl;
  }
}

// Deletes all nodes in the list, freeing memory
// Carefully handles circular list structure to avoid infinite loops
void LinkedList::clear() {
  if (head == nullptr) return;  // Nothing to clear if list is empty

  // Safety check: ensure we have a valid circular list
  if (head->next == nullptr) {
    // Single node that's not properly circular
    delete head;
    head = nullptr;
    len = 0;
    return;
  }

  node* current = head->next;  // Start from node after head
  head->next =
      nullptr;  // IMPORTANT: Break the circle FIRST to avoid infinite loop

  // Delete all nodes except the original head
  while (current != nullptr) {     // Continue until we reach the end
    node* nodeToDelete = current;  // Store current node to delete
    current = current->next;       // Move to next node
    delete nodeToDelete;           // Free memory of stored node
  }

  // Now delete the original head node
  delete head;     // Delete the head node
  head = nullptr;  // Reset head pointer to null
  len = 0;         // Reset length to zero
  // Note: listName and taken status are NOT reset by clear() itself,
  // the calling function (like handleDeleteList) should handle those.
}

// Checks if the list is empty
// Returns true if head is nullptr (no nodes)
bool LinkedList::isEmpty() const {
  return head == nullptr;
}

// Searches for songs containing the searchTerm (case-insensitive)
// Displays matching songs with their positions
void LinkedList::search(const std::string& searchTerm) const {
  if (head == nullptr) {
    std::cout << "\t\tList is empty. Nothing to search." << std::endl;
    return;  // Exit if list is empty
  }

  node* temp = head;   // Start at head node
  bool found = false;  // Track if any matches were found
  int position = 1;    // Track current position

  // Prepare lowercase search term for case-insensitive comparison
  std::string lowerSearchTerm;
  lowerSearchTerm.reserve(
      searchTerm.size());  // Pre-allocate memory for efficiency
  std::transform(
      searchTerm.begin(), searchTerm.end(), std::back_inserter(lowerSearchTerm),
      [](unsigned char c) { return std::tolower(c); });  // Convert to lowercase

  std::cout << "\t\tSearching for text: \"" << searchTerm << "\"" << std::endl;

  // Iterate through the list
  do {
    // Get clean version of the current song name (filename without path)
    std::string cleanListSong = getCleanSongName(temp->song);

    // Prepare lowercase version of the current song name
    std::string lowerListSong;
    lowerListSong.reserve(cleanListSong.size());  // Pre-allocate memory
    std::transform(cleanListSong.begin(), cleanListSong.end(),
                   std::back_inserter(lowerListSong), [](unsigned char c) {
                     return std::tolower(c);
                   });  // Convert to lowercase

    // Check if the lowercase song name contains the lowercase search term
    if (lowerListSong.find(lowerSearchTerm) !=
        std::string::npos) {  // String found
      std::cout << "\t\t✅ Match found at position " << position << ":"
                << std::endl;
      std::cout << "\t\t   Song: " << cleanListSong << std::endl;
      std::cout << "\t\t   Artist: " << temp->artist << std::endl;
      // Optionally show full path for debugging/clarity
      // std::cout << "\t\t   (Full Path: " << temp->song << ")" << std::endl;
      found = true;  // Mark that we found at least one match
      // Continue searching for more matches (don't break)
    }
    temp = temp->next;  // Move to next node
    position++;         // Increment position counter
  } while (temp != head);  // Loop through the circular list until back to head

  if (!found) {
    std::cout << "\t\t❌ No songs found containing that text." << std::endl;
  }
}

// Private helper to swap the *data* fields of two nodes
// Used by sorting algorithms to exchange node contents without changing links
void LinkedList::swapNodesData(node* a, node* b) {
  if (a && b && a != b) {             // Ensure nodes are valid and different
    std::swap(a->song, b->song);      // Swap song data
    std::swap(a->artist, b->artist);  // Swap artist data
  }
}

// Sorts the list by song title (case-insensitive) using Bubble Sort
// Maintains list structure while sorting just the data
void LinkedList::sortBySong() {
  if (head == nullptr || head->next == head)
    return;  // Already sorted if 0 or 1 element

  bool swapped;  // Flag to track if any swaps occurred in pass
  do {
    swapped = false;       // Reset swap flag for each pass
    node* current = head;  // Start at head
    // In a circular list, we iterate 'len-1' times for comparisons in one pass
    for (int i = 0; i < len - 1; ++i) {
      node* nextNode = current->next;  // Get next node

      // Get clean names for comparison (filenames without paths)
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
        swapNodesData(current, nextNode);  // Swap their data fields
        swapped = true;  // Mark that a swap occurred in this pass
      }
      current = nextNode;  // Move to the next node for next comparison
    }
    // After one full pass, the largest element is at the 'end' relative to the
    // start
  } while (swapped);  // Repeat passes until no swaps occur (sorted)
}

// Sorts the list by artist name (case-insensitive) using Bubble Sort
// Maintains list structure while sorting just the data
void LinkedList::sortByArtist() {
  if (head == nullptr || head->next == head)
    return;  // Already sorted if 0 or 1 element

  bool swapped;  // Flag to track if any swaps occurred in pass
  do {
    swapped = false;       // Reset swap flag for each pass
    node* current = head;  // Start at head
    for (int i = 0; i < len - 1; ++i) {
      node* nextNode = current->next;  // Get next node

      // Convert artist names to lowercase for comparison
      std::string lowerCurrentArtist, lowerNextArtist;
      std::transform(current->artist.begin(), current->artist.end(),
                     std::back_inserter(lowerCurrentArtist),
                     ::tolower);  // Current artist to lowercase
      std::transform(nextNode->artist.begin(), nextNode->artist.end(),
                     std::back_inserter(lowerNextArtist),
                     ::tolower);  // Next artist to lowercase

      // Compare lowercase strings
      if (lowerCurrentArtist >
          lowerNextArtist) {               // If current should come after next
        swapNodesData(current, nextNode);  // Swap node data fields
        swapped = true;                    // Mark swap occurred
      }
      current = nextNode;  // Move to next node
    }
  } while (swapped);  // Repeat if swaps occurred (until sorted)
}

// Saves the playlist data to a JSON file
// Returns true on success, false on failure
bool LinkedList::saveToFile(const std::string& filename) const {
  try {
    json playlistJson;  // Create JSON object for the playlist

    // Store list metadata
    playlistJson["listName"] = listName;  // Save playlist name
    playlistJson["length"] = len;         // Save number of songs

    // Create an array for songs
    json songsArray = json::array();  // Create empty JSON array

    // Add each song as a JSON object with song path and artist
    if (head != nullptr) {
      node* current = head;  // Start at head
      do {
        json songObj;                     // Create JSON object for current song
        songObj["song"] = current->song;  // Add song path/name
        songObj["artist"] = current->artist;  // Add artist name
        songsArray.push_back(songObj);        // Add song object to array
        current = current->next;              // Move to next song
      } while (current != head);  // Continue until we loop back to head
    }

    // Add songs array to the main JSON object
    playlistJson["songs"] = songsArray;  // Add songs array to playlist object

    // Open file for writing
    std::ofstream file(filename);  // Create/open output file stream
    if (!file.is_open()) {
      std::cerr << "\t\tError: Could not open file '" << filename
                << "' for writing." << std::endl;
      return false;  // Return failure if can't open file
    }

    // Write pretty-printed JSON to file
    file << playlistJson.dump(4);  // 4 spaces for indentation

    // Check for errors and close file
    bool success = file.good();  // Check if stream is in good state
    file.close();                // Close the file

    if (!success) {
      std::cerr << "\t\tError: A problem occurred while writing to file '"
                << filename << "'." << std::endl;
      return false;  // Return failure if write errors occurred
    }

    return true;  // Return success
  } catch (const json::exception& e) {
    std::cerr << "\t\tJSON error when saving playlist: " << e.what()
              << std::endl;
    return false;  // Return failure on JSON error
  } catch (const std::exception& e) {
    std::cerr << "\t\tError when saving playlist: " << e.what() << std::endl;
    return false;  // Return failure on any other exception
  }
}

// Loads playlist data from a JSON file
// Returns true on success, false on failure
bool LinkedList::loadFromFile(const std::string& filename) {
  try {
    // Open file for reading
    std::ifstream file(filename);  // Open input file stream
    if (!file.is_open()) {
      // Fail silently - file might just not exist, which isn't necessarily an
      // error here
      return false;  // Return failure if can't open file
    }

    // Parse JSON from file
    json playlistJson;  // Create JSON object to hold parsed data
    try {
      file >> playlistJson;  // Read JSON from file into object
    } catch (const json::parse_error& e) {
      std::cerr << "\t\tError: Failed to parse JSON from '" << filename
                << "': " << e.what() << std::endl;
      file.close();  // Close file
      return false;  // Return failure on parse error
    }

    // Clear existing list
    clear();  // Remove all existing nodes

    // Read list name
    if (playlistJson.contains("listName") &&
        playlistJson["listName"].is_string()) {
      std::string loadedName = playlistJson["listName"].get<std::string>();
      // Validate the loaded string - check for null bytes or excessive length
      if (loadedName.find('\0') != std::string::npos) {
        std::cerr << "\t\tWarning: Playlist name contains null bytes, using "
                     "default name."
                  << std::endl;
        listName = "";
      } else if (loadedName.length() > 100) {
        std::cerr << "\t\tWarning: Playlist name too long, truncating."
                  << std::endl;
        listName = loadedName.substr(0, 100);
      } else {
        listName = loadedName;
      }
    } else {
      listName = "";  // Default to empty name if not in JSON
    }

    // Check for songs array
    if (playlistJson.contains("songs") && playlistJson["songs"].is_array()) {
      // Get the songs array
      const json& songsArray =
          playlistJson["songs"];  // Reference to the songs array

      // Process each song in the array
      for (const auto& songObj :
           songsArray) {  // Iterate through each song object
        if (songObj.contains("song") && songObj.contains("artist") &&
            songObj["song"].is_string() && songObj["artist"].is_string()) {
          std::string songPath =
              songObj["song"].get<std::string>();  // Get song path
          std::string artistName =
              songObj["artist"].get<std::string>();  // Get artist name

          // Add song to the list
          add_end(songPath, artistName);  // Add to end of playlist
        } else {
          std::cerr
              << "\t\tWarning: Skipping improperly formatted song entry in '"
              << filename << "'." << std::endl;
          // Continue loading other songs
        }
      }
    }

    // Additional check: Verify expected length if provided
    if (playlistJson.contains("length") &&
        playlistJson["length"].is_number_integer()) {
      int expectedLen =
          playlistJson["length"].get<int>();  // Get expected length
      if (len != expectedLen) {
        std::cerr << "\t\tWarning: Expected " << expectedLen
                  << " songs, loaded " << len << " from '" << filename << "'."
                  << std::endl;
        // Continue despite mismatch
      }
    }

    file.close();  // Close the file
    taken = true;  // Mark list slot as taken/in-use
    return true;   // Return success
  } catch (const json::exception& e) {
    std::cerr << "\t\tJSON error when loading playlist: " << e.what()
              << std::endl;
    clear();       // Clear any partially loaded data
    return false;  // Return failure on JSON error
  } catch (const std::exception& e) {
    std::cerr << "\t\tException caught during file load '" << filename
              << "': " << e.what() << std::endl;
    clear();       // Clear any partially loaded data
    return false;  // Return failure on any other exception
  }
}
