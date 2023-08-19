#ifndef PEDROKV_CODEC_CLIENT_CODEC_H
#define PEDROKV_CODEC_CLIENT_CODEC_H

#include <pedrolib/buffer/array_buffer.h>
#include <pedronet/callbacks.h>
#include <memory>
#include "pedrokv/codec/request.h"
#include "pedrokv/codec/response.h"
#include "pedrokv/logger/logger.h"

namespace pedrokv {

class ClientChannelCodec : public ChannelHandlerAdaptor {
 public:
  explicit ClientChannelCodec(ChannelContext::Ptr ctx)
      : ChannelHandlerAdaptor(std::move(ctx)) {}

  virtual void OnRequest(Timestamp now, Response<>) = 0;

  void OnRead(Timestamp now, ArrayBuffer& buffer) override {
    std::string value;
    while (true) {
      Response resp;
      if (resp.UnPack(&buffer)) {
        OnRequest(now, std::move(resp));
        continue;
      }

      uint16_t content_length;
      PeekInt(&buffer, &content_length);
      buffer.EnsureWritable(content_length);
      break;
    }
  }
};

}  // namespace pedrokv

#endif  // PEDROKV_CODEC_CLIENT_CODEC_H
