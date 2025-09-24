import torch
import torch.nn as nn

class ml_model_in(nn.Module):
    def __init__(self):
        super(ml_model_in, self).__init__()
        self.features = nn.Sequential(
            nn.Conv2d(1, 64, kernel_size=3, padding=1),  # Canal de entrada ajustado para 1
            nn.ReLU(),
            nn.MaxPool2d(kernel_size=2, stride=2),
            nn.Conv2d(64, 192, kernel_size=5, padding=2)
        )

    def forward(self, x):
        x = self.features(x)
        return x


class ml_model_hidden(nn.Module):
    def __init__(self):
        super(ml_model_hidden, self).__init__()
        self.features = nn.Sequential(
            nn.ReLU(),
            nn.MaxPool2d(kernel_size=2, stride=2),
            nn.Conv2d(192, 384, kernel_size=3, padding=1), # Atualizado para entrada 256 e saída 384
            nn.ReLU(),
            nn.Conv2d(384, 256, kernel_size=3, padding=1)
        )

    def forward(self, x):
        x = self.features(x)
        return x

class ml_model_out(nn.Module):
    def __init__(self, NUM_CLASSES):
        super(ml_model_out, self).__init__()
        self.features = nn.Sequential(
            nn.ReLU(),
            nn.Conv2d(256, 256, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(kernel_size=2, stride=2)
        )
        self.classifier = nn.Sequential(
            nn.Dropout(),
            nn.Linear(256 * 3 * 3, 4096),  # Ajustado para corresponder à saída da camada anterior
            nn.ReLU(),
            nn.Dropout(),
            nn.Linear(4096, 4096),
            nn.ReLU(),
            nn.Linear(4096, NUM_CLASSES)
        )

    def forward(self, x):
        x = self.features(x)
        x = x.view(x.size(0), 256 * 3 * 3)  # Ajustado para corresponder à saída da camada anterior
        x = self.classifier(x)
        return x
