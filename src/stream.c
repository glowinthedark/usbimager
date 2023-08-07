/*
 * usbimager/stream.c
 *
 * Copyright (C) 2020 bzt (bztsrc@gitlab)
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @brief Stream Input/Output file functions
 *
 */

#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include "lang.h"
#include "stream.h"

#ifdef WINVER
#include <windows.h>
/*extern int _fileno(FILE *f);*/
FILE *stream_fopen(char *fn, char *mode)
{
    wchar_t *szFilePathName, wmode[8];
    int wlen;
    FILE *f = NULL;
    memset(wmode, 0, sizeof(wmode));
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 8);
    wlen = MultiByteToWideChar(CP_UTF8, 0, fn, -1, NULL, 0);
    if(wlen > 0) {
        szFilePathName = (wchar_t*)malloc((wlen+1) * sizeof(wchar_t));
        if(szFilePathName) {
            MultiByteToWideChar(CP_UTF8, 0, fn, -1, szFilePathName, wlen);
            szFilePathName[wlen] = 0;
            f = _wfopen(szFilePathName, wmode);
            free(szFilePathName);
        }
    }
    return f;
}
#else
#include <sys/statvfs.h>
extern int fileno(FILE *f);
#define stream_fopen fopen
#endif
#if !defined(WINVER) && !defined(MACOSX)
uint64_t mytell (FILE * stream)
{ fpos_t pos; return (fgetpos(stream, &pos)) ? 0L : (uint64_t)(pos.__pos); }
int myseek (FILE* stream, uint64_t offset)
{ fpos_t pos = {0}; pos.__pos = offset; return fsetpos(stream, &pos); }
#else
uint64_t mytell (FILE * stream)
{ fpos_t pos; return (fgetpos(stream, &pos)) ? 0LL : (uint64_t)(pos); }
int myseek (FILE* stream, uint64_t offset)
{ fpos_t pos = (fpos_t)offset; return fsetpos(stream, &pos); }
#endif

int verbose = 0;
int buffer_size = 1024*1024;
int baud = 115200;
int force = 0;
int dstfd = 0;

/**
 * helper for xz to dynamically get the largest dictionary size possible
 */
struct xz_dec *xzinit(void)
{
    struct xz_dec *ret = NULL;
    uint32_t siz = (1UL << 30); /* start at 1G */
    for(; !ret && siz > (1UL << 25); siz >>= 1) {
        if(verbose) printf("  xz dictionary size %u M... ", siz / 1024 / 1024);
        ret = xz_dec_init(XZ_DYNALLOC, siz);
        if(verbose) printf(ret ? "OK\r\n" : "failed\r\n");
    }
    if(!ret && verbose) printf("  xz unable to allocate dictionary\r\n");
    return ret;
}

/**
 * convert ascii octal number to binary number
 */
uint64_t oct2bin(char *str, int size)
{
    uint64_t s = 0;
    while(size-- > 0) { s <<= 3; s += *str++ - '0'; }
    return s;
}

/**
 * convert ascii hex number to binary number
 */
uint64_t hex2bin(char *str, int size)
{
    uint64_t s = 0;
    while(size-- > 0) {
        s <<= 4;
        if(*str >= '0' && *str <= '9')
            s += (uint64_t)((uint8_t)(*str)-'0');
        else if(*str >= 'A' && *str <= 'F')
            s += (uint64_t)((uint8_t)(*str)-'A'+10);
        else if(*str >= 'a' && *str <= 'f')
            s += (uint64_t)((uint8_t)(*str)-'a'+10);
        str++;
    }
    return s;
}

/**
 * Returns progress percentage and the status string in str
 */
