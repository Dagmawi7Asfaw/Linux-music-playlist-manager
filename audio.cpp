#include "audio.h"  // Includes link.h -> utilities, string, iostream, iomanip etc.

#include <cstdlib>
#include <limits>  // For std::numeric_limits
// Required for MP3 decoding
#include <mpg123.h>
// Required for PulseAudio output
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
// Required for FLAC, WAV, OGG support
#include <sndfile.h>
// Required for sleep()
#include <unistd.h>
// Required for low-level file check (fopen)
#include <cstdio>
// Required for memset
#include <cstring>
// Required for std::vector (safer buffer)
#include <vector>
// Required for toupper
#include <cctype>
// Required for std::min, std::max
#include <algorithm>
// Required for std::fixed, std::setprecision, std::setfill, std::setw
#include <iomanip>
// Required for async input handling
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
// Required for pattern matching with extension
#include <regex>

// Define buffer size for reading decoded audio chunks
#define AUDIO_BUFFER_SIZE \
  8192  // Increased buffer size might improve performance slightly

// getCleanSongName is included via audio.h -> link.h

// --- Helper function for non-blocking keyboard input ---
bool kbhit() {
  struct pollfd fds;
  fds.fd = STDIN_FILENO;
  fds.events = POLLIN;

  // Poll with timeout of 0, effectively making it non-blocking
  return poll(&fds, 1, 0) == 1;
}

// --- Get a character without waiting for Enter ---
char getch() {
  char buf = 0;
  struct termios old_term;

  // Get the current terminal settings
  if (tcgetattr(STDIN_FILENO, &old_term) < 0) {
    perror("tcgetattr()");
    return 0;
  }

  // Create new settings
  struct termios new_term = old_term;

  // Disable canonical mode (line-by-line input)
  // and echo (don't show typed characters)
  // TCSANOW = apply changes immediately
  new_term.c_lflag &= ~(ICANON | ECHO);

  // Set minimum characters and timeout
  new_term.c_cc[VMIN] = 1;   // Minimum characters to read
  new_term.c_cc[VTIME] = 0;  // Timeout in deciseconds

  // Apply the new settings
  if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) < 0) {
    perror("tcsetattr");
    return 0;
  }

  ssize_t read_result = read(STDIN_FILENO, &buf, 1);

  // Restore the old settings
  if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) < 0) {
    perror("tcsetattr restore");
  }

  if (read_result < 0) {
    perror("read()");
    return 0;
  }

  return buf;
}

// --- Utility function to check if a file is an MP3 ---
bool isMP3File(const std::string& filename) {
  std::regex mp3Pattern(".*\\.[mM][pP]3$");
  return std::regex_match(filename, mp3Pattern);
}

