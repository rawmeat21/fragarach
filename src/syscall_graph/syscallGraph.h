#pragma once
#include "../ebpf/event.h"
#include<map>
#include<utility>
#include<vector>
#include "../json.hpp"

using json=nlohmann::json;

struct COOGraph
{
    std::vector<uint32_t> from;
    std::vector<uint32_t> to;
    std::vector<uint32_t> weights;

    json tojson()
    {
        json j;
        j["from"] = from;
        j["to"] = to;
        j["weights"] = weights;
        return j;
    }
};

class SyscallGraph
{
    std::map<std::pair<uint32_t,uint32_t>,uint32_t> edges;

public:
    void build(const std::vector<event>& events);
    COOGraph cooExport();
};