int stream_status(stream_t *ctx, char *str, int done)
{
    time_t t = time(NULL);
    uint64_t d = 0;
    int h,m,s;
#ifdef WINVER
    wchar_t rem[64];
#else
    char rem[64];
#endif
    if(done) {
        memset(str, 0, 2);
        if(ctx->fileSize && ctx->readSize >= ctx->fileSize
#ifndef WINVER
            && !errno
#endif
        ) {
            d = t - ctx->start;
            h = d / 3600; d %= 3600; m = d / 60; s = d % 60;
            if(h<0 || h>23) h = 0;
            if(m<0) m = 0;
            if(s<0) s = 0;
#ifdef WINVER
            wsprintfW((wchar_t*)str, lang[L_DONE], h, m, s);
#else
            sprintf(str, lang[L_DONE], h, m, s);
#endif
        }
        return 0;
    }
    rem[0] = 0;
    if(ctx->start < t) {
        if(ctx->readSize) {
            if(ctx->fileSize)
                d = ctx->readSize / (t - ctx->start);
            else
                d = ctx->cmrdSize / (t - ctx->start);
            ctx->avgSpeedBytes += d;
            ctx->avgSpeedNum++;
            d = ctx->avgSpeedBytes / ctx->avgSpeedNum;
            if(verbose > 1) printf("  average speed %" PRIu64" bytes / sec\r\n", d);
        }
        if(ctx->avgSpeedNum > 2) {
            d = d ? (ctx->fileSize ? ctx->fileSize - ctx->readSize : ctx->compSize - ctx->cmrdSize) / d : 0;
            h = d / 3600; d %= 3600; m = d / 60; if(h<0 || h>23) h = 0; if(m<0) m = 0;
#ifdef WINVER
            if(h > 0) wsprintfW(rem, (wchar_t*)lang[h>1 && m>1 ? L_STATHSMS : (h>1 && m<2 ? L_STATHSM :
                    (h==1 && m>0 ? L_STATHMS : L_STATHM))], h, m);
            else if(m > 0) wsprintfW(rem, (wchar_t*)lang[m>1 ? L_STATMS : L_STATM], m);
            else wsprintfW(rem, (wchar_t*)lang[L_STATLM]);
#else
            if(h > 0) sprintf(rem, lang[h>1 && m>1 ? L_STATHSMS : (h>1 && m<2 ? L_STATHSM :
                    (h==1 && m>0 ? L_STATHMS : L_STATHM))], h, m);
            else if(m > 0) sprintf(rem, lang[m>1 ? L_STATMS : L_STATM], m);
            else strcpy(rem, lang[L_STATLM]);
#endif
        }
    }
#ifdef WINVER
    if(ctx->fileSize)
        wsprintfW((wchar_t*)str, L"%6u %s / %u %s%s%s",
            (unsigned int)(ctx->readSize >> 20), lang[L_MIB],
            (unsigned int)(ctx->fileSize >> 20), lang[L_MIB], rem[0] ? L", " : L"", rem);
    else
        wsprintfW((wchar_t*)str, L"%6u %s %s%s%s",
            (unsigned int)(ctx->readSize >> 20), lang[L_MIB], lang[L_SOFAR], rem[0] ? L", " : L"", rem);
#else
    if(ctx->fileSize)
        sprintf(str, "%6" PRIu64 " %s / %" PRIu64 " %s%s%s",
            (ctx->readSize >> 20), lang[L_MIB],
            (ctx->fileSize >> 20), lang[L_MIB], rem[0] ? ", " : "", rem);
    else
        sprintf(str, "%6" PRIu64 " %s %s%s%s",
            (ctx->readSize >> 20), lang[L_MIB], lang[L_SOFAR], rem[0] ? ", " : "", rem);
#endif
    d = ctx->fileSize ? (ctx->readSize * 1000) / (ctx->fileSize * 10) :
        (ctx->cmrdSize * 1000) / (ctx->compSize * 10 + 1);
    /* readSize can be greater than fileSize because it's rounded up to 512 bytes */
    return d > 100 ? 100 : d;
}

/**
 * Open file and determine the source's format
 */
