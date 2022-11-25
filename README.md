# 玲珑：沙箱

玲珑（Linglong） is the container application toolkit of deepin.

玲珑是玲珑塔的简称，既表示容器能对应用有管控作用，也表明了应用/运行时/系统向塔一样分层的思想。

## Feature

- [x] Standard oci runtime

## Roadmap

### Current

- [ ] OCI Standard Box

## Dependencies

```bash
#For release based on debian
sudo apt-get install cmake build-essential libyaml-cpp-dev nlohmann-json3-dev libgtest-dev
```

## Installation

## Roadmap

### Current

- [ ] Configuration
    - [x] Root
    - [ ] Mount
        - [ ] Cgroup
    - [X] Hostname
- [ ] Linux Container
    - [x] Default Filesystems
    - [X] Namespaces
        - [x] Network
    - [X] User namespace mappings
    - [ ] Devices
    - [ ] Default Devices
    - [ ] Control groups
    - [ ] IntelRdt
    - [ ] Sysctl
    - [ ] Seccomp
    - [ ] Rootfs Mount Propagation
    - [ ] Masked Paths
    - [ ] Readonly Paths
    - [ ] Mount Label
- [ ] Extend
    - [x] Container manager
    - [x] Debug
    - [ ] DBus proxy permission control
    - [ ] Filesystem permission control base fuse
    - [ ] X11&&Wayland security

### TODO

- [ ] full support of parse all seccomp arch and syscall

## Getting help

Any usage issues can ask for help via

- [Gitter](https://gitter.im/orgs/linuxdeepin/rooms)
- [IRC channel](https://webchat.freenode.net/?channels=deepin)
- [Forum](https://bbs.deepin.org)
- [WiKi](https://wiki.deepin.org/)

## Getting involved

We encourage you to report issues and contribute changes

- [Contribution guide for developers](https://github.com/linuxdeepin/developer-center/wiki/Contribution-Guidelines-for-Developers-en)
  . (English)
- [开发者代码贡献指南](https://github.com/linuxdeepin/developer-center/wiki/Contribution-Guidelines-for-Developers) (中文)

## License

This project is licensed under [LGPL-3.0-or-later](LICENSE).
