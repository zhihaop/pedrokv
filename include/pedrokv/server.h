#ifndef PEDROKV_KV_SERVER_H
#define PEDROKV_KV_SERVER_H

#include "pedrodb/segment_db.h"
#include "pedrokv/codec/server_codec.h"
#include "pedrokv/defines.h"
#include "pedrokv/logger/logger.h"
#include "pedrokv/options.h"

#include <pedrodb/db.h>
#include <pedronet/tcp_server.h>
#include <memory>
#include <utility>

namespace pedrokv {

class ServerChannelHandler;
class Server : nonmovable,
               noncopyable,
               public std::enable_shared_from_this<Server> {
  
  friend class ServerChannelHandler;
  pedronet::TcpServer server_;
  pedronet::InetAddress address_;
  ServerOptions options_;

  pedrodb::DB::Ptr db_;
  
  Response<> handleRequest(Timestamp now, RequestView request);
  
 public:
  Server(pedronet::InetAddress address, ServerOptions options);

  void Bind() {
    server_.Bind(address_);
    PEDROKV_INFO("server bind success: {}", address_);
  }

  void Start() {
    server_.Start();
    PEDROKV_INFO("server start");
  }

  void Close() {
    db_.reset();
    server_.Close();
  }
};

}  // namespace pedrokv

#endif  // PEDROKV_KV_SERVER_H