#define HEADER_SIZE 65536
int stream_open(stream_t *ctx, char *fn, int uncompr)
{
    unsigned char *buff;
    char *url = NULL, *s, *d;
    uint64_t fs = 0, hs = 0, zr;
    int64_t insiz;
    int x = 0, y;
#ifndef WINVER
    struct stat st;
#endif

    errno = 0;
    memset(ctx, 0, sizeof(stream_t));
    if(!fn || !*fn) return 1;
    /* some DE uses URL on file drag'n'drop instead of a path */
    if(!memcmp(fn, "file://", 7)) {
        url = d = malloc(strlen(fn) + 1);
        if(!url) {
            main_getErrorMessage();
            return 1;
        }
        for(s = fn + 7; *s > ' ' && *s != '?' && *s != '#'; s++)
            if(*s == '%') {
                *d++ = (char)hex2bin(s + 1, 2);
                s += 2;
            } else
                *d++ = *s;
        *d = 0; fn = url;
        if(!*fn) {
            free(url);
            return 1;
        }
    }

    if(verbose) printf("stream_open(%s)\r\n", fn);

    ctx->compBuf = (unsigned char*)malloc(buffer_size);
    if(!ctx->compBuf) {
        main_getErrorMessage();
        if(url) free(url);
        return 1;
    }
    memset(ctx->compBuf, 0, buffer_size);
    ctx->verifyBuf = (char*)malloc(buffer_size);
    if(!ctx->verifyBuf) {
        main_getErrorMessage();
        if(url) free(url);
        free(ctx->compBuf); ctx->compBuf = NULL;
        return 1;
    }
    memset(ctx->verifyBuf, 0, buffer_size);
    ctx->buffer = (char*)malloc(buffer_size);
    if(!ctx->buffer) {
        main_getErrorMessage();
        if(url) free(url);
        free(ctx->compBuf); ctx->compBuf = NULL;
        free(ctx->verifyBuf); ctx->verifyBuf = NULL;
        return 1;
    }
    memset(ctx->buffer, 0, buffer_size);

    ctx->f = stream_fopen(fn, "rb");
    if(url) free(url);
    if(!ctx->f) return 1;
#ifdef WINVER
    fs = (uint64_t)_filelengthi64(_fileno(ctx->f));
#else
    if(!fstat(fileno(ctx->f), &st))
        fs = (uint64_t)st.st_size;
#endif
    ctx->avail = 0;
    if(!uncompr) {
        if(!(hs = fread(ctx->compBuf, 1, HEADER_SIZE, ctx->f))) {}
    }

    /* detect input format */
    /* only decompress buffer_size - 64k max, so that the first stream_read() call won't fail
     * decompressing because of output buffer being full */
    if(ctx->compBuf[0] == 0x1f && ctx->compBuf[1] == 0x8b) {
        /* gzip */
        if(verbose) printf(" gzip\r\n");
        /* see issue #109, this might cause errors if uncompressed size is actually bigger than 4G,
         * therefore don't mind the trailer, assume we don't know the uncompressed size */
/*
        myseek(ctx->f, fs - 4L);
        if(!fread(&ctx->fileSize, 4, 1, ctx->f))
            ctx->fileSize = 0;
        myseek(ctx->f, hs);
*/
        ctx->compSize = fs - 8;
        ctx->cmrdSize = hs;
        buff = ctx->compBuf + 3;
        x = *buff++; buff += 6;
        if(x & 4) { y = *buff++; y += (*buff++ << 8); buff += y; }
        if(x & 8) { while(*buff++ != 0); }
        if(x & 16) { while(*buff++ != 0); }
        if(x & 2) buff += 2;
        ctx->type = TYPE_DEFLATE;
        if((inflateInit2(&ctx->zstrm, -MAX_WBITS)) != Z_OK) { fclose(ctx->f); return 4; }
        ctx->zstrm.next_out = (unsigned char*)ctx->buffer;
        ctx->zstrm.avail_out = HEADER_SIZE;
        ctx->zstrm.next_in = buff;
        ctx->zstrm.avail_in = hs - (uint64_t)(buff - ctx->compBuf);
        do {
            if(!ctx->zstrm.avail_in) {
                insiz = ctx->compSize - ctx->cmrdSize;
                if(insiz < 1) { x = Z_STREAM_END; break; }
                if(insiz > buffer_size) insiz = buffer_size;
                ctx->zstrm.next_in = ctx->compBuf;
                ctx->zstrm.avail_in = insiz;
                if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                ctx->cmrdSize += (uint64_t)insiz;
            }
            x = inflate(&ctx->zstrm, Z_NO_FLUSH);
        } while(x == Z_OK && ctx->zstrm.avail_out > 0);
        if(x != Z_OK) {
            if(verbose) printf("  zlib inflate error %d\r\n", x);
            fclose(ctx->f); return 4;
        }
        ctx->avail = HEADER_SIZE - ctx->zstrm.avail_out;
    } else
    if(ctx->compBuf[0] == 'B' && ctx->compBuf[1] == 'Z' && ctx->compBuf[2] == 'h') {
        /* bzip2 */
        if(verbose) printf(" bzip2\r\n");
        ctx->compSize = fs;
        ctx->cmrdSize = hs;
        ctx->type = TYPE_BZIP2;
        if((BZ2_bzDecompressInit(&ctx->bstrm, 0, 0)) != BZ_OK) { fclose(ctx->f); return 4; }
        ctx->bstrm.next_out = ctx->buffer;
        ctx->bstrm.avail_out = HEADER_SIZE;
        ctx->bstrm.next_in = (char*)ctx->compBuf;
        ctx->bstrm.avail_in = hs;
        do {
            if(!ctx->bstrm.avail_in) {
                insiz = ctx->compSize - ctx->cmrdSize;
                if(insiz < 1) { x = BZ_STREAM_END; break; }
                if(insiz > buffer_size) insiz = buffer_size;
                ctx->bstrm.next_in = (char*)ctx->compBuf;
                ctx->bstrm.avail_in = insiz;
                if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                ctx->cmrdSize += insiz;
            }
            x = BZ2_bzDecompress(&ctx->bstrm);
        } while(x == BZ_OK && ctx->bstrm.avail_out > 0);
        if(x != BZ_OK) {
            if(verbose) printf("  bzip2 decompress error %d\r\n", x);
            fclose(ctx->f); return 4;
        }
        ctx->avail = HEADER_SIZE - ctx->bstrm.avail_out;
    } else
    if(ctx->compBuf[0] == 0xFD && ctx->compBuf[1] == '7' && ctx->compBuf[2] == 'z' && ctx->compBuf[3] == 'X' &&
        ctx->compBuf[4] == 'Z') {
        /* xz */
        if(verbose) printf(" xz\r\n");
        ctx->compSize = fs;
        ctx->cmrdSize = hs;
        ctx->type = TYPE_XZ;
        xz_crc32_init();
        ctx->xz = xzinit();
        if (!ctx->xz) { fclose(ctx->f); return 4; }
        ctx->xstrm.out = (unsigned char*)ctx->buffer;
        ctx->xstrm.out_pos = 0;
        ctx->xstrm.out_size = HEADER_SIZE;
        ctx->xstrm.in = (unsigned char*)ctx->compBuf;
        ctx->xstrm.in_pos = 0;
        ctx->xstrm.in_size = hs;
        do {
            if(ctx->xstrm.in_pos == ctx->xstrm.in_size) {
                insiz = ctx->compSize - ctx->cmrdSize;
                if(insiz < 1) { x = XZ_STREAM_END; break; }
                if(insiz > buffer_size) insiz = buffer_size;
                ctx->xstrm.in = (unsigned char*)ctx->compBuf;
                ctx->xstrm.in_pos = 0;
                ctx->xstrm.in_size = insiz;
                if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                ctx->cmrdSize += (uint64_t)insiz;
            }
            x = xz_dec_run(ctx->xz, &ctx->xstrm);
            if(x == XZ_UNSUPPORTED_CHECK) x = XZ_OK;
        } while(x == XZ_OK && ctx->xstrm.out_pos < ctx->xstrm.out_size);
        if(x != XZ_OK) {
            if(verbose) {
                if(x == XZ_MEMLIMIT_ERROR)
                    printf("  xz dictionary so big, doesn't fit into memory\r\n");
                else
                    printf("  xz decompress error %d\r\n", x);
            }
            fclose(ctx->f); return 4;
        }
        ctx->avail = ctx->xstrm.out_pos;
    } else
    if(ctx->compBuf[0] == 0x28 && ctx->compBuf[1] == 0xB5 && ctx->compBuf[2] == 0x2F && ctx->compBuf[3] == 0xFD) {
        /* zstandard */
        if(verbose) printf(" zstd\r\n");
        ctx->compSize = fs;
        ctx->cmrdSize = hs;
        zr = (uint64_t)ZSTD_getFrameContentSize(ctx->compBuf, sizeof(ctx->compBuf));
        if(zr != ZSTD_CONTENTSIZE_UNKNOWN && zr != ZSTD_CONTENTSIZE_ERROR)
            ctx->fileSize = zr;
        else
            ctx->fileSize = 0;
        ctx->type = TYPE_ZSTD;
        ctx->zstd = ZSTD_createDCtx();
        if (!ctx->zstd) { fclose(ctx->f); return 4; }
        ctx->zo.dst = ctx->buffer;
        ctx->zo.pos = 0;
        ctx->zo.size = HEADER_SIZE;
        ctx->zi.src = ctx->compBuf;
        ctx->zi.pos = 0;
        ctx->zi.size = hs;
        do {
            if(ctx->zi.pos == ctx->zi.size) {
                insiz = ctx->compSize - ctx->cmrdSize;
                if(insiz < 1) { x = 0; break; }
                if(insiz > buffer_size) insiz = buffer_size;
                ctx->zi.src = ctx->compBuf;
                ctx->zi.pos = 0;
                ctx->zi.size = insiz;
                if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                ctx->cmrdSize += (uint64_t)insiz;
            }
            x = (int) ZSTD_decompressStream(ctx->zstd, &ctx->zo, &ctx->zi);
        } while(!ZSTD_isError(x) && ctx->zo.pos < ctx->zo.size);
        if(ZSTD_isError(x)) {
            if(verbose) printf("  zstd decompress error %d\r\n", x);
            fclose(ctx->f); return 4;
        }
        ctx->avail = ctx->zo.pos;
    } else
    if(ctx->compBuf[0] == 'P' && ctx->compBuf[1] == 'K' && ctx->compBuf[2] == 3 && ctx->compBuf[3] == 4) {
        /* pkzip */
        if(verbose) printf(" pkzip\r\n");
        if((ctx->compBuf[6] & 1) || (ctx->compBuf[6] & (1<<6))) {
            fclose(ctx->f);
            return 2;
        }
        switch(ctx->compBuf[8]) {
            case 0: ctx->type = TYPE_PLAIN; break;
            case 8:
                ctx->type = TYPE_DEFLATE;
                if((inflateInit2(&ctx->zstrm, -MAX_WBITS)) != Z_OK) { fclose(ctx->f); return 4; }
                break;
            case 12:
                ctx->type = TYPE_BZIP2;
                if((BZ2_bzDecompressInit(&ctx->bstrm, 0, 0)) != BZ_OK) { fclose(ctx->f); return 4; }
                break;
            case 93:
                ctx->type = TYPE_ZSTD;
                if(!(ctx->zstd = ZSTD_createDCtx())) { fclose(ctx->f); return 4; }
                break;
            case 95:
                ctx->type = TYPE_XZ;
                xz_crc32_init();
                if(!(ctx->xz = xzinit())) { fclose(ctx->f); return 4; }
                break;
            default: fclose(ctx->f); return 3;
        }
        if(memcmp(ctx->compBuf + 18, "\xff\xff\xff\xff\xff\xff\xff\xff", 8)) {
            memcpy(&ctx->compSize, ctx->compBuf + 18, 4);
            memcpy(&ctx->fileSize, ctx->compBuf + 22, 4);
        } else {
            /* zip64 */
            if(verbose) printf("   zip64\r\n");
            for(x = 30 + ctx->compBuf[26] + (ctx->compBuf[27]<<8), y = x + ctx->compBuf[28] + (ctx->compBuf[29]<<8);
                x < y && x < (int)sizeof(ctx->compBuf) - 4; x += 4 + ctx->compBuf[x + 2] + (ctx->compBuf[x + 3]<<8))
                    if(ctx->compBuf[x] == 1 && ctx->compBuf[x + 1] == 0) {
                        memcpy(&ctx->compSize, ctx->compBuf + x + 12, 8);
                        memcpy(&ctx->fileSize, ctx->compBuf + x + 4, 8);
                        break;
                    }
            if(!ctx->compSize || !ctx->fileSize) { fclose(ctx->f); return 2; }
        }
        myseek(ctx->f, (uint64_t)(30 + ctx->compBuf[26] + (ctx->compBuf[27]<<8) + ctx->compBuf[28] + (ctx->compBuf[29]<<8)));
    } else
    if(ctx->compBuf[0] == 'Z' && ctx->compBuf[1] == 'Z' && ctx->compBuf[2] == 'z' && ctx->compBuf[3] == 0x1A) {
        /* ZZZip, per entity compressed */
        if(verbose) printf(" ZZZip\r\n");
        x = (ctx->compBuf[7] & 0x80 << 9) | (ctx->compBuf[5] << 8) | ctx->compBuf[4];
        y = ((ctx->compBuf[7] & 0x7F) << 8) | ctx->compBuf[6];
        if(y < 2 || (ctx->compBuf[15] & 0xF0) || (ctx->compBuf[15] & 0x0F) > 1) { fclose(ctx->f); return 2; }
        memcpy(&ctx->compSize, ctx->compBuf + 32, 8);
        memcpy(&ctx->fileSize, ctx->compBuf + 16, 8);
        if(!ctx->compSize || !ctx->fileSize) { fclose(ctx->f); return 2; }
        ctx->type = TYPE_PLAIN;
        if((ctx->compBuf[15] & 0x0F) == 1)
            switch(ctx->compBuf[48]) {
                case 2:
                    ctx->type = TYPE_DEFLATE;
                    if((inflateInit2(&ctx->zstrm, -MAX_WBITS)) != Z_OK) { fclose(ctx->f); return 4; }
                    break;
                case 3:
                    ctx->type = TYPE_BZIP2;
                    if((BZ2_bzDecompressInit(&ctx->bstrm, 0, 0)) != BZ_OK) { fclose(ctx->f); return 4; }
                    break;
                case 5:
                    ctx->type = TYPE_XZ;
                    xz_crc32_init();
                    if(!(ctx->xz = xzinit())) { fclose(ctx->f); return 4; }
                    break;
                case 7:
                    ctx->type = TYPE_ZSTD;
                    if(!(ctx->zstd = ZSTD_createDCtx())) { fclose(ctx->f); return 4; }
                    break;
                default: fclose(ctx->f); return 3;
            }
        myseek(ctx->f, (uint64_t)(x + y));
    } else
    if(ctx->compBuf[0] == '7' && ctx->compBuf[1] == 'z' && ctx->compBuf[2] == 0xBC && ctx->compBuf[3] == 0xAF) {
        /* 7zip */
        if(verbose) printf(" 7z (deliberately not supported, use xz instead)\r\n");
        /* this is a badly designed, non-transmission-error-proof, inadeqvate for long term preservation format
         * with a particularly badly written, badly documented, non-portable SDK which I simply refuse to support.
         * Don't use this format if your data is precious to you. Better to use tar.xz.  You have been warned. */
        fclose(ctx->f); return 3;
    } else {
        /* uncompressed image */
        if(verbose) printf(" raw image\r\n");
        ctx->fileSize = fs;
        ctx->type = TYPE_PLAIN;
        memcpy(ctx->buffer, ctx->compBuf, hs);
        ctx->avail = hs;
    }
    if(ctx->avail) {
        fs = 0;
        if(ctx->buffer[257] == 'u' && ctx->buffer[258] == 's' && ctx->buffer[259] == 't' && ctx->buffer[260] == 'a' &&
            ctx->buffer[261] == 'r') {
            /* ustar */
            if(verbose) printf(" tar\r\n");
            if(ctx->buffer[156] != 0 && ctx->buffer[156] != '0') { fclose(ctx->f); return 2; }
            ctx->fileSize = oct2bin(ctx->buffer + 0x7C, 11);
            fs = 512;
        } else
        if(ctx->buffer[0] == '0' && ctx->buffer[1] == '7' && ctx->buffer[2] == '0' && ctx->buffer[3] == '7' &&
            ctx->buffer[4] == '0') {
            /* cpio */
            fs = ctx->avail + 1;
            if(ctx->buffer[5] == '7') {
                if(verbose) printf(" cpio hpodc\r\n");
                ctx->fileSize = oct2bin(ctx->buffer + 8*6 + 11 + 6, 11);
                x = (int)oct2bin(ctx->buffer + 8*6 + 11, 6);
                fs = 9*6 + 2*11 + x;
            } else
            if(ctx->buffer[5] == '1' || ctx->buffer[5] == '2') {
                if(verbose) printf(" cpio\r\n");
                ctx->fileSize = hex2bin(ctx->buffer + 8*6 + 6, 8);
                x = (int)hex2bin(ctx->buffer + 8*11 + 6, 8);
                fs = (110 + x + 3) & ~3;
            }
        } else
        if(ctx->buffer[0] == 'Z' && ctx->buffer[1] == 'Z' && ctx->buffer[2] == 'z' && ctx->buffer[3] == 0x1A) {
            /* ZZip overall compressed */
            if(verbose) printf(" ZZZip\r\n");
            x = ((ctx->buffer[7] & 0x7F) << 8) | ctx->buffer[6];
            fs = ((ctx->buffer[7] & 0x80 << 9) | (ctx->buffer[5] << 8) | ctx->buffer[4]) + x;
            if(x < 2 || ctx->buffer[15]) { fclose(ctx->f); return 2; }
            memcpy(&ctx->fileSize, ctx->buffer + 16, 8);
        }
        if(ctx->type == TYPE_PLAIN) {
            myseek(ctx->f, fs);
            ctx->avail = 0;
        } else if(fs > 0) {
            if(!ctx->fileSize) { fclose(ctx->f); return 4; }
            /* this can only happen if header is bigger than the 64k we've decompressed so far. Hopefully
             * will never happen with sane archives, that would require an extra huge filename (> PATH_MAX) */
            if(fs > ctx->avail) { fclose(ctx->f); return 2; }
            memcpy(ctx->buffer, ctx->buffer + fs, ctx->avail - fs);
            ctx->avail -= fs;
        }
        ctx->readSize = ctx->avail;
    }
    if(verbose) printf(" type %d compSize %" PRIu64 " fileSize %" PRIu64
        " avail %" PRIu64 " data offset %" PRIu64 "\r\n",
        ctx->type, ctx->compSize, ctx->fileSize, ctx->avail, mytell(ctx->f));
    if(!ctx->compSize && !ctx->fileSize) { fclose(ctx->f); return 1; }

    ctx->start = time(NULL);
    return 0;
}

