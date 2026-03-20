#include "syscallGraph.h"

void SyscallGraph::build(const std::vector<event>& events)
{
    for(int i=0;i<(int)events.size()-1;++i)
    {
        edges[{events[i].syscall_nr,events[i+1].syscall_nr}]++;
    }
}

COOGraph SyscallGraph::cooExport()
{
    COOGraph cg{};

    for(auto& edge:edges)
    {
        cg.from.push_back(edge.first.first);
        cg.to.push_back(edge.first.second);
        cg.weights.push_back(edge.second);
    }

    return cg;
}