// --- Core Audio Player Function ---
int player(const std::string& filename) {
  // 1. --- Basic File Existence Check ---
  FILE* file = fopen(filename.c_str(), "rb");
  if (!file) {
    std::cerr << "\t\tError: File not found or cannot be opened - " << filename
              << std::endl;
    return 1;
  }
  fclose(file);

  // Check if this is an MP3 file or another audio format
  bool isMP3 = isMP3File(filename);

  if (isMP3) {
    // --- MP3 Playback Using mpg123 ---

    // 2. --- Initialize mpg123 Library ---
    if (mpg123_init() != MPG123_OK) {
      std::cerr << "\t\tError: Cannot initialize mpg123 library." << std::endl;
      return 1;
    }

    // 3. --- Create mpg123 Handle ---
    int mpg123_error_code = MPG123_OK;
    mpg123_handle* mh = mpg123_new(NULL, &mpg123_error_code);
    if (mh == NULL || mpg123_error_code != MPG123_OK) {
      std::cerr << "\t\tError: Unable to create mpg123 handle: "
                << mpg123_plain_strerror(mpg123_error_code) << std::endl;
      mpg123_exit();
      return 1;
    }

    // 4. --- Configure mpg123 Parameters ---
    mpg123_param(mh, MPG123_VERBOSE, 0, 0.0);
    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_IGNORE_INFOFRAME, 0);
    mpg123_param(mh, MPG123_RESYNC_LIMIT, -1, 0.0);

    // 5. --- Open the MP3 File with mpg123 ---
    if (mpg123_open(mh, filename.c_str()) != MPG123_OK) {
      std::cerr << "\t\tError: mpg123 cannot open '" << filename
                << "': " << mpg123_strerror(mh) << std::endl;
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 6. --- Get Audio Format from MP3 ---
    long rate = 0;
    int channels = 0, encoding = 0;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
      std::cerr << "\t\tError: Cannot get initial audio format for '"
                << filename << "'" << std::endl;
      mpg123_close(mh);
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 7. --- Force Output Format for PulseAudio ---
    const int OUTPUT_FORMAT = MPG123_ENC_SIGNED_16;
    const int OUTPUT_CHANNELS = channels;
    const long OUTPUT_RATE = rate;

    mpg123_format_none(mh);
    if (mpg123_format(mh, OUTPUT_RATE, OUTPUT_CHANNELS, OUTPUT_FORMAT) !=
        MPG123_OK) {
      std::cerr << "\t\tError: mpg123 cannot set output format to S16LE."
                << std::endl;
      mpg123_close(mh);
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 8. --- Setup PulseAudio Simple Stream ---
    pa_simple* pa_stream = NULL;
    pa_sample_spec sample_spec;
    sample_spec.format = PA_SAMPLE_S16LE;
    sample_spec.rate = static_cast<uint32_t>(OUTPUT_RATE);
    sample_spec.channels = static_cast<uint8_t>(OUTPUT_CHANNELS);

    int pa_error_code;
    pa_stream =
        pa_simple_new(NULL, "MP3 Playlist App", PA_STREAM_PLAYBACK, NULL,
                      "Music", &sample_spec, NULL, NULL, &pa_error_code);

    if (!pa_stream) {
      std::cerr << "\t\tError: Cannot create PulseAudio stream: "
                << pa_strerror(pa_error_code) << std::endl;
      mpg123_close(mh);
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 9. --- Prepare for Playback Loop ---
    if (mpg123_scan(mh) != MPG123_OK) {
      std::cerr << "\t\tWarning: Could not fully scan '" << filename
                << "' for accurate length." << std::endl;
    }
    off_t total_frames =
        mpg123_length(mh);  // Total number of PCM frames (-1 if unknown)

    double total_seconds = (OUTPUT_RATE > 0 && total_frames > 0)
                               ? static_cast<double>(total_frames) / OUTPUT_RATE
                               : 0.0;

    std::vector<unsigned char> buffer(AUDIO_BUFFER_SIZE);
    size_t bytes_decoded = 0;

    // --- Display Info ---
    std::string displayName = getCleanSongName(filename);
    std::cout << "\t\t" << "▶️ Now playing: " << displayName << std::endl;
    if (total_seconds > 0) {
      int minutes = static_cast<int>(total_seconds) / 60;
      double seconds_part = total_seconds - minutes * 60;
      std::cout << "\t\t" << "   Duration: " << minutes << ":" << std::fixed
                << std::setprecision(1) << std::setfill('0') << std::setw(4)
                << seconds_part << std::endl;
    }
    std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
              << std::endl;

    // --- Playback Loop ---
    int mpg123_result = MPG123_OK;
    off_t current_frame_pos = 0;
    int last_percent = -1;
    bool playback_error = false;

    while (true) {
      mpg123_result =
          mpg123_read(mh, buffer.data(), buffer.size(), &bytes_decoded);

      if (mpg123_result == MPG123_DONE) {
        break;
      }
      if (mpg123_result == MPG123_NEW_FORMAT) {
        std::cerr << "\t\tInfo: MP3 stream changed format mid-playback."
                  << std::endl;
        mpg123_getformat(mh, &rate, &channels, &encoding);
        continue;
      }
      if (mpg123_result != MPG123_OK) {
        std::cerr << "\n\t\tError: mpg123 decoding error: "
                  << mpg123_plain_strerror(mpg123_result) << std::endl;
        playback_error = true;
        break;
      }
      if (bytes_decoded == 0) continue;

      if (pa_simple_write(pa_stream, buffer.data(), bytes_decoded,
                          &pa_error_code) < 0) {
        std::cerr << "\n\t\tError: PulseAudio write error: "
                  << pa_strerror(pa_error_code) << std::endl;
        playback_error = true;
        break;
      }

      // --- Progress Bar Update ---
      current_frame_pos = mpg123_framepos(mh);

      if (total_frames > 0) {
        double fraction_complete =
            static_cast<double>(current_frame_pos) / total_frames;
        int percent = static_cast<int>(fraction_complete * 100.0);
        percent = std::min(100, std::max(0, percent));

        if (percent != last_percent) {
          last_percent = percent;
          int progress_bar_width = 25;
          int pos = static_cast<int>(fraction_complete * progress_bar_width);
          pos = std::min(progress_bar_width, std::max(0, pos));

          std::cout << "\r\t\tProgress: [";
          for (int i = 0; i < progress_bar_width; ++i) {
            std::cout << (i < pos ? "■" : " ");
          }
          std::cout << "] " << percent << "% " << std::flush;
        }
      } else {
        std::cout << "\r\t\tPlaying... (duration unknown) " << std::flush;
      }
    }  // End of playback loop

    std::cout << std::endl;  // Final newline after progress bar

    // 10. --- Cleanup ---
    if (!playback_error) {
      if (pa_simple_drain(pa_stream, &pa_error_code) < 0) {
        std::cerr << "\t\tWarning: pa_simple_drain() failed: "
                  << pa_strerror(pa_error_code) << std::endl;
      }
      std::cout << "\t\t✓ Playback finished." << std::endl;
    } else {
      std::cerr << "\t\tPlayback stopped due to error." << std::endl;
    }

    if (pa_stream) pa_simple_free(pa_stream);
    if (mh) {
      mpg123_close(mh);
      mpg123_delete(mh);
    }
    mpg123_exit();

    return playback_error ? 1 : 0;
  } else {
    // --- WAV, FLAC, OGG playback using libsndfile ---

    // Open the audio file with libsndfile
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));

    SNDFILE* sndfile = sf_open(filename.c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
      std::cerr << "\t\tError: Cannot open audio file with libsndfile: "
                << sf_strerror(NULL) << std::endl;
      return 1;
    }

    // Setup PulseAudio
    pa_simple* pa_stream = NULL;
    pa_sample_spec sample_spec;
    sample_spec.format = PA_SAMPLE_S16LE;  // We'll convert all formats to S16LE
    sample_spec.rate = sfinfo.samplerate;
    sample_spec.channels = sfinfo.channels;

    int pa_error_code;
    pa_stream =
        pa_simple_new(NULL, "Audio Playlist App", PA_STREAM_PLAYBACK, NULL,
                      "Music", &sample_spec, NULL, NULL, &pa_error_code);

    if (!pa_stream) {
      std::cerr << "\t\tError: Cannot create PulseAudio stream: "
                << pa_strerror(pa_error_code) << std::endl;
      sf_close(sndfile);
      return 1;
    }

    // Setup terminal for non-blocking input
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    // Set stdin to non-blocking mode
    int old_flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    // Calculate total duration
    double total_seconds =
        static_cast<double>(sfinfo.frames) / sfinfo.samplerate;

    // Buffer for audio data - we'll read as 16-bit samples
    std::vector<short> buffer(AUDIO_BUFFER_SIZE);

    // Display info
    std::string displayName = getCleanSongName(filename);
    std::cout << "\t\t" << "▶️ Now playing: " << displayName << std::endl;
    if (total_seconds > 0) {
      int minutes = static_cast<int>(total_seconds) / 60;
      double seconds_part = total_seconds - minutes * 60;
      std::cout << "\t\t" << "   Duration: " << minutes << ":" << std::fixed
                << std::setprecision(1) << std::setfill('0') << std::setw(4)
                << seconds_part << std::endl;
    }
    std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
              << std::endl;

    // Playback loop
    sf_count_t frames_read;
    sf_count_t total_frames = sfinfo.frames;
    sf_count_t frames_played = 0;
    sf_count_t current_frame_pos = 0;
    int last_percent = -1;
    bool playback_error = false;
    bool is_paused = false;
    bool should_stop = false;

    while (!should_stop) {
      // Check for keyboard input
      if (kbhit()) {
        char key = getch();
        switch (key) {
          case ' ':  // Space - Toggle pause/play
            is_paused = !is_paused;
            std::cout << "\r\t\t" << (is_paused ? "⏸️ Paused " : "▶️ Playing")
                      << "                                  " << std::endl;
            break;
          case 's':  // 's' - Stop playback
          case 'S':
            should_stop = true;
            std::cout << "\r\t\t⏹️ Stopped                                     "
                      << std::endl;
            break;
          case 'j':  // 'j' - Jump back 10 seconds
          {
            sf_count_t jump_frames = sfinfo.samplerate * 10;  // 10 seconds
            current_frame_pos = sf_seek(sndfile, -jump_frames, SEEK_CUR);
            if (current_frame_pos < 0) {
              sf_seek(sndfile, 0, SEEK_SET);
              current_frame_pos = 0;
            }
            frames_played = current_frame_pos;
            std::cout
                << "\r\t\t⏪ Jumped back 10s                               "
                << std::endl;
          } break;
          case 'k':  // 'k' - Jump forward 10 seconds
          {
            sf_count_t jump_frames = sfinfo.samplerate * 10;  // 10 seconds
            current_frame_pos = sf_seek(sndfile, jump_frames, SEEK_CUR);
            if (current_frame_pos < 0) {
              // Error in seeking, try to recover
              sf_seek(sndfile, 0, SEEK_SET);
              current_frame_pos = 0;
            }
            frames_played = current_frame_pos;
            std::cout
                << "\r\t\t⏩ Jumped forward 10s                            "
                << std::endl;
          } break;
        }
      }

      // If paused, skip decoding/output steps
      if (is_paused) {
        usleep(100000);  // 100ms pause to reduce CPU usage while paused
        continue;
      }

      frames_read = sf_readf_short(sndfile, buffer.data(),
                                   buffer.size() / sfinfo.channels);
      if (frames_read <= 0) {
        // End of file or error
        break;
      }

      size_t bytes_to_write = frames_read * sfinfo.channels * sizeof(short);

      if (pa_simple_write(pa_stream, buffer.data(), bytes_to_write,
                          &pa_error_code) < 0) {
        std::cerr << "\n\t\tError: PulseAudio write error: "
                  << pa_strerror(pa_error_code) << std::endl;
        playback_error = true;
        break;
      }

      // Update progress
      frames_played += frames_read;
      current_frame_pos =
          sf_seek(sndfile, 0, SEEK_CUR);  // Get current position

      if (total_frames > 0) {
        double fraction_complete =
            static_cast<double>(frames_played) / total_frames;
        int percent = static_cast<int>(fraction_complete * 100.0);
        percent = std::min(100, std::max(0, percent));

        if (percent != last_percent) {
          last_percent = percent;
          int progress_bar_width = 25;
          int pos = static_cast<int>(fraction_complete * progress_bar_width);
          pos = std::min(progress_bar_width, std::max(0, pos));

          std::cout << "\r\t\tProgress: [";
          for (int i = 0; i < progress_bar_width; ++i) {
            std::cout << (i < pos ? "■" : " ");
          }
          std::cout << "] " << percent << "% " << std::flush;
        }
      } else {
        std::cout << "\r\t\tPlaying... (duration unknown) " << std::flush;
      }
    }

    std::cout << std::endl;  // Final newline after progress bar

    // Cleanup
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);  // Restore terminal settings
    fcntl(STDIN_FILENO, F_SETFL, old_flags);     // Restore flags

    if (!playback_error && !should_stop) {
      if (pa_simple_drain(pa_stream, &pa_error_code) < 0) {
        std::cerr << "\t\tWarning: pa_simple_drain() failed: "
                  << pa_strerror(pa_error_code) << std::endl;
      }
      std::cout << "\t\t✓ Playback finished." << std::endl;
    } else if (should_stop) {
      std::cout << "\t\t⏹️ Playback stopped by user." << std::endl;
    } else {
      std::cerr << "\t\tPlayback stopped due to error." << std::endl;
    }

    if (pa_stream) pa_simple_free(pa_stream);
    sf_close(sndfile);

    return playback_error ? 1 : (should_stop ? 2 : 0);
  }
}

