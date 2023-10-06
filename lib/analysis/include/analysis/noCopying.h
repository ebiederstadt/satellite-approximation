#pragma once

#define MAKE_NONCOPYABLE(c) \
private:                    \
    c(c const&) = delete;   \
    c& operator=(c const&) = delete
