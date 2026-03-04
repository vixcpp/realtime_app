#include <realtime_app/realtime_app.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace vix::realtime_app;

/**
 * Tiny fake WS runtime:
 * - validates handshake
 * - creates a session
 * - feeds text frames
 * - prints what the app sends
 */
class ConsoleSession : public RealtimeSession
{
public:
  explicit ConsoleSession(std::string id) : id_(std::move(id)) {}

  std::string_view connection_id() const noexcept override { return id_; }

  void send_text(std::string text) override
  {
    std::cout << "[send to " << id_ << "] " << text << "\n";
  }

  void close(int code = 1000, std::string reason = "closed") override
  {
    std::cout << "[close " << id_ << "] code=" << code << " reason=" << reason << "\n";
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

int main()
{
  RealtimeApplication app;

  // Handshake auth: allow only if header "x-token" == "dev"
  app.set_handshake_auth_hook([](const vix::web_app::Request &req, SessionMeta &meta) -> PolicyDecision
                              {
    auto it = req.headers.find("x-token");
    if (it == req.headers.end() || it->second != "dev")
      return PolicyDecision::deny(vix::api_app::ApiApplication::unauthorized("bad token", "bad_token"));

    meta.user_id = "user_1";
    meta.token_id = "token_dev";
    return PolicyDecision::allow(); });

  // App-level message handler: join + broadcast
  app.set_on_message([&](RealtimeSession &s, std::string text)
                     {
    if (text == "join:room1")
    {
      app.join_topic(s, "room1");
      s.send_text("{\"type\":\"system\",\"payload\":\"joined room1\"}");
      return;
    }

    RealtimeEvent ev;
    ev.type = "message";
    ev.topic = "room1";
    ev.id = "";
    ev.ts_ms = now_ms();
    ev.payload = std::string("{\"from\":\"") + std::string(s.meta().user_id) + "\",\"text\":\"" + vix::api_app::json_escape(text) + "\"}";

    app.broadcast_event_raw_payload(ev, s.connection_id()); });

  // "Incoming" handshake request
  vix::web_app::Request req;
  req.path = "/ws";
  req.headers["x-token"] = "dev";

  SessionMeta meta;
  PolicyDecision d = app.validate_handshake(req, meta);
  if (!d.allowed)
  {
    std::cout << "handshake denied: status=" << (d.response ? d.response->status : 0) << "\n";
    return 0;
  }

  ConsoleSession s("conn_1");
  s.meta() = meta;

  app.on_connected(s);

  // Simulate frames
  app.on_text_message(s, "join:room1");
  app.on_text_message(s, "hello realtime");

  // simulate runtime tick
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  app.on_disconnected(s);
  return 0;
}
