#include <mruby.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <mruby/hash.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby/class.h>

#include <atomic>
#include <string>
#include <vector>
#include <utility>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <memory>

#include "../chan_src/variant.hpp"
#include "../chan_src/variant.cpp"
#include "../chan_src/channel.hpp"
#include "../chan_src/channel.cpp"


struct Routine {
  std::string script;
  Variant error;
  std::thread* th;
  std::atomic<bool> is_running;
  std::atomic<bool> got_error;

  Routine(const char* z) : script(z)
  {
    this->th = nullptr;
    this->is_running = false;
    this->got_error = false;
  }
  ~Routine()
  {
    if (this->th) {
      this->th->join();
      delete this->th;
    }
    this->error.free();
  }

  void join()
  {
    if (this->th) {
      this->th->join();
      delete this->th;
      this->th = nullptr;
    }
  }
};

static void
routine_free(mrb_state* mrb, void* p)
{
  Routine* r = static_cast<Routine*>(p);
  r->~Routine();
  mrb_free(mrb, p);
}

static mrb_data_type routine_t = {"Routine", routine_free};

static mrb_value
routine_init(mrb_state* mrb, mrb_value self)
{
  char* script;

  mrb_get_args(mrb, "z", &script);

  Routine* r = (Routine*)mrb_malloc(mrb, sizeof(Routine));
  r = new (r) Routine(script);

  DATA_TYPE(self) = &routine_t;
  DATA_PTR(self) = r;

  return self;
}

static mrb_value
routine_start(mrb_state* mrb, mrb_value self)
{
  Routine* r = static_cast<Routine*>(DATA_PTR(self));

  if (r->is_running) return mrb_nil_value();

  r->join();

  r->th = new std::thread([r]() {
    r->is_running = true;
    mrb_state* m = mrb_open();
    mrb_load_string(m, r->script.c_str());
    if (m->exc) {
      mrb_value v = mrb_funcall(m, mrb_obj_value(m->exc), "inspect", 0);
      r->error = Variant(Variant::Type::STRING, RSTRING_PTR(v), RSTRING_LEN(v));
      r->got_error = true;
    }
    mrb_close(m);
    r->is_running = false;
  });

  return mrb_nil_value();
}

static mrb_value
routine_get_error(mrb_state* mrb, mrb_value self)
{
  Routine* r = static_cast<Routine*>(DATA_PTR(self));
  if (r->got_error)
    return r->error.to_mruby(mrb);
  else
    return mrb_nil_value();
}

static mrb_value
routine_join(mrb_state* mrb, mrb_value self)
{
  Routine* r = static_cast<Routine*>(DATA_PTR(self));
  r->join();
  return mrb_nil_value();
}

static mrb_value
routine_is_running(mrb_state* mrb, mrb_value self)
{
  bool is_running = static_cast<Routine*>(DATA_PTR(self))->is_running;
  return mrb_bool_value(is_running);
}

static void
chan_free(mrb_state* mrb, void* p)
{
  static_cast<Channel*>(p)->release();
}

static mrb_data_type chan_t = {"Channel", chan_free};

static mrb_value
chan_get(mrb_state* mrb, mrb_value self)
{
  char* name;

  mrb_get_args(mrb, "z", &name);

  struct RClass* cls = mrb_class_get(mrb, "Channel");

  Channel* chan = Channel::get_channel(name);

  return mrb_obj_value(mrb_data_object_alloc(mrb, cls, chan, &chan_t));
}

static mrb_value
chan_send(mrb_state* mrb, mrb_value self)
{
  mrb_value obj;

  mrb_get_args(mrb, "o", &obj);

  Variant v = Variant::from_mruby(mrb, obj);
  if (v.type == Variant::Type::UNKNOWN) {
    mrb_raisef(mrb, E_TYPE_ERROR, "unknown type: %S", obj);
  }
  static_cast<Channel*>(DATA_PTR(self))->send(v);

  return mrb_nil_value();
}

static mrb_value
chan_try_recv(mrb_state* mrb, mrb_value self)
{
  mrb_value blk;

  mrb_get_args(mrb, "&", &blk);

  Variant v;
  Channel* chan = static_cast<Channel*>(DATA_PTR(self));

  if (chan->try_receive(&v)) {
    if (v.type == Variant::Type::CLOSE) {
      chan->close();
      return mrb_false_value();
    }

    mrb_value arg = v.to_mruby(mrb);
    mrb_yield_argv(mrb, blk, 1, &arg);
    v.free();

    return mrb_true_value();
  }

  return mrb_false_value();
}

static mrb_value
chan_recv(mrb_state* mrb, mrb_value self)
{
  mrb_value blk;

  mrb_get_args(mrb, "&", &blk);

  Variant v;
  Channel* chan = static_cast<Channel*>(DATA_PTR(self));

  if (chan->receive(&v)) {
    if (v.type == Variant::Type::CLOSE) {
      chan->close();
      return mrb_false_value();
    }

    mrb_value arg = v.to_mruby(mrb);
    mrb_yield_argv(mrb, blk, 1, &arg);
    v.free();

    return mrb_true_value();
  }

  return mrb_false_value();
}

static mrb_value
chan_close(mrb_state* mrb, mrb_value self)
{
  Variant v;
  v.type = Variant::Type::CLOSE;
  Channel* chan = static_cast<Channel*>(DATA_PTR(self));
  chan->send(v);
  return mrb_nil_value();
}

MRB_BEGIN_DECL

void
mrb_mruby_channel_gem_init(mrb_state* mrb)
{
  struct RClass* cls;

  cls = mrb_define_class(mrb, "Routine", mrb->object_class);
  MRB_SET_INSTANCE_TT(cls, MRB_TT_DATA);

  mrb_define_method(mrb, cls, "initialize", routine_init, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, cls, "start", routine_start, MRB_ARGS_NONE());
  mrb_define_method(mrb, cls, "get_error", routine_get_error, MRB_ARGS_NONE());
  mrb_define_method(mrb, cls, "join", routine_join, MRB_ARGS_NONE());
  mrb_define_method(mrb, cls, "running?", routine_is_running, MRB_ARGS_NONE());

  cls = mrb_define_class(mrb, "Channel", mrb->object_class);
  MRB_SET_INSTANCE_TT(cls, MRB_TT_DATA);

  mrb_define_class_method(mrb, cls, "get", chan_get, MRB_ARGS_REQ(1));

  mrb_define_method(mrb, cls, "send", chan_send, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, cls, "try_recv", chan_try_recv, MRB_ARGS_NONE() | MRB_ARGS_BLOCK());
  mrb_define_method(mrb, cls, "recv", chan_recv, MRB_ARGS_NONE() | MRB_ARGS_BLOCK());
  mrb_define_method(mrb, cls, "close", chan_close, MRB_ARGS_NONE());

}

void
mrb_mruby_channel_gem_final(mrb_state* mrb)
{
}

MRB_END_DECL
