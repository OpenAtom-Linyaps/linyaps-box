# linyaps-box

[![CodeQL](https://github.com/OpenAtom-Linyaps/linyaps-box/actions/workflows/codeql.yml/badge.svg?branch=master)](https://github.com/OpenAtom-Linyaps/linyaps-box/actions/workflows/codeql.yml)
[![CI](https://github.com/OpenAtom-Linyaps/linyaps-box/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/OpenAtom-Linyaps/linyaps-box/actions/workflows/ci.yml)
[![License](https://img.shields.io/github/license/OpenAtom-Linyaps/linyaps-box)](LICENSE)

[ **en** | [zh_CN](./README.zh_CN.md) ]

[![Packaging status](https://repology.org/badge/vertical-allrepos/linyaps-box.svg)](https://repology.org/project/linyaps-box/versions)

This project is a simple [OCI runtime] mainly used by [linyaps],
which is a toolkit for Linux desktop application distributing.

[OCI runtime]: https://github.com/opencontainers/runtime-spec
[linyaps]: https://github.com/OpenAtom-Linyaps/linyaps

## Build

It is recommend to use [cmake-presets]:

```bash
cmake --workflow --preset=dev
```

[cmake-presets]: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html

## License

This project is licensed under [LGPL-3.0-or-later](LICENSE).