// --- Player with controls: play/pause/stop ---
int playerWithControls(const std::string& filename) {
  // 1. --- Basic File Existence Check ---
  FILE* file = fopen(filename.c_str(), "rb");
  if (!file) {
    std::cerr << "\t\tError: File not found or cannot be opened - " << filename
              << std::endl;
    return 1;
  }
  fclose(file);

  // Check if this is an MP3 file or another audio format
  bool isMP3 = isMP3File(filename);

  if (isMP3) {
    // --- MP3 Playback Using mpg123 ---

    // 2. --- Initialize mpg123 Library ---
    if (mpg123_init() != MPG123_OK) {
      std::cerr << "\t\tError: Cannot initialize mpg123 library." << std::endl;
      return 1;
    }

    // 3. --- Create mpg123 Handle ---
    int mpg123_error_code = MPG123_OK;
    mpg123_handle* mh = mpg123_new(NULL, &mpg123_error_code);
    if (mh == NULL || mpg123_error_code != MPG123_OK) {
      std::cerr << "\t\tError: Unable to create mpg123 handle: "
                << mpg123_plain_strerror(mpg123_error_code) << std::endl;
      mpg123_exit();
      return 1;
    }

    // 4. --- Configure mpg123 Parameters ---
    mpg123_param(mh, MPG123_VERBOSE, 0, 0.0);
    mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_IGNORE_INFOFRAME, 0);
    mpg123_param(mh, MPG123_RESYNC_LIMIT, -1, 0.0);

    // 5. --- Open the MP3 File with mpg123 ---
    if (mpg123_open(mh, filename.c_str()) != MPG123_OK) {
      std::cerr << "\t\tError: mpg123 cannot open '" << filename
                << "': " << mpg123_strerror(mh) << std::endl;
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 6. --- Get Audio Format from MP3 ---
    long rate = 0;
    int channels = 0, encoding = 0;
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
      std::cerr << "\t\tError: Cannot get initial audio format for '"
                << filename << "'" << std::endl;
      mpg123_close(mh);
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 7. --- Force Output Format for PulseAudio ---
    const int OUTPUT_FORMAT = MPG123_ENC_SIGNED_16;
    const int OUTPUT_CHANNELS = channels;
    const long OUTPUT_RATE = rate;

    mpg123_format_none(mh);
    if (mpg123_format(mh, OUTPUT_RATE, OUTPUT_CHANNELS, OUTPUT_FORMAT) !=
        MPG123_OK) {
      std::cerr << "\t\tError: mpg123 cannot set output format to S16LE."
                << std::endl;
      mpg123_close(mh);
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 8. --- Setup PulseAudio Simple Stream ---
    pa_simple* pa_stream = NULL;
    pa_sample_spec sample_spec;
    sample_spec.format = PA_SAMPLE_S16LE;
    sample_spec.rate = static_cast<uint32_t>(OUTPUT_RATE);
    sample_spec.channels = static_cast<uint8_t>(OUTPUT_CHANNELS);

    int pa_error_code;
    pa_stream =
        pa_simple_new(NULL, "MP3 Playlist App", PA_STREAM_PLAYBACK, NULL,
                      "Music", &sample_spec, NULL, NULL, &pa_error_code);

    if (!pa_stream) {
      std::cerr << "\t\tError: Cannot create PulseAudio stream: "
                << pa_strerror(pa_error_code) << std::endl;
      mpg123_close(mh);
      mpg123_delete(mh);
      mpg123_exit();
      return 1;
    }

    // 9. --- Prepare for Playback Loop ---
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    // Set stdin to non-blocking mode
    int old_flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    if (mpg123_scan(mh) != MPG123_OK) {
      std::cerr << "\t\tWarning: Could not fully scan '" << filename
                << "' for accurate length." << std::endl;
    }
    off_t total_frames =
        mpg123_length(mh);  // Total number of PCM frames (-1 if unknown)

    double total_seconds = (OUTPUT_RATE > 0 && total_frames > 0)
                               ? static_cast<double>(total_frames) / OUTPUT_RATE
                               : 0.0;

    std::vector<unsigned char> buffer(AUDIO_BUFFER_SIZE);
    size_t bytes_decoded = 0;

    // --- Display Info ---
    std::string displayName = getCleanSongName(filename);
    std::cout << "\t\t" << "▶️ Now playing: " << displayName << std::endl;
    if (total_seconds > 0) {
      int minutes = static_cast<int>(total_seconds) / 60;
      double seconds_part = total_seconds - minutes * 60;
      std::cout << "\t\t" << "   Duration: " << minutes << ":" << std::fixed
                << std::setprecision(1) << std::setfill('0') << std::setw(4)
                << seconds_part << std::endl;
    }
    std::cout << "\t\t"
              << "   Controls: [Space] Play/Pause, [s] Stop, [j] -10s, [k] +10s"
              << std::endl;
    std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
              << std::endl;

    // --- Playback Loop ---
    int mpg123_result = MPG123_OK;
    off_t current_frame_pos = 0;
    int last_percent = -1;
    bool playback_error = false;
    bool is_paused = false;
    bool should_stop = false;

    while (!should_stop) {
      // Check for keyboard input
      if (kbhit()) {
        char key = getch();
        switch (key) {
          case ' ':  // Space - Toggle pause/play
            is_paused = !is_paused;
            std::cout << "\r\t\t" << (is_paused ? "⏸️ Paused " : "▶️ Playing")
                      << "                                  " << std::endl;
            break;
          case 's':  // 's' - Stop playback
          case 'S':
            should_stop = true;
            std::cout << "\r\t\t⏹️ Stopped                                     "
                      << std::endl;
            break;
          case 'j':  // 'j' - Jump back 10 seconds
            if (current_frame_pos > 0) {
              off_t jump_frames = OUTPUT_RATE * 10;  // 10 seconds
              off_t target_frame = current_frame_pos - jump_frames;
              if (target_frame < 0) target_frame = 0;
              mpg123_seek_frame(mh, target_frame, SEEK_SET);
              std::cout
                  << "\r\t\t⏪ Jumped back 10s                               "
                  << std::endl;
            }
            break;
          case 'k':  // 'k' - Jump forward 10 seconds
            if (total_frames > 0) {
              off_t jump_frames = OUTPUT_RATE * 10;  // 10 seconds
              off_t target_frame = current_frame_pos + jump_frames;
              if (target_frame >= total_frames) target_frame = total_frames - 1;
              mpg123_seek_frame(mh, target_frame, SEEK_SET);
              std::cout
                  << "\r\t\t⏩ Jumped forward 10s                            "
                  << std::endl;
            }
            break;
        }
      }

      // If paused, skip decoding/output steps
      if (is_paused) {
        usleep(100000);  // 100ms pause to reduce CPU usage while paused
        continue;
      }

      mpg123_result =
          mpg123_read(mh, buffer.data(), buffer.size(), &bytes_decoded);

      if (mpg123_result == MPG123_DONE) {
        break;
      }
      if (mpg123_result == MPG123_NEW_FORMAT) {
        std::cerr << "\t\tInfo: MP3 stream changed format mid-playback."
                  << std::endl;
        mpg123_getformat(mh, &rate, &channels, &encoding);
        continue;
      }
      if (mpg123_result != MPG123_OK) {
        std::cerr << "\n\t\tError: mpg123 decoding error: "
                  << mpg123_plain_strerror(mpg123_result) << std::endl;
        playback_error = true;
        break;
      }
      if (bytes_decoded == 0) continue;

      if (pa_simple_write(pa_stream, buffer.data(), bytes_decoded,
                          &pa_error_code) < 0) {
        std::cerr << "\n\t\tError: PulseAudio write error: "
                  << pa_strerror(pa_error_code) << std::endl;
        playback_error = true;
        break;
      }

      // --- Progress Bar Update ---
      current_frame_pos = mpg123_framepos(mh);

      if (total_frames > 0) {
        double fraction_complete =
            static_cast<double>(current_frame_pos) / total_frames;
        int percent = static_cast<int>(fraction_complete * 100.0);
        percent = std::min(100, std::max(0, percent));

        if (percent != last_percent) {
          last_percent = percent;
          int progress_bar_width = 25;
          int pos = static_cast<int>(fraction_complete * progress_bar_width);
          pos = std::min(progress_bar_width, std::max(0, pos));

          std::cout << "\r\t\tProgress: [";
          for (int i = 0; i < progress_bar_width; ++i) {
            std::cout << (i < pos ? "■" : " ");
          }
          std::cout << "] " << percent << "% " << std::flush;
        }
      } else {
        std::cout << "\r\t\tPlaying... (duration unknown) " << std::flush;
      }
    }  // End of playback loop

    std::cout << std::endl;  // Final newline after progress bar

    // 10. --- Cleanup ---
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);  // Restore terminal settings
    fcntl(STDIN_FILENO, F_SETFL, old_flags);     // Restore flags

    if (!playback_error && !should_stop) {
      if (pa_simple_drain(pa_stream, &pa_error_code) < 0) {
        std::cerr << "\t\tWarning: pa_simple_drain() failed: "
                  << pa_strerror(pa_error_code) << std::endl;
      }
      std::cout << "\t\t✓ Playback finished." << std::endl;
    } else if (should_stop) {
      std::cout << "\t\t⏹️ Playback stopped by user." << std::endl;
    } else {
      std::cerr << "\t\tPlayback stopped due to error." << std::endl;
    }

    if (pa_stream) pa_simple_free(pa_stream);
    if (mh) {
      mpg123_close(mh);
      mpg123_delete(mh);
    }
    mpg123_exit();

    return playback_error ? 1 : (should_stop ? 2 : 0);
  } else {
    // --- WAV, FLAC, OGG playback using libsndfile ---

    // Open the audio file with libsndfile
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));

    SNDFILE* sndfile = sf_open(filename.c_str(), SFM_READ, &sfinfo);
    if (!sndfile) {
      std::cerr << "\t\tError: Cannot open audio file with libsndfile: "
                << sf_strerror(NULL) << std::endl;
      return 1;
    }

    // Setup PulseAudio
    pa_simple* pa_stream = NULL;
    pa_sample_spec sample_spec;
    sample_spec.format = PA_SAMPLE_S16LE;  // We'll convert all formats to S16LE
    sample_spec.rate = sfinfo.samplerate;
    sample_spec.channels = sfinfo.channels;

    int pa_error_code;
    pa_stream =
        pa_simple_new(NULL, "Audio Playlist App", PA_STREAM_PLAYBACK, NULL,
                      "Music", &sample_spec, NULL, NULL, &pa_error_code);

    if (!pa_stream) {
      std::cerr << "\t\tError: Cannot create PulseAudio stream: "
                << pa_strerror(pa_error_code) << std::endl;
      sf_close(sndfile);
      return 1;
    }

    // Setup terminal for non-blocking input
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;
    new_tio.c_lflag &= ~(ICANON | ECHO);  // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    // Set stdin to non-blocking mode
    int old_flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);

    // Calculate total duration
    double total_seconds =
        static_cast<double>(sfinfo.frames) / sfinfo.samplerate;

    // Buffer for audio data - we'll read as 16-bit samples
    std::vector<short> buffer(AUDIO_BUFFER_SIZE);

    // Display info
    std::string displayName = getCleanSongName(filename);
    std::cout << "\t\t" << "▶️ Now playing: " << displayName << std::endl;
    if (total_seconds > 0) {
      int minutes = static_cast<int>(total_seconds) / 60;
      double seconds_part = total_seconds - minutes * 60;
      std::cout << "\t\t" << "   Duration: " << minutes << ":" << std::fixed
                << std::setprecision(1) << std::setfill('0') << std::setw(4)
                << seconds_part << std::endl;
    }
    std::cout << "\t\t"
              << "   Controls: [Space] Play/Pause, [s] Stop, [j] -10s, [k] +10s"
              << std::endl;
    std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
              << std::endl;

    // Playback loop
    sf_count_t frames_read;
    sf_count_t total_frames = sfinfo.frames;
    sf_count_t frames_played = 0;
    sf_count_t current_frame_pos = 0;
    int last_percent = -1;
    bool playback_error = false;
    bool is_paused = false;
    bool should_stop = false;

    while (!should_stop) {
      // Check for keyboard input
      if (kbhit()) {
        char key = getch();
        switch (key) {
          case ' ':  // Space - Toggle pause/play
            is_paused = !is_paused;
            std::cout << "\r\t\t" << (is_paused ? "⏸️ Paused " : "▶️ Playing")
                      << "                                  " << std::endl;
            break;
          case 's':  // 's' - Stop playback
          case 'S':
            should_stop = true;
            std::cout << "\r\t\t⏹️ Stopped                                     "
                      << std::endl;
            break;
          case 'j':  // 'j' - Jump back 10 seconds
          {
            sf_count_t jump_frames = sfinfo.samplerate * 10;  // 10 seconds
            current_frame_pos = sf_seek(sndfile, -jump_frames, SEEK_CUR);
            if (current_frame_pos < 0) {
              sf_seek(sndfile, 0, SEEK_SET);
              current_frame_pos = 0;
            }
            frames_played = current_frame_pos;
            std::cout
                << "\r\t\t⏪ Jumped back 10s                               "
                << std::endl;
          } break;
          case 'k':  // 'k' - Jump forward 10 seconds
          {
            sf_count_t jump_frames = sfinfo.samplerate * 10;  // 10 seconds
            current_frame_pos = sf_seek(sndfile, jump_frames, SEEK_CUR);
            if (current_frame_pos < 0) {
              // Error in seeking, try to recover
              sf_seek(sndfile, 0, SEEK_SET);
              current_frame_pos = 0;
            }
            frames_played = current_frame_pos;
            std::cout
                << "\r\t\t⏩ Jumped forward 10s                            "
                << std::endl;
          } break;
        }
      }

      // If paused, skip decoding/output steps
      if (is_paused) {
        usleep(100000);  // 100ms pause to reduce CPU usage while paused
        continue;
      }

      frames_read = sf_readf_short(sndfile, buffer.data(),
                                   buffer.size() / sfinfo.channels);
      if (frames_read <= 0) {
        // End of file or error
        break;
      }

      size_t bytes_to_write = frames_read * sfinfo.channels * sizeof(short);

      if (pa_simple_write(pa_stream, buffer.data(), bytes_to_write,
                          &pa_error_code) < 0) {
        std::cerr << "\n\t\tError: PulseAudio write error: "
                  << pa_strerror(pa_error_code) << std::endl;
        playback_error = true;
        break;
      }

      // Update progress
      frames_played += frames_read;
      current_frame_pos =
          sf_seek(sndfile, 0, SEEK_CUR);  // Get current position

      if (total_frames > 0) {
        double fraction_complete =
            static_cast<double>(frames_played) / total_frames;
        int percent = static_cast<int>(fraction_complete * 100.0);
        percent = std::min(100, std::max(0, percent));

        if (percent != last_percent) {
          last_percent = percent;
          int progress_bar_width = 25;
          int pos = static_cast<int>(fraction_complete * progress_bar_width);
          pos = std::min(progress_bar_width, std::max(0, pos));

          std::cout << "\r\t\tProgress: [";
          for (int i = 0; i < progress_bar_width; ++i) {
            std::cout << (i < pos ? "■" : " ");
          }
          std::cout << "] " << percent << "% " << std::flush;
        }
      } else {
        std::cout << "\r\t\tPlaying... (duration unknown) " << std::flush;
      }
    }

    std::cout << std::endl;  // Final newline after progress bar

    // Cleanup
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);  // Restore terminal settings
    fcntl(STDIN_FILENO, F_SETFL, old_flags);     // Restore flags

    if (!playback_error && !should_stop) {
      if (pa_simple_drain(pa_stream, &pa_error_code) < 0) {
        std::cerr << "\t\tWarning: pa_simple_drain() failed: "
                  << pa_strerror(pa_error_code) << std::endl;
      }
      std::cout << "\t\t✓ Playback finished." << std::endl;
    } else if (should_stop) {
      std::cout << "\t\t⏹️ Playback stopped by user." << std::endl;
    } else {
      std::cerr << "\t\tPlayback stopped due to error." << std::endl;
    }

    if (pa_stream) pa_simple_free(pa_stream);
    sf_close(sndfile);

    return playback_error ? 1 : (should_stop ? 2 : 0);
  }
}

