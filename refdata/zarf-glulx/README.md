# Zarf Glulx Reference Data

Source: <https://eblong.com/zarf/glulx/>

Downloaded on 2026-06-21 with:

```sh
wget --recursive --level=1 --no-parent --no-host-directories --cut-dirs=2 \
  --domains=eblong.com --directory-prefix=refdata/zarf-glulx \
  https://eblong.com/zarf/glulx/
```

The raw `wget` capture was trimmed to remove `robots.txt` and Quixe browser-launch HTML files with query-string names. The retained files are the locally hosted Glulx specs, docs, sample games, unit-test games, Inform source files, save sample, and companion archives linked from the page.

Current contents:

- 32 `.inf` Inform source files
- 27 `.ulx` Glulx game/test files
- 6 `.gblorb` Blorb test files
- 2 `.z5` comparison Z-code files
- 1 `.save` sample save file
- 4 `.html` documentation/index files
- 10 other companion files, including spec PDFs, tar/zip archives, and `sensory.blb`

`SHA256SUMS` records checksums for the archived files.
