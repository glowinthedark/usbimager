ZStandard
=========

This is a heavily stripped down version of [zstd](https://github.com/facebook/zstd). The compressor only supports the fast method.

Compilation
-----------

```
make libzstd.a ZSTD_LIB_DICTBUILDER=0 ZSTD_LIB_DEPRECATED=0 ZSTD_LEGACY_SUPPORT=0
```
