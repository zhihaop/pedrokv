#ifndef PEDROKV_CODEC_CODEC_H
#define PEDROKV_CODEC_CODEC_H
#include <pedronet/callbacks.h>
#include <pedronet/tcp_connection.h>
#include "pedrokv/codec/request.h"
#include "pedrokv/codec/response.h"
#include "pedrokv/defines.h"
#include "pedrokv/logger/logger.h"

#include <utility>

namespace pedrokv {

class ServerChannelCodec : public ChannelHandlerAdaptor {
 public:
  explicit ServerChannelCodec(ChannelContext::Ptr ctx)
      : ChannelHandlerAdaptor(std::move(ctx)) {}

  virtual void OnRequest(Timestamp now, RequestView request) = 0;

  template <class T>
  void Send(const Response<T>& response) {
    response.Pack(GetContext().GetOutputBuffer());
  }

  void OnRead(Timestamp now, ArrayBuffer& buffer) override {
    while (true) {
      RequestView req;
      if (req.UnPack(&buffer)) {
        OnRequest(now, req);
        continue;
      }

      uint16_t content_length;
      PeekInt(&buffer, &content_length);
      buffer.EnsureWritable(content_length);
      break;
    }

    auto conn = GetConnection();
    if (conn != nullptr) {
      conn->Send(GetContext().GetOutputBuffer());
    }
  }
};
}  // namespace pedrokv

#endif  // PEDROKV_CODEC_CODEC_H
