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

/*std::pair<int,double> Inference::predict(COOGraph& cg)
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
}*/


/*std::pair<int,double> Inference::predict(COOGraph& cg)
{
    try
    {   
        // Ensure indices are kLong (int64)
        auto from_tensor = torch::tensor(cg.from, torch::kLong);
        auto to_tensor = torch::tensor(cg.to, torch::kLong);
        auto edge_index = torch::stack({from_tensor, to_tensor});

        // Weights and features are usually kFloat
        auto edge_attr = torch::tensor(cg.weights, torch::kFloat);
        auto sysc = torch::tensor(cg.nodes, torch::kLong);
        auto blocked = torch::tensor(cg.blocked, torch::kFloat);

        // Reshape to match Python expectations (edge_attr as 1D, blocked as 2D)
        edge_attr = edge_attr.view({-1}); 
        blocked = blocked.unsqueeze(1);

        auto batch = torch::zeros({(long)cg.nodes.size()}, torch::kLong);

        // Prepare the IValue vector
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(edge_index);
        inputs.push_back(edge_attr);
        inputs.push_back(sysc);
        inputs.push_back(blocked);
        inputs.push_back(batch);

        // Execute inference
        auto output = model.forward(inputs).toTensor();
        auto probs = torch::softmax(output, 1);

        return {probs.argmax(1).item<int>(), probs.max().item<double>()};
    }
    catch(const std::exception& e)
    {
        std::cerr << "Inference Error: " << e.what() << '\n';
        exit(1);
    }
}*/

std::pair<int,double> Inference::predict(COOGraph& cg)
{
    try
    {   
        // 1. Ensure all index-related tensors are kLong (int64)
        auto from_tensor = torch::tensor(cg.from, torch::kLong);
        auto to_tensor = torch::tensor(cg.to, torch::kLong);
        auto edge_index = torch::stack({from_tensor, to_tensor});
        auto sysc = torch::tensor(cg.nodes, torch::kLong);
        auto batch = torch::zeros({(long)cg.nodes.size()}, torch::kLong);

        // 2. Ensure feature-related tensors are kFloat (float32)
        auto edge_attr = torch::tensor(cg.weights, torch::kFloat);
        auto blocked = torch::tensor(cg.blocked, torch::kFloat);

        // 3. Match the EXACT shapes used during the Python trace
        // edge_attr was (10, 1) and blocked was (20, 1) in train(2).py
        edge_attr = edge_attr.unsqueeze(1); 
        blocked = blocked.unsqueeze(1);

        // 4. Build the input vector in the exact order of forward()
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(edge_index);
        inputs.push_back(edge_attr);
        inputs.push_back(sysc);
        inputs.push_back(blocked);
        inputs.push_back(batch);

        // 5. Run inference
        auto output = model.forward(inputs).toTensor();
        auto probs = torch::softmax(output, 1);

        return {probs.argmax(1).item<int>(), probs.max().item<double>()};
    }
    catch(const std::exception& e)
    {
        std::cerr << "Inference Error: " << e.what() << '\n';
        exit(1);
    }
}
