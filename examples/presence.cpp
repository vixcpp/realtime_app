#include <realtime_app/realtime_app.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace vix::realtime_app;

/**
 * Minimal session for presence demo.
 */
class DemoSession : public RealtimeSession
{
public:
  DemoSession(std::string id, std::string user)
      : id_(std::move(id))
  {
    meta_.user_id = std::move(user);
  }

  std::string_view connection_id() const noexcept override { return id_; }

  void send_text(std::string text) override
  {
    std::cout << "[send to " << meta_.user_id << "] " << text << "\n";
  }

  void close(int code = 1000, std::string reason = "closed") override
  {
    std::cout << "[close " << meta_.user_id << "] code=" << code << " reason=" << reason << "\n";
  }

  SessionMeta &meta() noexcept override { return meta_; }
  const SessionMeta &meta() const noexcept override { return meta_; }

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

/**
 * Tiny in-memory presence store:
 * user_id -> online?
 */
class PresenceStore
{
public:
  void set_online(std::string user_id, bool v)
  {
    online_[std::move(user_id)] = v;
  }

  bool is_online(const std::string &user_id) const
  {
    auto it = online_.find(user_id);
    return it != online_.end() && it->second;
  }

private:
  std::unordered_map<std::string, bool> online_;
};

int main()
{
  RealtimeApplication app;
  PresenceStore presence;

  // Everyone joins the same presence topic so they can see online/offline changes.
  const std::string presence_topic = "presence:global";

  app.set_on_connect([&](RealtimeSession &s)
                     {
    presence.set_online(s.meta().user_id, true);

    app.join_topic(s, presence_topic);

    RealtimeEvent ev;
    ev.type = "presence";
    ev.topic = presence_topic;
    ev.id = "";
    ev.ts_ms = now_ms();
    ev.payload = std::string("{\"user_id\":\"") + vix::api_app::json_escape(s.meta().user_id) + "\",\"online\":true}";

    // notify everyone (including the newly connected session)
    app.broadcast_event_raw_payload(ev); });

  app.set_on_disconnect([&](const RealtimeSession &s)
                        {
    presence.set_online(s.meta().user_id, false);

    RealtimeEvent ev;
    ev.type = "presence";
    ev.topic = presence_topic;
    ev.id = "";
    ev.ts_ms = now_ms();
    ev.payload = std::string("{\"user_id\":\"") + vix::api_app::json_escape(s.meta().user_id) + "\",\"online\":false}";

    // broadcast offline update (the disconnected session is already removed)
    app.broadcast_event_raw_payload(ev); });

  DemoSession a("c1", "alice");
  DemoSession b("c2", "bob");

  std::cout << "connect alice\n";
  app.on_connected(a);

  std::cout << "connect bob\n";
  app.on_connected(b);

  std::cout << "alice online? " << (presence.is_online("alice") ? "yes" : "no") << "\n";
  std::cout << "bob online?   " << (presence.is_online("bob") ? "yes" : "no") << "\n";

  std::cout << "disconnect alice\n";
  app.on_disconnected(a);

  std::cout << "alice online? " << (presence.is_online("alice") ? "yes" : "no") << "\n";
  std::cout << "bob online?   " << (presence.is_online("bob") ? "yes" : "no") << "\n";

  std::cout << "disconnect bob\n";
  app.on_disconnected(b);

  return 0;
}
