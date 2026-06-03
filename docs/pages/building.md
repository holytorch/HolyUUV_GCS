---
title: Building
layout: default
nav_order: 3
---

# Building from source
{: .no_toc }

Build the project and, optionally, package a portable AppImage.
{: .fs-6 .fw-300 }

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

## Build

Run the scripts **in order** from the project root:

```bash
# 1. Install dependencies (see Installation).
./scripts/install_deps.sh

# 2. Clean-build the project (CMake + make).
./scripts/build_project_linux.sh

# 3. Run it.
./build/HolyUUV_GCS
```

| Script                             | What it does                                              |
|------------------------------------|----------------------------------------------------------|
| `scripts/build_project_linux.sh`   | Clean CMake build → produces `build/HolyUUV_GCS`         |
| `scripts/package_appimage.sh`      | *(optional)* Bundles a portable AppImage for distribution |

{: .note }
> The scripts can be run from anywhere — each one `cd`s to the project root automatically.

## Portable AppImage

Want a single-file binary you can hand to anyone?

```bash
./scripts/package_appimage.sh   # → HolyUUV_GCS.AppImage
```

The resulting AppImage runs on **Ubuntu 24.04+** with no installation required:

```bash
chmod +x HolyUUV_GCS.AppImage
./HolyUUV_GCS.AppImage
```

It uses your system GPU when available and falls back to software rendering automatically.

---

Next: [Connecting]({% link pages/connecting.md %}).
