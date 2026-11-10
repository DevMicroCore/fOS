#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
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

  static File directory() { File f; f.directory_ = true; return f; }
  explicit operator bool() const { return data_ != nullptr || directory_; }
  bool isDirectory() const { return directory_; }
  File openNextFile() { return entries_ != nullptr && nextEntry_ < entries_->size() ? (*entries_)[nextEntry_++] : File(); }
  const char * name() const { return name_.c_str(); }
  void setName(const std::string& name) { name_ = name; }
  void setEntries(std::vector<File> entries) { entries_ = std::make_shared<std::vector<File>>(std::move(entries)); }
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
  void close() { data_.reset(); directory_ = false; }

 private:
  std::shared_ptr<std::vector<std::uint8_t>> data_;
  std::size_t position_ = 0;
  bool writable_ = false;
  bool directory_ = false;
  std::string name_;
  std::shared_ptr<std::vector<File>> entries_;
  std::size_t nextEntry_ = 0;
};

class FS {
 public:
  void addFile(const std::string& path, std::vector<std::uint8_t> bytes)
  {
    files_[path] = std::make_shared<std::vector<std::uint8_t>>(std::move(bytes));
    directories_.insert("/");
    for (std::size_t i = 1; i < path.size(); ++i) if (path[i] == '/') directories_.insert(path.substr(0, i));
  }
  bool exists(const char * path) const
  {
    return path != nullptr && (files_.find(path) != files_.end() || directories_.count(path));
  }
  bool mkdir(const char * path) { if(!path)return false; directories_.insert(path); return true; }
  bool remove(const char *) { return true; }
  File open(const char * path, const char * mode)
  {
    ++openCount_;
    if (path != nullptr && mode != nullptr && std::string(mode) == FILE_WRITE) {
      auto data = std::make_shared<std::vector<std::uint8_t>>();
      files_[path] = data;
      return File(data, true);
    }
    if (path != nullptr && directories_.count(path)) {
      File directory = File::directory(); directory.setName(path);
      const std::string prefix = std::string(path) == "/" ? "/" : std::string(path) + "/";
      std::vector<File> entries;
      for (const auto& folder : directories_) {
        if (folder.rfind(prefix, 0) == 0 && folder.size() > prefix.size() && folder.find('/', prefix.size()) == std::string::npos) {
          File entry = File::directory(); entry.setName(folder); entries.push_back(entry);
        }
      }
      for (const auto& item : files_) {
        if (item.first.rfind(prefix, 0) == 0 && item.first.find('/', prefix.size()) == std::string::npos) {
          File entry(item.second); entry.setName(item.first); entries.push_back(entry);
        }
      }
      directory.setEntries(std::move(entries)); return directory;
    }
    const auto file = path == nullptr ? files_.end() : files_.find(path);
    return file == files_.end() ? File() : File(file->second);
  }
  int openCount() const { return openCount_; }
  void resetOpenCount() { openCount_ = 0; }

 private:
  std::map<std::string, std::shared_ptr<std::vector<std::uint8_t>>> files_;
  int openCount_ = 0;
  std::set<std::string> directories_;
};

}  // namespace fs
