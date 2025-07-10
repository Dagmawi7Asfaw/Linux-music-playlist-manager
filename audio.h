#ifndef AUDIO_H
#define AUDIO_H

#include <string>  // Explicit include for clarity

#include "link.h"  // Needs LinkedList definition, includes utilities, string etc.

// Utility Function getCleanSongName is defined in link.h

// --- Audio Playback Function Declarations ---

// Plays a single audio file specified by filename.
// Returns 0 on success, non-zero on error.
int player(const std::string& filename);

// Plays a single audio file specified by filename with play/pause/stop
// controls. Returns 0 on success, 1 on error, 2 if stopped by user.
int playerWithControls(const std::string& filename);

// --- Playback Functions ---

// Plays the playlist with controls (play/pause/stop) for each song.
// This is the base function for sequential playback with controls.
void playWithControls(LinkedList& list);

// Plays a specific song from the playlist selected by the user with controls.
void playWithControlsSingle(LinkedList& list);

// Plays the playlist in reverse order with controls.
void playWithControlsReverse(LinkedList& list);

// Plays the playlist multiple times with controls.
void playWithControlsRepeat(LinkedList& list, int rounds = 1);

#endif  // AUDIO_H