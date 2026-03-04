#include <realtime_app/realtime_app.hpp>

#include <chrono>
#include <iostream>
#include <string>

using namespace vix::realtime_app;

class MemSession : public RealtimeSession
{
public:
  explicit MemSession(std::string id, std::string user)
      : id_(std::move(id))
  {
    meta_.user_id = std::move(user);
  }

  std::string_view connection_id() const noexcept override { return id_; }

  void send_text(std::string text) override
  {
    last = std::move(text);
    count++;
  }

  void close(int, std::string) override {}

  SessionMeta &meta() noexcept override { return meta_; }
  const SessionMeta &meta() const noexcept override { return meta_; }

  std::string last;
  int count = 0;

private:
  std::string id_;
  SessionMeta meta_;
};

static std::uint64_t now_ms()
{
  using Clock = std::chrono::system_clock;
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
  return static_cast<std::uint64_t>(ms.count());
}

int main()
{
  RealtimeApp app;

  MemSession a("a", "alice");
  MemSession b("b", "bob");
  MemSession c("c", "carol");

  app.on_connected(a);
  app.on_connected(b);
  app.on_connected(c);

  app.join_topic(a, "thread:1");
  app.join_topic(b, "thread:1");

  app.join_topic(c, "thread:2");

  RealtimeEvent ev;
  ev.type = "message";
  ev.topic = "thread:1";
  ev.id = "m1";
  ev.ts_ms = now_ms();
  ev.payload = "{\"text\":\"hi\"}";

  app.broadcast_event_raw_payload(ev);

  std::cout << "alice received: " << a.count << "\n";
  std::cout << "bob received:   " << b.count << "\n";
  std::cout << "carol received: " << c.count << "\n";

  app.on_disconnected(a);
  app.on_disconnected(b);
  app.on_disconnected(c);
  return 0;
}
