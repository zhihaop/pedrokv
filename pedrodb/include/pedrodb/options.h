#ifndef PEDRODB_OPTIONS_H
#define PEDRODB_OPTIONS_H

#include "pedrodb/defines.h"
#include <pedrolib/executor/thread_pool_executor.h>
#include <string>

namespace pedrodb {
struct Options {
  int8_t max_open_files = 16;
  size_t read_cache_bytes = kMaxFileBytes;
  size_t compaction_threshold_bytes = kMaxFileBytes / 2;
  size_t compaction_batch_bytes = 4 << 20;
  Duration sync_interval = Duration::Seconds(10);

  std::shared_ptr<Executor> executor{std::make_shared<DefaultExecutor>()};;
};

struct ReadOptions {};
struct WriteOptions {
  bool sync = false;
};
} // namespace pedrodb

#endif // PEDRODB_OPTIONS_H