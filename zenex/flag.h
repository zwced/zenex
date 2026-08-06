#pragma once

#define ZENEX_FLAG(name) \
    struct name##_t {}; \
    inline constexpr name##_t name{};
