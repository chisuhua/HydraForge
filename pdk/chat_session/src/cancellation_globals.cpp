#include <agenticdsl/pdk/cancellation_globals.h>

namespace hydraforge::pdk {

// §4.0.1: defaults to nullptr; main.cpp sets it before constructing ChatSession
std::shared_ptr<CancellationRegistry> g_cancellation_registry = nullptr;

}  // namespace hydraforge::pdk
