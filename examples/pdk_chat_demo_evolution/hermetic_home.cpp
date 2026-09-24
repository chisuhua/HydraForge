// examples/pdk_chat_demo_evolution/hermetic_home.cpp
// L2 hermetic HOME fixture impl (per P0'-4)
//
// Thread-local idempotency: per-thread static std::string holds the
// first-setup path, so subsequent calls in the same thread return the
// same guard pointer (no new mkdtemp).
//
// Atomic cross-thread guard: std::mutex protects mkdtemp itself + the
// static path string from concurrent first-call races across threads.
//
// Note: mkdtemp is used (not mktemp) for race-free atomic dir creation,
// per AGENTS.md mode #10 hygiene pattern (no umask windows / no TOCTOU).

#include "hermetic_home.h"

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace pdk_chat_demo_evolution::detail {

namespace {

// per-thread last setup (idempotent within same thread)
thread_local std::string tls_last_home;
thread_local std::string tls_last_genome_dir;
thread_local std::string tls_prev_home;

// cross-thread first-call race guard
std::mutex g_setup_mutex;

}  // namespace

HermeticHomeGuard* setup_hermetic_home() {
    // Idempotent on same thread: return new guard pointing to existing path
    if (!tls_last_home.empty()) {
        auto* guard = new HermeticHomeGuard{
            fs::path(tls_last_home),
            fs::path(tls_last_genome_dir),
            tls_prev_home
        };
        return guard;
    }

    std::lock_guard<std::mutex> lock(g_setup_mutex);

    // Double-check after acquiring lock (TOCTOU avoidance)
    if (!tls_last_home.empty()) {
        auto* guard = new HermeticHomeGuard{
            fs::path(tls_last_home),
            fs::path(tls_last_genome_dir),
            tls_prev_home
        };
        return guard;
    }

    char tmpl[] = "/tmp/l2-test-XXXXXX";
    char* dir = ::mkdtemp(tmpl);
    if (!dir) {
        throw std::runtime_error("mkdtemp failed for hermetic HOME");
    }

    // Capture original HOME for restore
    const char* prev = std::getenv("HOME");
    tls_prev_home = prev ? prev : "";

    // Set new HOME + HYDRAFORGE_GENOME_DIR
    std::string home_str = dir;
    std::string genome_str = home_str + "/.hydraforge/genomes";
    ::setenv("HOME", home_str.c_str(), 1);
    ::setenv("HYDRAFORGE_GENOME_DIR", genome_str.c_str(), 1);

    // Create genome subdir
    std::error_code ec;
    fs::create_directories(genome_str, ec);
    if (ec) {
        throw std::runtime_error("create_directories failed: " + ec.message());
    }

    tls_last_home = home_str;
    tls_last_genome_dir = genome_str;

    return new HermeticHomeGuard{
        fs::path(home_str),
        fs::path(genome_str),
        tls_prev_home
    };
}

void cleanup_hermetic_home(HermeticHomeGuard* guard) {
    if (!guard) return;

    // Remove the hermetic HOME dir (best-effort; ignore errors)
    std::error_code ec;
    fs::remove_all(guard->home, ec);
    // ec intentionally ignored — test fixtures may have already cleaned

    // Restore original HOME if we recorded one
    if (!guard->prev_home.empty()) {
        ::setenv("HOME", guard->prev_home.c_str(), 1);
    } else {
        ::unsetenv("HOME");
    }
    ::unsetenv("HYDRAFORGE_GENOME_DIR");

    // Clear thread-local state (idempotency re-armed for next setup call)
    tls_last_home.clear();
    tls_last_genome_dir.clear();
    tls_prev_home.clear();

    delete guard;
}

}  // namespace pdk_chat_demo_evolution::detail