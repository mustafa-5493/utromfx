import torch
import torch.nn as nn

# A simple locomotion-style MLP
model = nn.Sequential(
    nn.Linear(45, 128),
    nn.ReLU(),
    nn.Linear(128, 64),
    nn.Tanh(),
    nn.Linear(64, 12)
)

# Save the entire model object
torch.save(model, "mock_policy.pt")
print("Created mock_policy.pt")