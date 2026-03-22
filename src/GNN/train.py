import json
import torch
import os
from torch_geometric.data import Data, InMemoryDataset
from torch_geometric.nn import GCNConv, global_mean_pool
import torch.nn as nn
import torch.nn.functional as F
from torch_geometric.loader import DataLoader

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
        self.conv1 = GCNConv(17, 64) 
        self.conv2 = GCNConv(64, 64)
        self.classifier = nn.Linear(64, 2)
        self.dropout = nn.Dropout(p=0.5)

    def forward(self, edge_index, edge_attr, sysc, blocked, batch):
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
scripted = torch.jit.script(model)
scripted.save("/opt/fragarach/model_scripted.pt")