#include "pedrokv/server.h"

namespace pedrokv {

class ServerChannelHandler final : public ServerChannelCodec {
 public:
  ServerChannelHandler(ChannelContext::Ptr ctx, Server* server)
      : ServerChannelCodec(std::move(ctx)), server_(server) {}

  void OnRequest(Timestamp now, RequestView request) override {
    Response<> response = server_->handleRequest(now, request);
    Send(std::move(response));
  }

 private:
  Server* server_;
};

pedrokv::Server::Server(pedronet::InetAddress address,
                        pedrokv::ServerOptions options)
    : address_(std::move(address)), options_(std::move(options)) {
  server_.SetGroup(options_.boss_group, options_.worker_group);

  auto stat = pedrodb::DB::Open(options_.db_options, options_.db_path, &db_);
  if (stat != pedrodb::Status::kOk) {
    PEDROKV_FATAL("failed to open db {}", options_.db_path);
  }

  server_.SetBuilder([this](auto ctx) {
    return std::make_shared<ServerChannelHandler>(std::move(ctx), this);
  });
}

Response<> Server::handleRequest(Timestamp /* now */, RequestView request) {
  Response response;
  response.id = request.id;
  response.type = ResponseType::kError;

  auto db = db_;
  if (db == nullptr) {
    response.data = "server closed";
    return response;
  }

  pedrodb::Status status = pedrodb::Status::kOk;
  switch (request.type) {
    case RequestType::kGet: {
      status = db->Get({}, request.key, &response.data);
      break;
    }
    case RequestType::kDelete: {
      status = db->Delete({}, request.key);
      break;
    }
    case RequestType::kPut: {
      status = db->Put({}, request.key, request.value);
      break;
    }
    default: {
      PEDROKV_WARN("invalid request receive, {}", (uint32_t)response.type);
      break;
    }
  }

  if (status != pedrodb::Status::kOk) {
    response.type = ResponseType::kError;
    response.data = fmt::format("err: {}", status);
  } else {
    response.type = ResponseType::kOk;
  }

  return response;
}

}  // namespace pedrokv