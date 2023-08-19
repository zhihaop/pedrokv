#ifndef PEDROKV_CLIENT_H
#define PEDROKV_CLIENT_H
#include "pedrokv/codec/client_codec.h"
#include "pedrokv/defines.h"
#include "pedrokv/options.h"

#include <pedrolib/concurrent/latch.h>
#include <pedronet/tcp_client.h>
#include <future>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace pedrokv {

using ResponseCallback = std::function<void(Response<>)>;

class ClientChannelHandler;
class Client : nonmovable, noncopyable {
  friend class ClientChannelHandler;

  pedronet::TcpClient client_;
  ClientOptions options_;

  std::mutex mu_;
  std::condition_variable not_full_;
  std::unordered_map<uint32_t, std::shared_ptr<ResponseCallback>> table_;
  std::atomic_uint32_t request_id_{};

  std::shared_ptr<Latch> open_latch_;
  std::shared_ptr<Latch> close_latch_;

  void handleResponse(const Response<>& responses);
  void handleClose();
  void handleConnect();
  void requestSend(Request<> request, uint32_t id, ResponseCallback callback);

 public:
  using Ptr = std::shared_ptr<Client>;

  Client(InetAddress address, ClientOptions options)
      : client_(std::move(address)), options_(std::move(options)) {
    client_.SetGroup(options_.worker_group);
  }

  ~Client() { Close(); }

  void Close() {
    if (close_latch_ != nullptr) {
      client_.Shutdown();
      close_latch_->Await();
    }
  }

  void Get(std::string_view key, ResponseCallback callback);

  void Put(std::string_view key, std::string_view value,
           ResponseCallback callback);

  void Delete(std::string_view key, ResponseCallback callback);

  void Start();
};

class SyncClient {
  InetAddress address_;
  ClientOptions options_;
  pedronet::SocketChannel channel_;
  std::atomic_uint32_t request_id_{};
  ArrayBuffer buffer_;

  mutable std::mutex mu_;

 public:
  using Ptr = std::shared_ptr<SyncClient>;

  SyncClient(InetAddress address, ClientOptions options)
      : address_(std::move(address)),
        options_(std::move(options)),
        channel_(pedronet::Socket::Create(address_.Family(), false)) {}

  void Start() {
    Error error = channel_.Connect(address_);
    if (error != Error::kOk) {
      PEDROKV_ERROR("failed to connect to address {}", address_);
    }
  }

  Response<> RequestResponse(RequestView request) {
    Response response;
    response.type = ResponseType::kError;

    std::string value;
    std::unique_lock lock{mu_};
    request.Pack(&buffer_);
    while (buffer_.ReadableBytes()) {
      if (buffer_.Retrieve(&channel_) < 0) {
        PEDROKV_ERROR("failed to send channel {} {}", channel_, Error{errno});
        return response;
      }
    }

    buffer_.EnsureWritable(response.SizeOf());
    while (!response.UnPack(&buffer_)) {
      if (buffer_.Append(&channel_) < 0) {
        PEDROKV_ERROR("failed to recv channel {} {}", channel_, Error{errno});
        return response;
      }
    }

    return response;
  }

  Response<> Get(std::string_view key) {
    RequestView request;
    request.type = RequestType::kGet;
    request.key = key;
    request.id = request_id_.fetch_add(1);

    return RequestResponse(request);
  }

  Response<> Put(std::string_view key, std::string_view value) {
    RequestView request;
    request.type = RequestType::kPut;
    request.key = key;
    request.value = value;
    request.id = request_id_.fetch_add(1);

    return RequestResponse(request);
  }

  Response<> Delete(std::string_view key) {
    RequestView request;
    request.type = RequestType::kDelete;
    request.key = key;
    request.id = request_id_.fetch_add(1);

    return RequestResponse(request);
  }

  void Close() {
    std::unique_lock lock{mu_};
    channel_.CloseWrite();
  }
};
}  // namespace pedrokv

#endif  // PEDROKV_CLIENT_H
