// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include <backward.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>

namespace backward { SignalHandling sh; }

namespace {

[[maybe_unused]] const bool terminate_handler_installed = []() {
    std::set_terminate([]() {
        fprintf(stderr, "[terminate] unhandled exception — backtrace:\n");
        backward::StackTrace st;
        st.load_here(32);
        backward::Printer p;
        p.print(st, stderr);
        std::abort();
    });
    return true;
}();

} // namespace
