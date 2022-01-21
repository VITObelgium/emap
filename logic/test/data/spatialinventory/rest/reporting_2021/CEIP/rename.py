import os
from pathlib import Path

for p in Path(".").rglob("*"):
     if p.suffix == ".txt":
         os.remove(p)
         p.touch()
