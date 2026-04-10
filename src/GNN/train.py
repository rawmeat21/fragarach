import json
import torch
import os
from torch_geometric.data import Data, InMemoryDataset
from torch_geometric.nn import GCNConv, global_mean_pool
import torch.nn as nn
import torch.nn.functional as F
from torch_geometric.loader import DataLoader
from torch import Tensor
from typing import Optional
from torch_geometric.typing import OptTensor


class SyscallDataset(InMemoryDataset):
    def __init__(self, root):
        super().__init__(root)
        self.load(self.processed_paths[0])
    
    @property
    def raw_file_names(self):
        return os.listdir("/opt/fragarach/raw")
    
    @property
    def processed_file_names(self):
        return ["data.pt"]
    
    def process(self):
        graphs = []
        for filename in self.raw_file_names:
            if not filename.endswith(".json"): continue
            with open(f"/opt/fragarach/raw/{filename}") as f:
                raw = json.load(f)
                edge_index = torch.tensor([raw["from"], raw["to"]], dtype=torch.long)
                edge_attr  = torch.tensor(raw["weights"], dtype=torch.float).unsqueeze(1)
                sysc       = torch.tensor(raw["sysc"], dtype=torch.long)    
                blocked    = torch.tensor(raw["blocked"], dtype=torch.float).unsqueeze(1)
                label      = torch.tensor([raw["label"]], dtype=torch.long)

                data = Data(edge_index=edge_index,edge_attr=edge_attr,sysc=sysc,blocked=blocked,y=label)

            graphs.append(data)
        self.save(graphs, self.processed_paths[0])


class MalwareGNN(nn.Module):
    def __init__(self):
        super().__init__()
        self.embedding = nn.Embedding(500, 16)
        self.conv1 = GCNConv(17, 64,normalize=False) 
        self.conv2 = GCNConv(64, 64,normalize=False)
        self.classifier = nn.Linear(64, 2)
        self.dropout = nn.Dropout(p=0.5)

    def forward(self, edge_index: Tensor, edge_attr: Tensor, sysc: Tensor, blocked: Tensor, batch: Tensor):
        sysc_embedded = self.embedding(sysc)
        x = torch.cat([sysc_embedded, blocked], dim=1)
        
        x = self.conv1(x, edge_index, edge_attr.squeeze(1))
        x = F.relu(x)
        x = self.dropout(x)

        x = self.conv2(x, edge_index, edge_attr.squeeze(1))
        x = F.relu(x)
        x = self.dropout(x)
        
        x = global_mean_pool(x, batch)
        
        x = self.classifier(x)
        return x



dataset = SyscallDataset(root="/opt/fragarach")
dataset = dataset.shuffle()

train_size = int(0.8 * len(dataset))

train_dataset = dataset[:train_size]
test_dataset = dataset[train_size:]

train_loader = DataLoader(train_dataset, batch_size=32, shuffle=True)

test_loader = DataLoader(test_dataset, batch_size=32, shuffle=False)


criterion = nn.CrossEntropyLoss()

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
model = MalwareGNN().to(device)

optimizer = torch.optim.Adam(model.parameters(), lr=0.001)


def evaluate(model, loader):
    model.eval()
    correct = 0
    total = 0
    with torch.no_grad():
        for batch in loader:
            batch = batch.to(device)
            out = model(batch.edge_index, batch.edge_attr, batch.sysc, batch.blocked, batch.batch)
            pred = out.argmax(dim=1) 
            correct += (pred == batch.y.squeeze()).sum().item()
            total += len(batch.y)
    return correct / total


for epoch in range(100):
    model.train()
    # each batch has 32 graphs
    total_loss = 0
    for batch in train_loader:
        batch=batch.to(device)
        # clear previous gradients
        optimizer.zero_grad()   
        # forward pass
        out = model(batch.edge_index, batch.edge_attr, batch.sysc, batch.blocked, batch.batch)      

        loss = criterion(out, batch.y.squeeze())
        loss.backward()
        optimizer.step()
        total_loss += loss.item()

    acc = evaluate(model, test_loader)
    print(f"Epoch {epoch}, Loss: {total_loss:.4f}, Test Accuracy: {acc:.4f}")


torch.save(model.state_dict(), "/opt/fragarach/model.pt")


model.eval()

# 1. Prepare dummy data that matches your forward method EXACTLY
# edge_index: [2, 10], edge_attr: [10, 1], sysc: [20], blocked: [20, 1], batch: [20]
d_edge_index = torch.zeros((2, 10), dtype=torch.long).to(device)
d_edge_attr  = torch.zeros((10, 1), dtype=torch.float).to(device)
d_sysc       = torch.zeros((20,), dtype=torch.long).to(device)
d_blocked    = torch.zeros((20, 1), dtype=torch.float).to(device)
d_batch      = torch.zeros((20,), dtype=torch.long).to(device)

# 2. Trace the model instead of scripting it
try:
    # We pass the inputs as a tuple
    traced_model = torch.jit.trace(model, (d_edge_index, d_edge_attr, d_sysc, d_blocked, d_batch))
    
    # Save for C++
    traced_model.save("/opt/fragarach/model_scripted.pt")
    print("SUCCESS: Model traced and saved for C++.")
except Exception as e:
    print(f"Tracing failed: {e}")
