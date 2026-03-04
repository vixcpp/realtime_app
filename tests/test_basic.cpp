#include <realtime_app/realtime_app.hpp>

#include <cassert>
#include <iostream>
#include <string>

using namespace vix::realtime_app;

/**
 * Dummy session used for testing without a real websocket runtime.
 */
class DummySession : public RealtimeSession
{
public:
  explicit DummySession(std::string id) : id_(std::move(id)) {}

  std::string_view connection_id() const noexcept override
  {
    return id_;
  }

  void send_text(std::string text) override
  {
    last_message = std::move(text);
    sent_count++;
  }

  void close(int code = 1000, std::string reason = "closed") override
  {
    closed = true;
    close_code = code;
    close_reason = std::move(reason);
  }

  SessionMeta &meta() noexcept override
  {
    return meta_;
  }

  const SessionMeta &meta() const noexcept override
  {
    return meta_;
  }

public:
  std::string last_message;
  int sent_count = 0;

  bool closed = false;
  int close_code = 0;
  std::string close_reason;

private:
  std::string id_;
  SessionMeta meta_;
};

/**
 * Basic event broadcast test.
 */
void test_topic_broadcast()
{
  RealtimeApp app;

  DummySession s1("s1");
  DummySession s2("s2");

  app.on_connected(s1);
  app.on_connected(s2);

  app.join_topic(s1, "room1");
  app.join_topic(s2, "room1");

  RealtimeEvent ev;
  ev.type = "message";
  ev.topic = "room1";
  ev.id = "1";
  ev.ts_ms = 123;
  ev.payload = "{\"text\":\"hello\"}";

  app.broadcast_event_raw_payload(ev);

  assert(s1.sent_count == 1);
  assert(s2.sent_count == 1);

  std::cout << "broadcast ok\n";
}

/**
 * Test exclude sender logic.
 */
void test_exclude_sender()
{
  RealtimeApp app;

  DummySession s1("s1");
  DummySession s2("s2");

  app.on_connected(s1);
  app.on_connected(s2);

  app.join_topic(s1, "chat");
  app.join_topic(s2, "chat");

  RealtimeEvent ev;
  ev.type = "message";
  ev.topic = "chat";
  ev.id = "2";
  ev.ts_ms = 456;
  ev.payload = "{\"text\":\"hello\"}";

  app.broadcast_event_raw_payload(ev, "s1");

  assert(s1.sent_count == 0);
  assert(s2.sent_count == 1);

  std::cout << "exclude sender ok\n";
}

/**
 * Test message hook.
 */
void test_on_message_hook()
{
  RealtimeApp app;

  DummySession s1("s1");

  bool called = false;

  app.set_on_message([&](RealtimeSession &, std::string text)
                     {
        called = true;
        assert(text == "hello"); });

  app.on_connected(s1);
  app.on_text_message(s1, "hello");

  assert(called);

  std::cout << "message hook ok\n";
}

int main()
{
  test_topic_broadcast();
  test_exclude_sender();
  test_on_message_hook();

  std::cout << "realtime_app basic tests passed\n";
}
