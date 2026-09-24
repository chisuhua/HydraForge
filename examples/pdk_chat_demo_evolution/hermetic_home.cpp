// examples/pdk_chat_demo_evolution/hermetic_home.cpp
// L2 hermetic HOME fixture impl (per P0'-4)
//
// Thread-local idempotency: thread_local strings cache the first-setup path,
// so subsequent calls in the same thread return a new guard pointing to the
// same path (no new mkdtemp).
//
// Cross-thread hazard: setenv("HOME") is process-global, so two threads
// each calling setup_hermetic_home() will clobber each other's HOME (last
// writer wins) and cleanup races. The std::mutex below only protects
// mkdtemp from concurrent first-call races; it does NOT serialize HOME
// mutations. Tests run single-threaded (Catch2 sequential per thread);
// cross-thread callers must add their own synchronization.

#include "hermetic_home.h"

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace pdk_chat_demo_evolution::detail {

namespace {

thread_local std::string tls_last_home;
thread_local std::string tls_last_genome_dir;
thread_local std::string tls_prev_home;

std::mutex g_mkdtemp_mutex;

}  // namespace

HermeticHomeGuard* setup_hermetic_home() {
    if (!tls_last_home.empty()) {
        return new HermeticHomeGuard{
            fs::path(tls_last_home),
            fs::path(tls_last_genome_dir),
            tls_prev_home
        };
    }

    std::lock_guard<std::mutex> lock(g_mkdtemp_mutex);

    char tmpl[] = "/tmp/l2-test-XXXXXX";
    char* dir = ::mkdtemp(tmpl);
    if (!dir) {
        throw std::runtime_error("mkdtemp failed for hermetic HOME");
    }

    const char* prev = std::getenv("HOME");
    tls_prev_home = prev ? prev : "";

    std::string home_str = dir;
    std::string genome_str = home_str + "/.hydraforge/genomes";
    ::setenv("HOME", home_str.c_str(), 1);
    ::setenv("HYDRAFORGE_GENOME_DIR", genome_str.c_str(), 1);

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

    std::error_code ec;
    fs::remove_all(guard->home, ec);

    if (!guard->prev_home.empty()) {
        ::setenv("HOME", guard->prev_home.c_str(), 1);
    } else {
        ::unsetenv("HOME");
    }
    ::unsetenv("HYDRAFORGE_GENOME_DIR");

    tls_last_home.clear();
    tls_last_genome_dir.clear();
    tls_prev_home.clear();

    delete guard;
}

}  // namespace pdk_chat_demo_evolution::detail