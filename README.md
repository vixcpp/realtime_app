# realtime_app

Realtime application kit for modern C++.

`realtime_app` provides a deterministic realtime foundation built on top of `vix/api_app`, which itself builds on `vix/web_app` and `vix/app`.

It provides the core primitives required for realtime systems such as:

- WebSocket session abstraction
- Topic / room membership
- Broadcast events
- Presence hooks
- SSE helpers
- Realtime event envelope

Header-only. Layered. Deterministic.

## Download

https://vixcpp.com/registry/pkg/vix/realtime_app

## Why realtime_app?

Modern applications require realtime communication:

- chat systems
- notifications
- live dashboards
- collaborative editing
- presence indicators
- event streaming

Most C++ backends either:

- implement ad-hoc websocket logic
- tightly couple networking with business logic
- lack structured event envelopes
- provide no room / topic abstraction

`realtime_app` provides:

- structured realtime event model
- topic / room membership
- deterministic broadcast primitives
- WebSocket session abstraction
- SSE formatting helpers
- presence lifecycle hooks

All without forcing:

- a networking stack
- a websocket library
- a threading model
- a JSON library

You plug your runtime.

`realtime_app` handles the logic.

## Dependency

`realtime_app` depends on:

- `vix/api_app`
- `vix/web_app`
- `vix/app`

Layered architecture:

```
vix/app
  ↑
vix/web_app
  ↑
vix/api_app
  ↑
vix/realtime_app
```

This ensures:

- clean module boundaries
- no circular dependencies
- deterministic layering
- composable runtime architecture

When installed via Vix Registry, dependencies are installed automatically.

## Installation

### Using Vix Registry

```bash
vix add vix/realtime_app
vix deps
```

This installs automatically:

- `vix/api_app`
- `vix/web_app`
- `vix/app`

### Manual

```bash
git clone https://github.com/vixcpp/realtime_app.git
```

Add the `include/` directory to your include path.

Ensure `api_app`, `web_app`, and `app` are available.

## Quick example

```cpp
#include <realtime_app/realtime_app.hpp>
#include <iostream>

using namespace vix::realtime_app;

class Session : public RealtimeSession
{
public:
    std::string_view connection_id() const noexcept override { return "1"; }

    void send_text(std::string text) override
    {
        std::cout << text << std::endl;
    }

    void close(int, std::string) override {}

    SessionMeta &meta() noexcept override { return meta_; }
    const SessionMeta &meta() const noexcept override { return meta_; }

private:
    SessionMeta meta_;
};

int main()
{
    RealtimeApplication app;

    Session s;

    app.on_connected(s);
    app.join_topic(s, "chat");

    RealtimeEvent ev;
    ev.type = "message";
    ev.topic = "chat";
    ev.payload = "{\"text\":\"hello\"}";
    ev.ts_ms = 0;

    app.broadcast_event_raw_payload(ev);

    app.on_disconnected(s);
}
```

## Event model

Realtime messages use a structured event envelope:

```json
{
  "type": "message",
  "topic": "thread:123",
  "id": "event_id",
  "ts_ms": 1700000000000,
  "payload": {
    "text": "hello"
  }
}
```

Fields:

- `type` -> event type
- `topic` -> broadcast room
- `id` -> optional identifier
- `ts_ms` -> timestamp
- `payload` -> event data

Payload may be:

- JSON object
- JSON string
- arbitrary text

## Topics / rooms

Sessions can join or leave topics:

```cpp
app.join_topic(session, "thread:123");
app.leave_topic(session, "thread:123");
```

Broadcast to all members:

```cpp
app.broadcast_event_raw_payload(event);
```

Exclude sender:

```cpp
app.broadcast_event_raw_payload(event, session.connection_id());
```

## Presence hooks

`realtime_app` exposes connection lifecycle hooks:

```cpp
app.set_on_connect([](RealtimeSession& s) {
    std::cout << "user connected: " << s.meta().user_id << std::endl;
});

app.set_on_disconnect([](const RealtimeSession& s) {
    std::cout << "user disconnected\n";
});
```

Common use cases:

- presence tracking
- online indicators
- analytics
- connection metrics

## Message handling

Incoming frames are handled with:

```cpp
app.set_on_message([](RealtimeSession& s, std::string text) {
    std::cout << "message: " << text << std::endl;
});
```

Your runtime forwards frames to:

```cpp
app.on_text_message(session, text);
```

## SSE helpers

`realtime_app` also supports Server-Sent Events formatting:

```cpp
auto chunk = sse_format(
    "notify",
    "42",
    "{\"title\":\"hello\"}"
);
```

To open an SSE stream:

```cpp
auto res = RealtimeApplication::sse_open();
```

Headers set automatically:

- `content-type: text/event-stream`
- `cache-control: no-cache`
- `connection: keep-alive`

## API overview

Core types:

- `vix::realtime_app::RealtimeApplication`
- `vix::realtime_app::RealtimeSession`
- `vix::realtime_app::RealtimeEvent`
- `vix::realtime_app::SessionMeta`

Helpers:

- `encode_event_json_raw_payload()`
- `encode_event_json_string_payload()`
- `sse_format()`

## Design philosophy

`realtime_app` focuses on:

- deterministic realtime primitives
- runtime-agnostic design
- explicit event model
- composable topic broadcast
- minimal abstraction

It does not provide:

- websocket networking implementation
- distributed pub/sub
- persistence
- authentication system
- message queue

Those belong to:

- runtime layer
- infrastructure layer
- higher-level frameworks

## Complexity

Typical operations:

- topic join / leave -> O(1)
- broadcast -> O(n) per topic member
- event encoding -> O(n) payload size
- session lookup -> O(1) average

The module is optimized for:

- deterministic behavior
- low overhead
- predictable performance

## Tests

Run:

```bash
vix build
vix test
```

Tests verify:

- event encoding
- topic membership
- broadcast behavior
- message hooks
- presence lifecycle

## License

MIT License\
Copyright (c) Gaspard Kirira

