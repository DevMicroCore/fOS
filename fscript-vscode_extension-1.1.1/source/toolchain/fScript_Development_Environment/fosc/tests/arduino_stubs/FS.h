#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#define FILE_READ "r"
#define FILE_WRITE "w"

namespace fs {

class File {
 public:
  File() = default;
  explicit File(std::shared_ptr<std::vector<std::uint8_t>> data, bool writable = false)
    : data_(std::move(data)), writable_(writable) {}

  explicit operator bool() const { return data_ != nullptr; }
  bool isDirectory() const { return false; }
  File openNextFile() { return File(); }
  const char * name() const { return ""; }
  std::size_t size() const { return data_ == nullptr ? 0 : data_->size(); }
  bool seek(std::uint32_t position)
  {
    if (data_ == nullptr || position > data_->size()) return false;
    position_ = position;
    return true;
  }
  std::size_t read(std::uint8_t * destination, std::size_t count)
  {
    if (data_ == nullptr || destination == nullptr) return 0;
    const std::size_t available = data_->size() - position_;
    const std::size_t actual = count < available ? count : available;
    for (std::size_t index = 0; index < actual; ++index) {
      destination[index] = (*data_)[position_ + index];
    }
    position_ += actual;
    return actual;
  }
  std::size_t write(const std::uint8_t * source, std::size_t count)
  {
    if (data_ == nullptr || !writable_ || source == nullptr) return 0;
    data_->insert(data_->end(), source, source + count);
    position_ += count;
    return count;
  }
  void close() { data_.reset(); }

 private:
  std::shared_ptr<std::vector<std::uint8_t>> data_;
  std::size_t position_ = 0;
  bool writable_ = false;
};

class FS {
 public:
  void addFile(const std::string& path, std::vector<std::uint8_t> bytes)
  {
    files_[path] = std::make_shared<std::vector<std::uint8_t>>(std::move(bytes));
  }
  bool exists(const char * path) const
  {
    return path != nullptr && files_.find(path) != files_.end();
  }
  bool mkdir(const char *) { return true; }
  bool remove(const char *) { return true; }
  File open(const char * path, const char * mode)
  {
    ++openCount_;
    if (path != nullptr && mode != nullptr && std::string(mode) == FILE_WRITE) {
      auto data = std::make_shared<std::vector<std::uint8_t>>();
      files_[path] = data;
      return File(data, true);
    }
    const auto file = path == nullptr ? files_.end() : files_.find(path);
    return file == files_.end() ? File() : File(file->second);
  }
  int openCount() const { return openCount_; }
  void resetOpenCount() { openCount_ = 0; }

 private:
  std::map<std::string, std::shared_ptr<std::vector<std::uint8_t>>> files_;
  int openCount_ = 0;
};

}  // namespace fs
