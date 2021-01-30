XZ (LZMA)
=========

I've only modified the Makefile a bit to create a static library, and added an empty "fallthrough" define to xz_config.h.
I also had to add this little patch to make it work with concatenated streams:
```diff
--- a/src/xz/xz_dec_stream.c
+++ b/src/xz/xz_dec_stream.c
@@ -715,6 +715,10 @@ static enum xz_ret dec_main(struct xz_dec *s, struct xz_buf *b)
                        if (!fill_temp(s, b))
                                return XZ_OK;

+                       if (!memeq(s->temp.buf, HEADER_MAGIC, HEADER_MAGIC_SIZE)) {
+                               xz_dec_reset(s);
+                               continue;
+                       }
                        return dec_stream_footer(s);
                }
        }
```
Otherwise this the verbatim code from the Linux kernel.

Compilation
-----------

```
make libxz.a
```
