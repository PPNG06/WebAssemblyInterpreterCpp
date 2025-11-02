#pragma once

#include <span>
#include <vector>

#include "wasm/module.hpp"

namespace wasm
{
Module parse_module(std::span<const uint8_t> bytes);
}
