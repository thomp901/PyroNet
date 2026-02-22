# PyroNet
Early Fire Detection: Purdue Senior Design Project

Team Members:
Sebastian Carrizosa
Kameron Jackson
Diego Ramirez
Garrison Thompson

# Hardware

To download **only** the hardware folder that contains the KiCad files, use the following commands:

```bash
git clone --filter=blob:none --no-checkout https://github.com/thomp901/PyroNet.git
cd PyroNet
git sparse-checkout init --cone
git sparse-checkout set hardware/
git checkout main
```

Then, you can `pull`, `add`, `commit`, and `push` like normal.
