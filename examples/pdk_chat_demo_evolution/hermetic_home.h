// examples/pdk_chat_demo_evolution/hermetic_home.h
// L2 hermetic HOME fixture (per P0'-4)
//
// Per AGENTS.md mode #10 hygiene: NEVER pollute host filesystem.
// L2 mock + real-LLM tests use FilesystemGenomeRegistry with hermetic HOME
// fixture to avoid fresh-deploy hygiene violation (per AGENTS.md pattern #10
// + 2026-09-21 genome-registry HMAC fix case study).
//
// Thread-local idempotency: `setup_hermetic_home()` returns the same
// HermeticHomeGuard* on subsequent calls within the same thread (Task 2 case 2).

#pragma once

#include <filesystem>
#include <string>

namespace pdk_chat_demo_evolution::detail {

struct HermeticHomeGuard {
    std::filesystem::path home;        // /tmp/l2-test-XXXXXX
    std::filesystem::path genome_dir;  // {home}/.hydraforge/genomes

    // 原始 HOME, 用于 restore (避免测试间污染)
    std::string prev_home;
};

// Create a fresh hermetic HOME at /tmp/l2-test-XXXXXX with mkdtemp.
// Sets HOME + HYDRAFORGE_GENOME_DIR env vars and creates .hydraforge/genomes/.
// Returns ownership of HermeticHomeGuard* (caller must cleanup_hermetic_home).
//
// Per-thread idempotent: returns a NEW guard pointing to the same path on
// repeated calls within the same thread (per test_hermetic_home case 2).
// Cross-thread usage is UNSAFE — setenv("HOME") is process-global, so two
// threads each calling setup_hermetic_home() will clobber each other's
// HOME (last writer wins) and cleanup races. Use single-threaded fixtures
// only, or wrap in a process-wide lock.
HermeticHomeGuard* setup_hermetic_home();

// Remove the hermetic HOME directory and free the guard. Safe to call with
// nullptr (no-op). Calling cleanup() TWICE on the same non-null pointer is
// double-free UB. It IS safe to call cleanup() on two distinct guards whose
// home dirs point to the same path (second remove_all fails silently).
void cleanup_hermetic_home(HermeticHomeGuard* guard);

}  // namespace pdk_chat_demo_evolution::detail