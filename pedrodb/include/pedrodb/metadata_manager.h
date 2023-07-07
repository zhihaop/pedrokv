#ifndef PEDRODB_METADATA_MANAGER_H
#define PEDRODB_METADATA_MANAGER_H

#include "pedrodb/metadata_format.h"
#include "pedrodb/status.h"

#include <mutex>
#include <pedrolib/buffer/array_buffer.h>
#include <unordered_set>

namespace pedrodb {

class MetadataManager {
  mutable std::mutex mu_;

  std::string name_;
  std::unordered_set<file_t> files_;

  File file_;
  const std::string path_;

  Status Recovery();

  Status CreateDatabase();

public:
  explicit MetadataManager(std::string path) : path_(std::move(path)) {}
  ~MetadataManager() = default;

  Status Init();

  auto AcquireLock() const noexcept { return std::unique_lock{mu_}; }

  const auto &GetFiles() const noexcept { return files_; }

  Status CreateFile(file_t id);

  Status DeleteFile(file_t id);

  std::string GetDataFilePath(file_t id) const noexcept;
};

} // namespace pedrodb

#endif // PEDRODB_METADATA_MANAGER_H
