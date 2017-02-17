std::mutex Channel::named_channels_mtx;
std::map<std::string, Channel*> Channel::named_channels;


Channel* Channel::get_channel(const std::string& name)
{
  std::lock_guard<std::mutex> lk(Channel::named_channels_mtx);

  auto it = Channel::named_channels.find(name);
  if (it != Channel::named_channels.end()) {
    it->second->add_ref();
    return it->second;
  }

  auto chan = new Channel();
  chan->name = name;
  Channel::named_channels[name] = chan;
  chan->add_ref();
  return chan;
}

Channel::Channel() : refcount(0), is_close(false)
{
}

Channel::~Channel()
{
  std::unique_lock<std::mutex> lk(this->named_channels_mtx);
  Channel::named_channels.erase(name);
}

void Channel::add_ref()
{
  this->refcount.fetch_add(1, std::memory_order_relaxed);
}

void Channel::release()
{
  if (this->refcount.fetch_sub(1, std::memory_order_release) == 1) {
    std::atomic_thread_fence(std::memory_order_acquire);
    delete this;
  }
}

bool Channel::send(Variant& val)
{
  std::lock_guard<std::mutex> lk(this->mtx);

  if (this->is_close) return false;

  this->queue.push(val);
  this->cv.notify_one();
  return true;
}

bool Channel::try_receive(Variant* ret)
{
  std::lock_guard<std::mutex> lk(this->mtx);

  if (this->is_close || this->queue.empty()) return false;

  std::swap(*ret, this->queue.front());
  this->queue.pop();
  return true;
}

bool Channel::receive(Variant* ret)
{
  std::unique_lock<std::mutex> lk(this->mtx);

  while(!this->is_close && this->queue.empty()) {
    this->cv.wait(lk);
  }

  if (this->is_close) return false;

  std::swap(*ret, this->queue.front());
  this->queue.pop();
  return true;
}

void Channel::close()
{
  this->is_close = true;
  this->cv.notify_all();
}
