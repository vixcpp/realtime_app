/**
 * @file realtime_app.hpp
 * @brief Realtime application kit built on top of vix/api_app for WebSocket, SSE, events, and presence.
 *
 * `realtime_app` provides a minimal realtime foundation for JSON-first systems:
 *
 * - Event envelope (type, topic, id, timestamp, payload)
 * - WebSocket session abstraction (send/close, user metadata)
 * - Simple room/topic membership (join/leave/broadcast)
 * - Auth hook for handshake and per-message authorization
 * - Heartbeat policy (ping interval, idle timeout)
 * - SSE stream helper (server-sent events formatting)
 * - Presence hooks (on_connect/on_disconnect)
 *
 * This kit is intentionally lightweight:
 * - No mandatory network backend (no socket implementation)
 * - No mandatory JSON library (payload is string)
 * - No threading model required
 *
 * You bind it to your runtime by implementing a small transport interface.
 *
 * Requirements: C++17+
 * Header-only. Depends on `vix/api_app`.
 */

#ifndef VIX_REALTIME_APP_REALTIME_APP_HPP
#define VIX_REALTIME_APP_REALTIME_APP_HPP

#include <api_app/api_app.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vix::realtime_app
{

  /**
   * @brief Minimal event envelope used for realtime messages.
   *
   * Payload is an opaque string (typically JSON) to avoid mandatory JSON deps.
   */
  struct RealtimeEvent
  {
    std::string type;    // e.g. "message", "typing", "notify"
    std::string topic;   // room/topic name, e.g. "thread:123"
    std::string id;      // optional unique id (caller-provided)
    std::uint64_t ts_ms; // epoch millis (caller/runtime provided)
    std::string payload; // opaque (usually JSON string)
  };

  /**
   * @brief Escape a string for JSON output (minimal subset).
   *
   * Keep consistent with api_app style: deterministic minimal escaping.
   */
  inline std::string json_escape(std::string_view s)
  {
    return vix::api_app::json_escape(s);
  }

  /**
   * @brief Encode a RealtimeEvent as a JSON string.
   *
   * Shape:
   * {
   *   "type":"...",
   *   "topic":"...",
   *   "id":"...",
   *   "ts_ms":123,
   *   "payload":<raw string JSON> | "<escaped string>"
   * }
   *
   * Note:
   * - payload is emitted as a JSON string value by default.
   * - if you want raw JSON object payload, pass payload that already contains JSON
   *   and use encode_event_json_raw_payload().
   */
  inline std::string encode_event_json_string_payload(const RealtimeEvent &e)
  {
    std::string out;
    out.reserve(e.type.size() + e.topic.size() + e.id.size() + e.payload.size() + 96);

    out += "{\"type\":\"";
    out += json_escape(e.type);
    out += "\",\"topic\":\"";
    out += json_escape(e.topic);
    out += "\",\"id\":\"";
    out += json_escape(e.id);
    out += "\",\"ts_ms\":";
    out += std::to_string(e.ts_ms);
    out += ",\"payload\":\"";
    out += json_escape(e.payload);
    out += "\"}";

    return out;
  }

  /**
   * @brief Encode a RealtimeEvent as JSON but treat payload as raw JSON.
   *
   * Payload is inserted without quotes. It must be valid JSON.
   */
  inline std::string encode_event_json_raw_payload(const RealtimeEvent &e)
  {
    std::string out;
    out.reserve(e.type.size() + e.topic.size() + e.id.size() + e.payload.size() + 96);

    out += "{\"type\":\"";
    out += json_escape(e.type);
    out += "\",\"topic\":\"";
    out += json_escape(e.topic);
    out += "\",\"id\":\"";
    out += json_escape(e.id);
    out += "\",\"ts_ms\":";
    out += std::to_string(e.ts_ms);
    out += ",\"payload\":";
    out += (e.payload.empty() ? "null" : e.payload);
    out += "}";

    return out;
  }

  /**
   * @brief SSE (Server-Sent Events) formatting helper.
   *
   * Produces a single SSE event chunk:
   * event: <type>\n
   * id: <id>\n
   * data: <data>\n
   * \n
   *
   * Caller is responsible for streaming this over a chunked response.
   */
  inline std::string sse_format(std::string_view type,
                                std::string_view id,
                                std::string_view data)
  {
    std::string out;
    out.reserve(type.size() + id.size() + data.size() + 32);

    out += "event: ";
    out += type;
    out += "\n";

    if (!id.empty())
    {
      out += "id: ";
      out += id;
      out += "\n";
    }

    // SSE requires each data line to start with "data: ".
    // If data contains newlines, split.
    out += "data: ";
    for (char c : data)
    {
      out += c;
      if (c == '\n')
        out += "data: ";
    }
    out += "\n\n";
    return out;
  }

  /**
   * @brief Heartbeat configuration for sessions.
   */
  struct HeartbeatPolicy
  {
    std::chrono::milliseconds ping_interval{25000};
    std::chrono::milliseconds idle_timeout{90000};
  };

  /**
   * @brief Minimal session metadata.
   *
   * user_id can be set by auth hook, token parser, etc.
   */
  struct SessionMeta
  {
    std::string user_id;
    std::string token_id;
    std::unordered_map<std::string, std::string> tags;
  };

  /**
   * @brief Transport-facing realtime session interface.
   *
   * Your runtime implements this interface for each active connection.
   * `realtime_app` never does IO; it only calls send/close and manages rooms.
   */
  class RealtimeSession
  {
  public:
    virtual ~RealtimeSession() = default;

    /// Stable connection id for maps and room membership.
    virtual std::string_view connection_id() const noexcept = 0;

    /// Send a text message to the client.
    virtual void send_text(std::string text) = 0;

    /// Close the session.
    virtual void close(int code = 1000, std::string reason = "closed") = 0;

    /// Access mutable metadata.
    virtual SessionMeta &meta() noexcept = 0;

    /// Access const metadata.
    virtual const SessionMeta &meta() const noexcept = 0;
  };

  /**
   * @brief Decision result for auth/policy checks.
   *
   * If denied, `response` can be used for HTTP handshake failures (401/403).
   * For message-level denial, you can return deny() and the caller decides how to notify.
   */
  struct PolicyDecision
  {
    bool allowed = true;
    std::optional<vix::web_app::Response> response;

    static PolicyDecision allow()
    {
      return PolicyDecision{true, std::nullopt};
    }

    static PolicyDecision deny(vix::web_app::Response r)
    {
      PolicyDecision d;
      d.allowed = false;
      d.response = std::move(r);
      return d;
    }
  };

  /**
   * @brief In-memory topic membership index.
   *
   * Deterministic, minimal, single-process. Not intended as distributed presence.
   */
  class TopicIndex
  {
  public:
    void join(std::string_view topic, std::string_view connection_id)
    {
      topics_[std::string(topic)].insert(std::string(connection_id));
    }

    void leave(std::string_view topic, std::string_view connection_id)
    {
      const auto it = topics_.find(std::string(topic));
      if (it == topics_.end())
        return;
      it->second.erase(std::string(connection_id));
      if (it->second.empty())
        topics_.erase(it);
    }

    void leave_all(std::string_view connection_id)
    {
      const std::string cid(connection_id);
      std::vector<std::string> empty;

      for (auto &kv : topics_)
      {
        kv.second.erase(cid);
        if (kv.second.empty())
          empty.push_back(kv.first);
      }

      for (const auto &t : empty)
        topics_.erase(t);
    }

    std::vector<std::string> members(std::string_view topic) const
    {
      std::vector<std::string> out;
      const auto it = topics_.find(std::string(topic));
      if (it == topics_.end())
        return out;

      out.reserve(it->second.size());
      for (const auto &cid : it->second)
        out.push_back(cid);
      return out;
    }

  private:
    std::unordered_map<std::string, std::unordered_set<std::string>> topics_;
  };

  /**
   * @brief Realtime application built on top of vix/api_app.
   *
   * This class provides policy hooks, topic membership, and helpers.
   * It is runtime-agnostic: you call the methods from your WebSocket/SSE runtime.
   */
  class RealtimeApp
  {
  public:
    using HandshakeAuthHook = std::function<PolicyDecision(const vix::web_app::Request &, SessionMeta &)>;
    using MessageAuthHook = std::function<PolicyDecision(const RealtimeSession &, const std::string &text)>;

    using OnConnectHook = std::function<void(RealtimeSession &)>;
    using OnDisconnectHook = std::function<void(const RealtimeSession &)>;

    /// Called when a valid message is received. User handles parsing and logic.
    using OnMessageHook = std::function<void(RealtimeSession &, std::string text)>;

    RealtimeApp() = default;
    ~RealtimeApp() = default;

    RealtimeApp(const RealtimeApp &) = delete;
    RealtimeApp &operator=(const RealtimeApp &) = delete;

    RealtimeApp(RealtimeApp &&) = delete;
    RealtimeApp &operator=(RealtimeApp &&) = delete;

    void set_heartbeat_policy(HeartbeatPolicy p) { hb_ = p; }

    void set_handshake_auth_hook(HandshakeAuthHook h) { handshake_auth_ = std::move(h); }
    void set_message_auth_hook(MessageAuthHook h) { message_auth_ = std::move(h); }

    void set_on_connect(OnConnectHook h) { on_connect_ = std::move(h); }
    void set_on_disconnect(OnDisconnectHook h) { on_disconnect_ = std::move(h); }
    void set_on_message(OnMessageHook h) { on_message_ = std::move(h); }

    /**
     * @brief Validate a WebSocket handshake request.
     *
     * Runtime flow suggestion:
     * - Create a SessionMeta meta{}
     * - Call validate_handshake(req, meta)
     * - If allowed: accept WS and attach meta to session
     * - If denied: return HTTP response to client
     */
    PolicyDecision validate_handshake(const vix::web_app::Request &req, SessionMeta &meta) const
    {
      if (handshake_auth_)
        return handshake_auth_(req, meta);

      return PolicyDecision::allow();
    }

    /**
     * @brief Register a session as connected (presence hook).
     *
     * Runtime should call this after accepting the WS connection.
     */
    void on_connected(RealtimeSession &s)
    {
      sessions_[std::string(s.connection_id())] = &s;

      // Note: membership is explicit (join_topic). We do not auto-join anything.
      if (on_connect_)
        on_connect_(s);
    }

    /**
     * @brief Unregister a session (presence hook).
     *
     * Runtime should call this on disconnect.
     */
    void on_disconnected(const RealtimeSession &s)
    {
      topic_index_.leave_all(s.connection_id());
      sessions_.erase(std::string(s.connection_id()));

      if (on_disconnect_)
        on_disconnect_(s);
    }

    /**
     * @brief Join a topic/room.
     */
    void join_topic(RealtimeSession &s, std::string_view topic)
    {
      topic_index_.join(topic, s.connection_id());
    }

    /**
     * @brief Leave a topic/room.
     */
    void leave_topic(RealtimeSession &s, std::string_view topic)
    {
      topic_index_.leave(topic, s.connection_id());
    }

    /**
     * @brief Broadcast a text message to a topic members.
     *
     * Sender can be excluded by passing exclude_connection_id.
     */
    void broadcast_text(
        std::string_view topic,
        std::string text,
        std::string_view exclude_connection_id = {}) const
    {
      const auto members = topic_index_.members(topic);
      for (const auto &cid : members)
      {
        if (!exclude_connection_id.empty() && cid == exclude_connection_id)
          continue;

        const auto it = sessions_.find(cid);
        if (it == sessions_.end() || !it->second)
          continue;

        it->second->send_text(text);
      }
    }

    /**
     * @brief Send an event to a topic (string payload mode).
     */
    void broadcast_event_string_payload(
        const RealtimeEvent &e,
        std::string_view exclude_connection_id = {}) const
    {
      broadcast_text(e.topic, encode_event_json_string_payload(e), exclude_connection_id);
    }

    /**
     * @brief Send an event to a topic (raw payload mode).
     */
    void broadcast_event_raw_payload(
        const RealtimeEvent &e,
        std::string_view exclude_connection_id = {}) const
    {
      broadcast_text(e.topic, encode_event_json_raw_payload(e), exclude_connection_id);
    }

    /**
     * @brief Handle an incoming text frame from a session.
     *
     * Runtime calls this when a message arrives.
     * - Applies message auth hook (optional)
     * - Calls user on_message hook (optional)
     */
    void on_text_message(RealtimeSession &s, std::string text) const
    {
      if (message_auth_)
      {
        PolicyDecision d = message_auth_(s, text);
        if (!d.allowed)
          return;
      }

      if (on_message_)
        on_message_(s, std::move(text));
    }

    const HeartbeatPolicy &heartbeat_policy() const noexcept { return hb_; }

    /**
     * @brief SSE helper response for initiating a stream.
     *
     * This returns headers appropriate for SSE. Body is empty.
     * Your runtime should keep the connection open and stream chunks.
     */
    static vix::web_app::Response sse_open()
    {
      vix::web_app::Response r;
      r.status = 200;
      r.headers["content-type"] = "text/event-stream";
      r.headers["cache-control"] = "no-cache";
      r.headers["connection"] = "keep-alive";
      return r;
    }

  private:
    HeartbeatPolicy hb_{};

    HandshakeAuthHook handshake_auth_{};
    MessageAuthHook message_auth_{};

    OnConnectHook on_connect_{};
    OnDisconnectHook on_disconnect_{};
    OnMessageHook on_message_{};

    // Runtime-managed pointer table (single-process).
    // NOTE: If your runtime is multi-threaded, you must synchronize calls.
    std::unordered_map<std::string, RealtimeSession *> sessions_{};
    TopicIndex topic_index_{};
  };

} // namespace vix::realtime_app

#endif // VIX_REALTIME_APP_REALTIME_APP_HPP