/**
 * Read no more than buffer_size uncompressed bytes of source data
 */
int stream_read(stream_t *ctx)
{
    int ret = 0;
    int64_t size = 0, insiz;

    errno = 0;
    size = ctx->fileSize - ctx->readSize;
    if(size < 1) { if(ctx->fileSize) return 0; size = 0; }
    if(size > buffer_size) size = buffer_size;
    if(verbose > 1)
        printf("stream_read() readSize %" PRIu64 " / fileSize %" PRIu64 " (input size %"
            PRId64 "), cmrdSize %" PRIu64 " / compSize %" PRIu64 "u\r\n",
            ctx->readSize, ctx->fileSize, size, ctx->cmrdSize, ctx->compSize);

    switch(ctx->type) {
        case TYPE_PLAIN:
            if(!(size = fread(ctx->buffer + ctx->avail, 1, size, ctx->f))) {}
        break;
        case TYPE_DEFLATE:
            ctx->zstrm.next_out = (unsigned char*)ctx->buffer + ctx->avail;
            ctx->zstrm.avail_out = buffer_size - ctx->avail;
            do {
                if(!ctx->zstrm.avail_in) {
                    insiz = ctx->compSize - ctx->cmrdSize;
                    if(insiz < 1) { ret = Z_STREAM_END; break; }
                    if(insiz > buffer_size) insiz = buffer_size;
                    if(verbose > 1) printf("  deflate cmrdSize %" PRIu64
                        " insiz %" PRId64 "\r\n", ctx->cmrdSize, insiz);
                    ctx->zstrm.next_in = ctx->compBuf;
                    ctx->zstrm.avail_in = insiz;
                    if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                    ctx->cmrdSize += (uint64_t)insiz;
                }
                ret = inflate(&ctx->zstrm, Z_NO_FLUSH);
            } while(ret == Z_OK && ctx->zstrm.avail_out > 0);
            if(ret != Z_OK && ret != Z_STREAM_END) {
                if(verbose) printf("  zlib inflate error %d\r\n", ret);
                return -1;
            }
            size = buffer_size - ctx->zstrm.avail_out;
        break;
        case TYPE_BZIP2:
            ctx->bstrm.next_out = ctx->buffer + ctx->avail;
            ctx->bstrm.avail_out = buffer_size - ctx->avail;
            do {
                if(!ctx->bstrm.avail_in) {
                    insiz = ctx->compSize - ctx->cmrdSize;
                    if(insiz < 1) { ret = BZ_STREAM_END; break; }
                    if(insiz > buffer_size) insiz = buffer_size;
                    if(verbose > 1) printf("  bzip2 cmrdSize %" PRIu64
                        " insiz %" PRId64 "\r\n", ctx->cmrdSize, insiz);
                    ctx->bstrm.next_in = (char*)ctx->compBuf;
                    ctx->bstrm.avail_in = insiz;
                    if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                    ctx->cmrdSize += (uint64_t)insiz;
                }
                ret = BZ2_bzDecompress(&ctx->bstrm);
            } while(ret == BZ_OK && ctx->bstrm.avail_out > 0);
            if(ret != BZ_OK && ret != BZ_STREAM_END) {
                if(verbose) printf("  bzip2 decompress error %d\r\n", ret);
                return -1;
            }
            size = buffer_size - ctx->bstrm.avail_out;
        break;
        case TYPE_XZ:
            ctx->xstrm.out = (unsigned char*)ctx->buffer;
            ctx->xstrm.out_pos = ctx->avail;
            ctx->xstrm.out_size = buffer_size;
            do {
                if(ctx->xstrm.in_pos == ctx->xstrm.in_size) {
                    insiz = ctx->compSize - ctx->cmrdSize;
                    if(insiz < 1) { ret = XZ_STREAM_END; break; }
                    if(insiz > buffer_size) insiz = buffer_size;
                    if(verbose > 1) printf("  xz cmrdSize %" PRIu64
                        " insiz %" PRId64 "\r\n", ctx->cmrdSize, insiz);
                    ctx->xstrm.in = (unsigned char*)ctx->compBuf;
                    ctx->xstrm.in_pos = 0;
                    ctx->xstrm.in_size = insiz;
                    if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                    ctx->cmrdSize += (uint64_t)insiz;
                }
                ret = xz_dec_run(ctx->xz, &ctx->xstrm);
                if(ret == XZ_UNSUPPORTED_CHECK) ret = XZ_OK;
            } while(ret == XZ_OK && ctx->xstrm.out_pos < ctx->xstrm.out_size);
            if(ret != XZ_OK && ret != XZ_STREAM_END) {
                if(verbose) printf("  xz decompress error %d\r\n", ret);
                return -1;
            }
            size = ctx->xstrm.out_pos;
        break;
        case TYPE_ZSTD:
            ctx->zo.dst = ctx->buffer;
            ctx->zo.pos = ctx->avail;
            ctx->zo.size = buffer_size;
            do {
                if(ctx->zi.pos == ctx->zi.size) {
                    insiz = ctx->compSize - ctx->cmrdSize;
                    if(insiz < 1) { ret = 0; break; }
                    if(insiz > buffer_size) insiz = buffer_size;
                    if(verbose > 1) printf("  zstd cmrdSize %" PRIu64
                        " insiz %" PRId64 "\r\n", ctx->cmrdSize, insiz);
                    ctx->zi.src = ctx->compBuf;
                    ctx->zi.pos = 0;
                    ctx->zi.size = insiz;
                    if(!fread(ctx->compBuf, insiz, 1, ctx->f)) break;
                    ctx->cmrdSize += (uint64_t)insiz;
                }
                ret = (int) ZSTD_decompressStream(ctx->zstd, &ctx->zo, &ctx->zi);
            } while(!ZSTD_isError(ret) && ctx->zo.pos < ctx->zo.size);
            if(ZSTD_isError(ret)) {
                if(verbose) printf("  zstd decompress error %d\r\n", ret);
                return -1;
            }
            size = ctx->zo.pos;
        break;
    }
    while(size & 511) ctx->buffer[size++] = 0;
    if(verbose > 1) printf("stream_read() output size %" PRId64 "\r\n", size);
    ctx->readSize += (uint64_t)size;
    ctx->avail = 0;
    return size;
}

