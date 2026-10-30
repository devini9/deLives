# Build do deLives (Flatpak)

## Build e instalar localmente (usuário)
```bash
flatpak-builder --user --install --force-clean build-dir packaging/io.github.devini9.delives.json
```

## Gerar bundle (.flatpak) para instalar no Bazzite/outros
```bash
flatpak-builder --repo=repo --force-clean build-dir packaging/io.github.devini9.delives.json
flatpak build-bundle repo io.github.devini9.delives.flatpak io.github.devini9.delives
```
