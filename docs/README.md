# Documentation assets

Component and package documentation lives in the README files beside the source:

- [Components](../components)
- [Packages](../packages)
- [Tests](../tests/README.md)

Images used by those documents are stored here:

```text
images/
├── badges/                    # Static badges such as the MIT license
├── hardware/                  # Shared product photos and pin maps
├── logos/                     # Project and vendor logos
└── packages/                  # Package-specific photos and diagrams
    ├── button/neimz/
    ├── display/colorado/
    ├── hvac/samsung/
    ├── led/
    ├── radiation/
    ├── scale/
    └── wallpad/
```

Keep shared hardware images in `images/hardware/` and reuse them across README
files. Store package-specific images under `images/packages/`, matching the path
under the repository's `packages/` directory. Reference images with paths relative
to each README file.