/**
 * Get a reference to the destination file system
 */
int stream_dst(char *fn, FILE *f)
{
#ifdef WINVER
    char tmp[MAX_PATH + 1];
    (void)f;
    GetFullPathNameA(fn, MAX_PATH, tmp, NULL);
    return tmp[1] == ':' ? (int)tmp[0] : 0;
#else
    (void)fn;
    return fileno(f);
#endif
}

/**
 * Return available space on destination file system
 */
uint64_t stream_avail(int fd)
{
#ifdef WINVER
    char tmp[MAX_PATH + 1];
    DWORD spc = 0, bps = 0, fc = 0, tc;
    tmp[0] = fd; tmp[1] = ':'; tmp[2] = '\\'; tmp[3] = 0;
    if(fd && GetDiskFreeSpaceA(tmp, &spc, &bps, &fc, &tc))
        return (uint64_t)fc * (uint64_t)spc * (uint64_t)bps;
#else
    struct statvfs vfs;
    if(fd && !fstatvfs(fd, &vfs))
        return (uint64_t)vfs.f_bavail * (uint64_t)vfs.f_bsize;
#endif
    /* return the largest unsigned number if we don't know the limit */
    return (uint64_t)-1UL;
}

/**
 * Open file for writing
 */
int stream_create(stream_t *ctx, char *fn, int comp, uint64_t size)
{
    errno = 0;
    memset(ctx, 0, sizeof(stream_t));

    if(verbose)
        printf("stream_create(%s) comp %d size %" PRIu64 ")\r\n", fn, comp, size);

    if(!fn || !*fn || !size) return 1;

    ctx->compBuf = (unsigned char*)malloc(buffer_size);
    if(!ctx->compBuf) {
        main_getErrorMessage();
        return 1;
    }
    ctx->buffer = (char*)malloc(buffer_size);
    if(!ctx->buffer) {
        main_getErrorMessage();
        free(ctx->compBuf); ctx->compBuf = NULL;
        return 1;
    }

    if(comp) {
        ctx->type = TYPE_ZSTD;
        ctx->zcmp = ZSTD_createCCtx();
        ctx->g = stream_fopen(fn, "wb");
        if(!ctx->g || !ctx->zcmp) {
            main_getErrorMessage();
            if(ctx->g) fclose(ctx->g);
            ctx->g = NULL;
            if(ctx->zcmp) ZSTD_freeCCtx(ctx->zcmp);
            ctx->zcmp = NULL;
            free(ctx->buffer); ctx->buffer = NULL;
            free(ctx->compBuf); ctx->compBuf = NULL;
            return 1;
        }
        dstfd = stream_dst(fn, ctx->g);
        ZSTD_CCtx_setParameter(ctx->zcmp, ZSTD_c_compressionLevel, 1);
        ZSTD_CCtx_setParameter(ctx->zcmp, ZSTD_c_nbWorkers, 4);
        ZSTD_CCtx_setPledgedSrcSize(ctx->zcmp, size);
    } else {
        ctx->type = TYPE_PLAIN;
        ctx->f = stream_fopen(fn, "wb");
        if(!ctx->f) {
            main_getErrorMessage();
            free(ctx->buffer); ctx->buffer = NULL;
            free(ctx->compBuf); ctx->compBuf = NULL;
            return 1;
        }
        dstfd = stream_dst(fn, ctx->f);
    }

    ctx->fileSize = size;
    ctx->start = time(NULL);
    return 0;
}