// --- Playlist Playback Mode Implementations ---

// --- With Controls Implementations ---

void playWithControls(LinkedList& list) {
  if (list.head == nullptr) {
    std::cout << "\t\t" << "⚠️ Playlist is empty. Nothing to play." << std::endl;
    return;
  }

  int n = list.len;
  node* current = list.head;

  std::cout << "\t\t" << "🎵 Playlist: "
            << (list.listName.empty() ? "[Unnamed]" : list.listName)
            << " (With Controls)" << std::endl;
  std::cout << "\t\t" << "📂 Total tracks: " << n << std::endl;
  std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            << std::endl;

  for (int i = 0; i < n; ++i) {
    if (current == nullptr) {
      std::cerr
          << "\t\tError: Encountered unexpected null node during playback."
          << std::endl;
      break;
    }

    std::string songPath = current->song;
    std::string cleanName = getCleanSongName(songPath);

    std::cout << "\t\t" << "🎧 Playing Track " << (i + 1) << "/" << n
              << std::endl;
    std::cout << "\t\t   Song: " << cleanName << std::endl;
    std::cout << "\t\t   Artist: " << current->artist << std::endl;

    int result = playerWithControls(songPath);

    // If result is 2, user chose to stop playback
    if (result == 2) {
      std::cout << "\t\t" << "⏹️ Playlist playback stopped by user."
                << std::endl;
      return;
    } else if (result != 0) {
      char choice;
      std::cout << "\t\t"
                << "❓ Error playing track. Continue with next? (Y/N): ";
      std::cin >> choice;
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      if (std::toupper(static_cast<unsigned char>(choice)) != 'Y') {
        std::cout << "\t\t" << "⏹️ Playback stopped by user." << std::endl;
        return;
      }
      std::cout << "\t\t" << "Skipping to next track..." << std::endl;
    }

    current = current->next;

    if (i < n - 1) {
      std::cout << "\t\t" << "Next track in 2 seconds..." << std::endl;
      std::cout << "\t\t" << "────────────────────────────────────────"
                << std::endl;
      sleep(2);
    }
  }

  std::cout << "\t\t" << "✅ Playlist playback complete!" << std::endl;
}

