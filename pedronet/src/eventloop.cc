#include "pedronet/eventloop.h"

namespace pedronet {

void EventLoop::Loop() {
  PEDRONET_TRACE("EventLoop::Loop() running");
  owner_ = core::Thread::GetID();

  SelectChannels selected;
  while (state()) {
    auto err = selector_->Wait(kSelectTimeout, &selected);
    if (!err.Empty()) {
      PEDRONET_ERROR("failed to call selector_.Wait(): {}", err);
      continue;
    }

    size_t n_events = selected.channels.size();
    for (size_t i = 0; i < n_events; ++i) {
      Channel *ch = selected.channels[i];
      ReceiveEvents event = selected.events[i];
      ch->HandleEvents(event, selected.now);
    }

    std::unique_lock<std::mutex> lock(mu_);
    std::swap(running_tasks_, pending_tasks_);
    lock.unlock();

    for (auto &task : running_tasks_) {
      task();
    }

    running_tasks_.clear();
  }
  owner_.reset();
}
void EventLoop::Close() {
  state_ = 0;

  PEDRONET_TRACE("EventLoop is shutting down.");
  event_channel_.WakeUp();
  // TODO: await shutdown ?
}
void EventLoop::Schedule(Callback cb) {
  PEDRONET_TRACE("submit task");
  std::unique_lock<std::mutex> lock(mu_);

  bool wake_up = pending_tasks_.empty();
  pending_tasks_.emplace_back(std::move(cb));
  if (wake_up) {
    event_channel_.WakeUp();
  }
}
void EventLoop::AssertInsideLoop() const {
  if (!CheckInsideLoop()) {
    PEDRONET_ERROR("check in event loop failed, own={}, thd={}",
                   owner_.value_or(-1), core::Thread::GetID());
    std::terminate();
  }
}
void EventLoop::Register(Channel *channel, Callback callback) {
  PEDRONET_TRACE("EventLoopImpl::Register({})", *channel);
  if (!CheckInsideLoop()) {
    Schedule([this, channel, cb = std::move(callback)]() mutable {
      Register(channel, std::move(cb));
    });
    return;
  }
  auto it = channels_.find(channel);
  if (it == channels_.end()) {
    selector_->Add(channel, SelectEvents::kNoneEvent);
    if (callback) {
      callback();
    }
    channels_.emplace_hint(it, channel, std::move(callback));
  }
}
void EventLoop::Deregister(Channel *channel) {
  if (!CheckInsideLoop()) {
    Schedule([=] { Deregister(channel); });
    return;
  }

  PEDRONET_INFO("EventLoopImpl::Deregister({})", *channel);
  auto it = channels_.find(channel);
  if (it == channels_.end()) {
    return;
  }

  auto callback = std::move(it->second);
  selector_->Remove(channel);
  channels_.erase(it);

  if (callback) {
    callback();
  }
}
EventLoop::EventLoop(std::unique_ptr<Selector> selector)
    : selector_(std::move(selector)), timer_queue_(timer_channel_, *this) {
  selector_->Add(&event_channel_, SelectEvents::kReadEvent);
  selector_->Add(&timer_channel_, SelectEvents::kReadEvent);

  PEDRONET_TRACE("create event loop");
}
}