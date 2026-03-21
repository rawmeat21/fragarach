#include "syscallGraph.h"

void SyscallGraph::build(const std::vector<event>& events)
{
    if(!events.size()) return;

    for(int i=0;i<(int)events.size()-1;++i)
    {
        nodes[events[i].syscall_nr]=events[i].blocked | nodes[events[i].syscall_nr];
        edges[{events[i].syscall_nr,events[i+1].syscall_nr}]++;
    }

    nodes[events[events.size()-1].syscall_nr]=events[events.size()-1].blocked | nodes[events[events.size()-1].syscall_nr];

    int i=0;
    for(auto& [sysc,stat]:nodes) syscVertex[sysc]=(i++);
}

COOGraph SyscallGraph::cooExport()
{
    COOGraph cg{};

    for(auto& edge:edges)
    {
        cg.from.push_back(syscVertex[edge.first.first]);
        cg.to.push_back(syscVertex[edge.first.second]);
        cg.weights.push_back(edge.second);
    }

    for(auto& [sysc,stat]:nodes)
    {
        cg.nodes.push_back(sysc);
        cg.blocked.push_back(stat);
    }

    return cg;
}