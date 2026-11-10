#pragma once

#include <stddef.h>
#include <string.h>

// One SD path policy for native storage/audio and textarea load/save.
// Normalize before checking the protected directory, never pass '..' to FS.
namespace FAppPath {
enum class Status { Ready, Invalid, TooLong, Protected };

inline bool isSystemName(const char * name, size_t length)
{
  const char protectedName[] = "system";
  if (name == nullptr || length != sizeof(protectedName) - 1) return false;
  for (size_t i = 0; i < length; ++i) {
    const char lower = name[i] >= 'A' && name[i] <= 'Z' ? name[i] + ('a' - 'A') : name[i];
    if (lower != protectedName[i]) return false;
  }
  return true;
}

inline Status append(const char * input, char * output, size_t capacity, size_t& used)
{
  const char * cursor = input;
  while (*cursor != '\0') {
    while (*cursor == '/') ++cursor;
    const char * segment = cursor;
    while (*cursor != '\0' && *cursor != '/') {
      const unsigned char c = static_cast<unsigned char>(*cursor);
      if (c < 0x20 || c == 0x7F || *cursor == '\\' || *cursor == ':' ||
          *cursor == '"' || *cursor == '<' || *cursor == '>' || *cursor == '|' ||
          *cursor == '?' || *cursor == '*') return Status::Invalid;
      ++cursor;
    }
    const size_t length = static_cast<size_t>(cursor - segment);
    if (length == 0) break;
    if (length == 1 && segment[0] == '.') continue;
    if (length == 2 && segment[0] == '.' && segment[1] == '.') {
      while (used > 1 && output[used - 1] != '/') --used;
      if (used > 1) --used;
      output[used] = '\0';
      continue;
    }
    // Reject ambiguous FAT-style names instead of allowing a filesystem to
    // reinterpret a protected component with spaces or trailing dots.
    if (segment[0] == ' ' || segment[length - 1] == ' ' || segment[length - 1] == '.')
      return Status::Invalid;
    const size_t separator = used > 1 ? 1 : 0;
    if (used + separator + length >= capacity) return Status::TooLong;
    if (separator != 0) output[used++] = '/';
    memcpy(output + used, segment, length);
    used += length;
    output[used] = '\0';
  }
  return Status::Ready;
}

inline Status resolve(const char * appDirectory, const char * input, char * output, size_t capacity)
{
  if (input == nullptr || input[0] == '\0' || output == nullptr || capacity < 2)
    return Status::Invalid;
  output[0] = '/';
  output[1] = '\0';
  size_t used = 1;
  if (input[0] != '/') {
    if (appDirectory == nullptr || appDirectory[0] != '/') return Status::Invalid;
    const Status status = append(appDirectory, output, capacity, used);
    if (status != Status::Ready) return status;
  }
  const Status status = append(input, output, capacity, used);
  if (status != Status::Ready) return status;
  const char * slash = strchr(output + 1, '/');
  const size_t firstLength = slash == nullptr ? used - 1 : static_cast<size_t>(slash - output - 1);
  return isSystemName(output + 1, firstLength) ? Status::Protected : Status::Ready;
}
}
