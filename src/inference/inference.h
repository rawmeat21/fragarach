#pragma once
#include<utility>
#include<torch/script.h>
#include<string>
#include "../syscall_graph/syscallGraph.h"

class Inference
{
    torch::jit::script::Module model;// the trained model
public:
    Inference(std::string_view modelPath);

    std::pair<int,double> predict(COOGraph& cg);// (benign/malicious,confidence score)
};