/**
 * Compress and write out data
 */
int stream_write(stream_t *ctx, char *buffer, int size)
{
    int i;
    uint64_t avail;
    size_t remaining;
    if(verbose > 1)
        printf("stream_write() readSize %" PRIu64 " / fileSize %" PRIu64 " (output size %d)\r\n",
            ctx->readSize, ctx->fileSize, size);
    errno = 0;
    ctx->readSize += (uint64_t)size;

    /* don't rely on OS reporting "no space left on device", go ahead that by buffer size times 2. See issue #50 */
    if(dstfd && (avail = stream_avail(dstfd)) && avail <= (((uint64_t)buffer_size) << 1)) {
        if(verbose) printf("stream_write() available size %" PRIu64 " <= 2 * buffer_size\r\n", avail);
#ifdef WINVER
        SetLastError(ERROR_DISK_FULL);
#else
        errno = ENOSPC;
#endif
        return 0;
    }

    switch(ctx->type) {
        case TYPE_PLAIN:
            /* check if the data contains only zeros nothing else */
            for(i = 0; i < size && !buffer[i]; i++) {}
            /* there's a bug in the newest Windows 10 kernel, see issue #53, so do not use sparse file under Win */
#if !defined(WINVER) || defined(WINKRNL_NOT_BUGGY_ANY_MORE)
            if(i == size) {
                /* if all bytes zero, then don't write just seek, that will create a sparse file */
                fseek(ctx->f, (long)size, SEEK_CUR);
            } else
#endif
            {
                /* we have some non-zero data, write the block as-is */
                if(!fwrite(buffer, size, 1, ctx->f))
                    size = 0;
            }
        break;
        case TYPE_ZSTD:
            ctx->zi.src = buffer; ctx->zi.size = size; ctx->zi.pos = 0;
            ctx->zo.dst = ctx->compBuf; ctx->zo.size = buffer_size; ctx->zo.pos = 0;
            do {
                remaining = ZSTD_compressStream2(ctx->zcmp, &ctx->zo , &ctx->zi,
                    ctx->readSize >= ctx->fileSize ? ZSTD_e_end : ZSTD_e_continue);
                if(!fwrite(ctx->compBuf, ctx->zo.pos, 1, ctx->g)) {
                    size = 0;
                    break;
                }
            } while(ctx->readSize >= ctx->fileSize ? (remaining != 0) : (ctx->zi.pos != (size_t)size));
        break;
    }
    if(verbose > 1) printf("stream_write() output size %d\r\n", size);
    return size;
}

