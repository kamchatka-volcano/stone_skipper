#pragma once
#include "task.h"
#include <asyncgi/asyncgi.h>
#include <sfun/member.h>
#include <utility>

namespace stone_skipper {
struct Task;

struct TaskProcessor {
    explicit TaskProcessor(Task);
    void operator()(const asyncgi::RouteParameters<>&, const asyncgi::Request&, asyncgi::Response&) const;

private:
    sfun::member<const Task> task_;
};

} //namespace stone_skipper