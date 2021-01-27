XZ (LZMA)
=========

I've only modified the Makefile a bit to create a static library, and added an empty "fallthrough" define to xz_config.h.
Otherwise this the verbatim code from the Linux kernel.

Compilation
-----------

```
make libxz.a
```