/**
 * Close stream descriptors
 */
void stream_close(stream_t *ctx)
{
    if(verbose) printf("stream_close()\r\n");
    if(ctx->compBuf) free(ctx->compBuf);
    if(ctx->verifyBuf) free(ctx->verifyBuf);
    if(ctx->buffer) free(ctx->buffer);
    if(ctx->f) fclose(ctx->f);
    if(ctx->g) fclose(ctx->g);
    switch(ctx->type) {
        case TYPE_DEFLATE: inflateEnd(&ctx->zstrm); break;
        case TYPE_BZIP2: BZ2_bzDecompressEnd(&ctx->bstrm); break;
        case TYPE_XZ: xz_dec_end(ctx->xz); break;
        case TYPE_ZSTD:
            if(ctx->zstd) ZSTD_freeDCtx(ctx->zstd);
            if(ctx->zcmp) ZSTD_freeCCtx(ctx->zcmp);
        break;
    }
    dstfd = 0;
}

/**
 * Check and set a valid baud rate
 */
void stream_baud(int rate)
{
    int i, bauds[] = { 57600, 115200, 230400, 460800, 500000, 576000,
        921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000,
        3500000, 4000000 };
    for(i = 0; i < (int)(sizeof(bauds)/sizeof(bauds[0])) && bauds[i] <= rate; i++)
        baud = bauds[i];
}

