#pragma once

#include "framework.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <boost/asio.hpp>
#include <mutex>
#include <queue>
#include <atomic>
#include <memory>
#include <string>
#include <thread>