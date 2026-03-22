#pragma once
#include "../ebpf/event.h"
#include<map>
#include<utility>
#include<vector>
#include "../json.hpp"

using json=nlohmann::json;

struct COOGraph
{
    std::vector<int> from;
    std::vector<int> to;
    std::vector<int> weights;

    std::vector<int> nodes;// nodes[i]= the syscall which vertex i maps to
    std::vector<int> blocked;// blocked[i]= whether nodes[i] was blocked
    
    int label=-1;
    json tojson()
    {
        json j;
        j["from"] = from;
        j["to"] = to;
        j["weights"] = weights;
        j["sysc"] = nodes;
        j["blocked"] = blocked;
        j["label"] = label;
        return j;
    }
};

class SyscallGraph
{
    std::map<std::pair<uint32_t,uint32_t>,uint32_t> edges;
    std::map<uint32_t,uint8_t> nodes;
    std::map<uint32_t,uint32_t> syscVertex;

public:
    void build(const std::vector<event>& events);
    COOGraph cooExport();
};