// Plays a specific song from the playlist selected by the user with controls
void playWithControlsSingle(LinkedList& li) {
  if (li.head == nullptr) {
    std::cout << "\t\t" << "⚠️ Playlist is empty. Cannot select a song to play."
              << std::endl;
    return;
  }

  std::cout << "\t\t" << "🎵 Playlist: "
            << (li.listName.empty() ? "[Unnamed]" : li.listName) << std::endl;
  li.display();  // Display list with numbers

  std::cout << "\t\t"
            << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            << std::endl;

  int choice = 0;
  while (true) {
    std::cout << "\t\t" << "📌 Enter track number to play (1-" << li.len
              << "): ";
    if (std::cin >> choice && choice >= 1 && choice <= li.len) {
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      break;
    } else {
      std::cout << "\t\t⚠️ Invalid input. Please enter a number between 1 and "
                << li.len << "." << std::endl;
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
  }

  node* current = li.head;
  for (int i = 1; i < choice; i++) {
    current = current->next;
  }

  if (current) {
    std::string songPath = current->song;
    std::string cleanName = getCleanSongName(songPath);

    std::cout << "\t\t" << "────────────────────────────────────────"
              << std::endl;
    std::cout << "\t\t" << "🎧 Selected track: " << cleanName << std::endl;
    std::cout << "\t\t" << "🎤 Artist: " << current->artist << std::endl;

    playerWithControls(songPath);  // Call the player with controls
  }
}

// Plays the playlist in reverse order with controls
void playWithControlsReverse(LinkedList& list) {
  if (list.head == nullptr) {
    std::cout << "\t\t" << "⚠️ Playlist is empty. Nothing to play in reverse."
              << std::endl;
    return;
  }

  std::cout << "\t\t" << "🎵 Playlist: "
            << (list.listName.empty() ? "[Unnamed]" : list.listName)
            << " (Reverse Playback with Controls)" << std::endl;
  std::cout << "\t\t" << "📂 Total tracks: " << list.len << std::endl;
  std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            << std::endl;

  Stack nodeStack;
  int n = list.len;
  node* temp = list.head;

  if (temp != nullptr) {
    do {
      nodeStack.push(temp);
      temp = temp->next;
    } while (temp != list.head);
  }

  std::cout << "\t\t" << "▶️ Starting playback in reverse order..." << std::endl;
  std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            << std::endl;

  node* nodeToPlay;
  int count = 1;
  bool userStopped = false;

  while ((nodeToPlay = nodeStack.pop()) != nullptr) {
    std::string songPath = nodeToPlay->song;
    std::string cleanName = getCleanSongName(songPath);

    std::cout << "\t\t" << "🎧 Playing track " << count++ << "/" << n
              << " (Reverse)" << std::endl;
    std::cout << "\t\t   Song: " << cleanName << std::endl;
    std::cout << "\t\t   Artist: " << nodeToPlay->artist << std::endl;

    int result = playerWithControls(songPath);

    // Check if user stopped playback
    if (result == 2) {
      std::cout << "\t\t" << "⏹️ Playback stopped by user." << std::endl;
      userStopped = true;
      break;
    } else if (result != 0) {
      char choice;
      std::cout << "\t\t"
                << "❓ Error playing track. Continue with next? (Y/N): ";
      std::cin >> choice;
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      if (std::toupper(static_cast<unsigned char>(choice)) != 'Y') {
        std::cout << "\t\t" << "⏹️ Playback stopped by user." << std::endl;
        userStopped = true;
        break;
      }
      std::cout << "\t\t" << "Skipping to next track (in reverse)..."
                << std::endl;
    }

    if (!nodeStack.isEmpty() && !userStopped) {
      std::cout << "\t\t" << "Next track in 2 seconds..." << std::endl;
      std::cout << "\t\t" << "────────────────────────────────────────"
                << std::endl;
      sleep(2);
    }
  }

  if (!userStopped) {
    std::cout << "\t\t" << "✅ Reverse playback complete!" << std::endl;
  }
}

// Plays the playlist multiple times with controls
void playWithControlsRepeat(LinkedList& list, int rounds) {
  if (list.head == nullptr) {
    std::cout << "\t\t" << "⚠️ Playlist is empty. Nothing to play." << std::endl;
    return;
  }
  if (rounds <= 0) {
    std::cout
        << "\t\t"
        << "Info: Number of rounds is zero or negative, skipping playback."
        << std::endl;
    return;
  }

  int n = list.len;
  node* temp = list.head;

  std::cout << "\t\t" << "🎵 Playlist: "
            << (list.listName.empty() ? "[Unnamed]" : list.listName)
            << " (Repeating " << rounds << " times with Controls)" << std::endl;
  std::cout << "\t\t" << "📂 Total tracks: " << n << std::endl;
  std::cout << "\t\t" << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            << std::endl;

  bool userStopped = false;

  for (int i = 0; i < n * rounds && !userStopped; ++i) {
    if (temp == nullptr) {
      std::cerr << "\t\tError: Reached null node unexpectedly in repeat mode."
                << std::endl;
      break;
    }
    std::string songPath = temp->song;
    std::string cleanName = getCleanSongName(songPath);

    int currentRound = (i / n) + 1;
    int trackInRound = (i % n) + 1;

    std::cout << "\t\t" << "🎧 Track " << trackInRound << "/" << n << " (Round "
              << currentRound << "/" << rounds << ")" << std::endl;
    std::cout << "\t\t   Song: " << cleanName << std::endl;
    std::cout << "\t\t   Artist: " << temp->artist << std::endl;

    int result = playerWithControls(songPath);

    // Check if user stopped playback
    if (result == 2) {
      std::cout << "\t\t" << "⏹️ Playback stopped by user." << std::endl;
      userStopped = true;
      break;
    } else if (result != 0) {
      char choice;
      std::cout << "\t\t"
                << "❓ Error playing track. Continue with next? (Y/N): ";
      std::cin >> choice;
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      if (std::toupper(static_cast<unsigned char>(choice)) != 'Y') {
        std::cout << "\t\t" << "⏹️ Playback stopped by user." << std::endl;
        userStopped = true;
        break;
      }
      std::cout << "\t\t" << "Skipping to next track..." << std::endl;
    }

    temp = temp->next;

    if (i < n * rounds - 1 && !userStopped) {
      std::cout << "\t\t" << "Next track in 2 seconds..." << std::endl;
      std::cout << "\t\t" << "────────────────────────────────────────"
                << std::endl;
      sleep(2);
    }
  }

  if (!userStopped) {
    std::cout << "\t\t" << "✅ Playlist repeat completed!" << std::endl;
  }
}