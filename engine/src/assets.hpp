#pragma once

#include <engine-generated/engine_assets.hpp>

template <hat::fixed_string Path>
constexpr std::span<const u8> get_asset() {
    return AssetResolver<Path>::get();
}
