#include <utility>

#include "pedrokv/client.h"

namespace pedrokv {

class ClientChannelHandler final : public ClientChannelCodec {
 public:
  void OnRequest(Timestamp now, Response<> response) final {
    client_->handleResponse(std::move(response));
  }
  void OnConnect(Timestamp now) final { client_->handleConnect(); }
  void OnClose(Timestamp now) final { client_->handleClose(); }
  ClientChannelHandler(ChannelContext::Ptr ctx, Client* client)
      : ClientChannelCodec(std::move(ctx)), client_(client) {}

 private:
  Client* client_;
};

void Client::requestSend(Request<> request, uint32_t id,
                         ResponseCallback callback) {
  std::unique_lock lock{mu_};

  while (responses_.size() > options_.max_inflight) {
    not_full_.wait(lock);
  }

  if (responses_.count(id)) {
    Response response;
    response.id = id;
    response.type = ResponseType::kError;
    callback(response);
    return;
  }

  responses_[id] = std::move(callback);
  if (!client_.Write(std::move(request))) {
    Response response;
    response.id = id;
    response.type = ResponseType::kError;

    responses_[id](response);
    responses_.erase(id);
  }
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

void Client::handleResponse(Response<> response) {
  std::unique_lock lock{mu_};

  auto it = responses_.find(response.id);
  if (it == responses_.end()) {
    return;
  }

  auto callback = std::move(it->second);
  if (callback) {
    callback(std::move(response));
  }
  responses_.erase(it);

  not_full_.notify_all();
}

void Client::handleClose() {
  std::unique_lock lock{mu_};
  Response response;
  response.type = ResponseType::kError;
  response.data = "client closed";

  for (auto& [_, callback] : responses_) {
    if (callback) {
      callback(response);
    }
  }
  responses_.clear();
  not_full_.notify_all();
  close_latch_->CountDown();
}

void Client::handleConnect() {
  open_latch_->CountDown();
}

}  // namespace pedrokv