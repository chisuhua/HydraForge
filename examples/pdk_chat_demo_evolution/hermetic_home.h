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
// Idempotent on the same thread — returns the same guard on repeated calls
// within one thread (per test_hermetic_home case 2).
HermeticHomeGuard* setup_hermetic_home();

// Remove the hermetic HOME directory and free the guard. Safe to call with
// nullptr (no-op). Double-cleanup is safe (no-op on already-cleaned guard).
void cleanup_hermetic_home(HermeticHomeGuard* guard);

}  // namespace pdk_chat_demo_evolution::detail