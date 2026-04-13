# PyroNet
Early Fire Detection: Purdue Senior Design Project

Team Members:
Sebastian Carrizosa
Kameron Jackson
Diego Ramirez
Garrison Thompson

## Download Specific Folders

To download **only** the a specific folder (e.g. hardware/, csp/) that contains the relevant files, run the following commands:

```bash
git clone --filter=blob:none --no-checkout https://github.com/thomp901/PyroNet.git
cd PyroNet
git sparse-checkout init --cone
git sparse-checkout set FOLDER/
git checkout main
```

Replace `FOLDER` with the specfic folder.

Then, you can `pull`, `add`, `commit`, and `push` like normal.

## Pushing

To push, go to the project directory in your terminal and enter the following commands:

```bash
git add .
git commit -m 'YOUR MESSAGE'
git push
```

Replace `YOUR MESSAGE` with a brief description of your changes.

## Discarding your changes

To **discard your changes** and pull the latest version, enter: 

```bash
git fetch --all
git reset --hard origin/main
```

> [!WARNING]
> Warning: This will permanently delete any local work.

