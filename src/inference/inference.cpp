#include "inference.h"
#include<vector>

Inference::Inference(std::string_view modelPath)
{
    try
    {
        model = torch::jit::load(std::string(modelPath));        
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        exit(1);
    }
}

std::pair<int,double> Inference::predict(COOGraph& cg)
{
    try
    {   
        auto from_tensor=torch::tensor(cg.from);
        auto to_tensor=torch::tensor(cg.to);

        auto edge_index=torch::stack({from_tensor,to_tensor});

        auto edge_attr=torch::tensor(cg.weights);

        auto sysc=torch::tensor(cg.nodes);
        auto blocked=torch::tensor(cg.blocked);

        edge_attr=edge_attr.unsqueeze(1);
        blocked=blocked.unsqueeze(1);

        auto batch=torch::zeros(cg.nodes.size(),torch::kInt64);

        std::vector<torch::jit::IValue> inputs;
        inputs={edge_index,edge_attr,sysc,blocked,batch};

        auto output = model.forward(inputs).toTensor();

        auto probs = torch::softmax(output, 1);

        return {probs.argmax(1).item<int>(),probs.max().item<double>()};
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        exit(1);
    }
}