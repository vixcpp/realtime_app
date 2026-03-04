#include <realtime_app/realtime_app.hpp>

#include <iostream>

using namespace vix::realtime_app;

int main()
{
  // Example: format an SSE event chunk.
  const std::string chunk = sse_format(
      "notify",
      "42",
      "{\"title\":\"Hello\",\"body\":\"SSE works\"}");

  std::cout << chunk;

  // Example: open SSE response headers (for an HTTP handler).
  auto r = RealtimeApp::sse_open();
  std::cout << "status=" << r.status << "\n";
  std::cout << "content-type=" << r.headers["content-type"] << "\n";
  return 0;
}
