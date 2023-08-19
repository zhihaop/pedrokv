#include <utility>

#include "pedrokv/client.h"

namespace pedrokv {

class ClientChannelHandler final : public ClientChannelCodec {
 public:
  void OnRequest(Timestamp now, Response<> response) final {
    client_->handleResponse(response);
  }
  void OnConnect(Timestamp now) final { client_->handleConnect(); }
  void OnClose(Timestamp now) final { client_->handleClose(); }
  ClientChannelHandler(ChannelContext::Ptr ctx, Client* client)
      : ClientChannelCodec(std::move(ctx)), client_(client) {}

 private:
  Client* client_;
};

Response<> ReturnError(uint32_t id, std::string msg) {
  Response response;
  response.id = id;
  response.type = ResponseType::kError;
  response.data = std::move(msg);
  return response;
}

void Client::requestSend(Request<> request, uint32_t id,
                         ResponseCallback callback) {

  auto ptr = std::make_shared<ResponseCallback>(std::move(callback));
  auto conn = client_.GetConnection();
  if (conn == nullptr) {
    (*ptr)(ReturnError(id, "client close"));
    return;
  }

  {
    std::unique_lock lock{mu_};
    while (table_.size() > options_.max_inflight) {
      not_full_.wait(lock);
    }

    if (table_.count(id)) {
      (*ptr)(ReturnError(id, "internal error"));
      return;
    }
    table_[id] = ptr;
  }

  conn->GetEventLoop().Schedule([this, id, ptr, request = std::move(request)] {
    auto conn = client_.GetConnection();
    if (conn == nullptr) {
      (*ptr)(ReturnError(id, "client close"));

      std::unique_lock lock{mu_};
      table_.erase(id);
      return;
    }

    auto ctx = conn->GetChannelContext();
    auto buffer = ctx->GetOutputBuffer();
    request.Pack(buffer);
    conn->Send(buffer);
  });
}

void Client::Start() {
  open_latch_ = std::make_shared<Latch>(1);
  close_latch_ = std::make_shared<Latch>(1);
  client_.SetBuilder([=](auto ctx) {
    return std::make_shared<ClientChannelHandler>(std::move(ctx), this);
  });

  client_.Start();
  open_latch_->Await();
}

void Client::Get(std::string_view key, ResponseCallback callback) {
  auto id = request_id_.fetch_add(1);
  Request request;
  request.type = RequestType::kGet;
  request.id = id;
  request.key = key;
  return requestSend(std::move(request), id, std::move(callback));
}

void Client::Put(std::string_view key, std::string_view value,
                 ResponseCallback callback) {
  auto id = request_id_.fetch_add(1);
  Request request;
  request.type = RequestType::kPut;
  request.id = id;
  request.key = key;
  request.value = value;
  return requestSend(std::move(request), id, std::move(callback));
}

void Client::Delete(std::string_view key, ResponseCallback callback) {
  auto id = request_id_.fetch_add(1);
  Request request;
  request.type = RequestType::kDelete;
  request.id = id;
  request.key = key;
  return requestSend(std::move(request), id, std::move(callback));
}

void Client::handleResponse(const Response<>& response) {
  std::unique_lock lock{mu_};

  auto it = table_.find(response.id);
  if (it == table_.end()) {
    return;
  }
  auto ptr = std::move(it->second);
  table_.erase(it);
  not_full_.notify_all();
  lock.unlock();

  (*ptr)(response);
}

void Client::handleClose() {
  std::unique_lock lock{mu_};
  for (auto& [id, ptr] : table_) {
    (*ptr)(ReturnError(id, "client closed"));
  }
  table_.clear();
  not_full_.notify_all();
  close_latch_->CountDown();
}

void Client::handleConnect() {
  open_latch_->CountDown();
}

}  // namespace pedrokv