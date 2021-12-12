# Filesystem Drivers

For a container, there are always need to create some file not exist. So we need use a virtual filesystem to mount file not exist or override lower file.

In flatpak, it not allow to override file, that is a good imagine, but how to bind a file not exist in the base, the flatpak can not deal with that now.

For docker or other container tools, an overlayfs (or other filesystem with same function) is need.

For linglong, we do not need a powerful overlayfs to action complex function, but just fix some issue that case by some not standard apps. so we use an mini fuse proxy to fake mount point or path not exist.

So apps should keep the rules first. And just keep compatibility with some not open source apps by fuse proxy filesystem.

## The access control

The access control is not an mandatory standard. OCI runtime spec do not care it, especially an dynamic access control. We use fuse proxy filesystem to finish this.

When app in container access an path not exist, the fuse proxy can get it. It the path is define as an permission control path, it will as user to confirm. and notify ll-box freeze all process in sandbox.

## How to use fuse-proxy?

fuse-proxy is a mount backend for ll-box. the ll-box call it before chroot in the new mount namespace.