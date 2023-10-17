/*
 * usbimager/main_tui.c
 *
 * Copyright (C) 2023 bzt (bztsrc@gitlab)
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
 * @brief Ncurses-like Text User interface for Linux
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include "lang.h"
#include "stream.h"
#include "disks.h"

#if !defined(USE_WRONLY) || !USE_WRONLY
#define NUMFLD 6
static char *blksizeList[10] = { "  1M", "  2M", "  4M", "  8M", " 16M", " 32M", " 64M", "128M", "256M", "512M" };
#else
#define NUMFLD 2
#endif

typedef struct {
    char *name;
    char type;
    uint64_t size;
    time_t time;
} filelist_t;

char **lang = NULL;
extern char *dict[NUMLANGS][NUMTEXTS + 1];

static char *bkpdir = NULL;
static int blksizesel = 0;
char *main_errorMessage = NULL, *main_errorTitle = NULL;
static char source[PATH_MAX], targetList[DISKS_MAX][128], *targetPtr[DISKS_MAX], status[128];
static char path[PATH_MAX];

struct termios otio, ntio;
int flg, needVerify = 1, needCompress = 0, progress = 0, numTargetList = 0, targetId = 0;
int sorting = 0, numFiles = 0;
filelist_t *files = NULL;

enum { PRE, SUF, VER, HOR, NW, NE, SW, SE };
char **t, *terms[3][8] = {
    { "", "", "\xe2\x94\x80", "\xe2\x94\x82", "\xe2\x94\x8c", "\xe2\x94\x90", "\xe2\x94\x94", "\xe2\x94\x98" },
    { "\033(0", "\033(B", "q", "x", "l", "k", "m", "j" },
    { "", "", "-", "|", "+", "+", "+", "+" }
};
int menu = 0, mainsel = 0, col = 0, row = 0, chkl = 0, chkh = 0, chkscr = 0, statx, staty, statw;
void mainRedraw(void);

static int fncmp(const void *a, const void *b)
{
    filelist_t *A = (filelist_t*)a, *B = (filelist_t*)b;
    if(sorting < 4) {
        if(A->type && !B->type) return 1;
        if(!A->type && B->type) return -1;
    }
    switch(sorting) {
        case 0: return strcmp(A->name, B->name);
        case 1: return strcmp(B->name, A->name);
        case 2: return A->size > B->size;
        case 3: return B->size > A->size;
        case 4: return (int)(A->time - B->time);
        case 5: return (int)(B->time - A->time);
    }
    return 0;
}

void freefiles(void)
{
    int i;
    if(files) {
        for(i = 0; i < numFiles; i++)
            if(files[i].name) free(files[i].name);
        free(files); files = NULL;
    }
    numFiles = chkl = chkscr = 0;
}

void readdirectory(void)
{
    DIR *dir;
    struct dirent *de;
    struct stat st;
    int i, j;

    freefiles();
    j = strlen(path);
    if(!j || path[j - 1] != '/') path[j++] = '/';
    dir = opendir(path);
    if(dir) {
        while((de = readdir(dir))) {
            if(de->d_name[0] == '.') continue;
            strcpy(path + j, de->d_name);
            if(stat(path, &st) || (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode) && !S_ISBLK(st.st_mode))) continue;
            i = numFiles++;
            files = (filelist_t*)realloc(files, numFiles * sizeof(filelist_t));
            if(!files) { numFiles = 0; break; }
            files[i].name = (char*)malloc(strlen(de->d_name)+1);
            if(!files[i].name) { numFiles--; continue; }
            strcpy(files[i].name, de->d_name);
            files[i].type = S_ISDIR(st.st_mode) ? 0 : (S_ISBLK(st.st_mode) ? 1 : 2);
            files[i].size = st.st_size;
            files[i].time = st.st_mtime;
        }
        closedir(dir);
        qsort(files, numFiles, sizeof(filelist_t), fncmp);
    }
    memset(path + j, 0, sizeof(path) - j);
}

void getstdindim(int *r, int *c)
{
    struct winsize ws;
    ioctl(0, TIOCGWINSZ, &ws);
    *r = ws.ws_row; *c = ws.ws_col;
}

void restorestdin(void)
{
    tcsetattr(0, TCSANOW, &otio);
    printf("\033[0m\033[H\033[J\033[?25h");
}

void ctrlc(int sig)
{
    (void)sig;
    restorestdin();
    exit(0);
}

void setupstdin(void)
{
    signal(SIGINT, ctrlc);
    flg = fcntl(0, F_GETFL);
    tcgetattr(0, &otio);
    ntio = otio;
    ntio.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(0, TCSANOW, &ntio);
}

char getcsi(void)
{
    char c, d;
    c = getchar();
    if(c == 0x1b) {
        fcntl(0, F_SETFL, flg | O_NONBLOCK);
        c = getchar();
        if(c == -1) { fcntl(0, F_SETFL, flg); return 0x1b; }
        while(c != -1) { d = c; c = getchar(); }
        fcntl(0, F_SETFL, flg);
        switch(d) {
            case 'A': return 1;
            case 'B': return 2;
            case 'C': return 3;
            case 'D': return 4;
            default: return 0;
        }
    }
    return c;
}

int mystrlen(const char *s)
{
    register size_t c=0;
    if(s) {
        while(*s) {
            if((*s & 128) != 0) {
                if((*s & 32) == 0 ) s++; else
                if((*s & 16) == 0 ) s+=2; else
                if((*s & 8) == 0 ) s+=3;
            }
            c++;
            s++;
        }
    }
    return c;
}

void bg(void) {
    int i;
    printf("\033[36;44;1m\033[H\033[J USBImager " USBIMAGER_VERSION "\n %s", t[PRE]);
    for(i = 0; i < col-2; i++) printf(t[VER]);
    printf("%s\033[%d;1H\033[0m\033[%d;44m[1] UTF-8, [2] VT100, [3] VT52",t[SUF],row, t == terms[2]?36:94);
}

void drawline(int x, int y, int w, int i, int a, char *sel, char *ina)
{
    int j;
    printf("\033[%s;%dm\033[%d;%dH%s%s%s \033[%sm",a?"0;30":ina,a?47:(t==terms[2]?46:100),y,x,t[PRE],t[HOR],t[SUF],i?sel:"30;1");
    for(j = 0; j < w - 4; j++) printf(" ");
    printf("\033[%s;%dm %s%s%s\033[40m  \033[%d;%dH\033[%s;%dm",a?"37;1":ina,a?47:(t==terms[2]?46:100),t[PRE],t[HOR],t[SUF],y,x+3,
        i?sel:(a?"0;30":"0;34"),i?1:(a?47:(t==terms[2]?46:100)));
}

void drawboxtop(int x, int y, int w, int i, int a,char *ina)
{
    int j;
    printf("\033[%d;%dH\033[%s;%dm%s%s",y,x,a?"0;30":ina,a?47:(t==terms[2]?46:100),t[PRE],t[NW]);
    for(j = 0; j < w - 2; j++) printf(t[VER]);
    printf("\033[%sm%s%s%s\033[%s;%dm\033[%d;%dH",a?"37;1":ina,t[NE],t[SUF], i?"\033[40m  ":"", a?"1;30":"1;34",
        a?47:(t==terms[2]?46:100), y, x + 3);
}

void drawboxbtm(int x, int y, int w, int a, char *ina)
{
    int j;
    printf("\033[%d;%dH\033[%s;%dm%s%s\033[%sm",y,x,a?"0;30":ina,a?47:(t==terms[2]?46:100),t[PRE],t[SW],a?"37;1":ina);
    for(j = 0; j < w - 2; j++) printf(t[VER]);
    printf("%s%s\033[40m  \033[0m",t[SE],t[SUF]);
}

void drawshd(int x, int y, int w) {
    int i;
    printf("\033[40m\033[%d;%dH",y,x+2);
    for(i = 0; i < w; i++) printf(" ");
    printf("\033[0m");
}

void drawtext(char *s, int w)
{
    int i = mystrlen(s);
    for(; i > w; i--) {
        if((*s & 128) != 0) {
            if((*s & 32) == 0 ) s++; else
            if((*s & 16) == 0 ) s+=2; else
            if((*s & 8) == 0 ) s+=3;
        }
        s++;
    }
    printf("%s", s);
    for(; i < w; i++) printf(" ");
}

void drawselect(char **list, int len, int x, int y, int w, int curr)
{
    int h, i;

    if(!list) curr = len = 0;
    chkh = (len > row - 6 ? row - 6: len); h = chkh + 2; chkl = len;
    y = (row - h) / 2 + 1; if(y < 1) y = 1;
    if(curr < chkscr) chkscr = curr;
    if(curr > chkscr + chkh - 1) chkscr = curr - chkh + 1;
    if(chkscr < 0) chkscr = 0;
    drawboxtop(x,y,w,0,1,"0;37");
    for(i = 0; i < chkh; i++) {
        drawline(x,y+1+i,w,chkscr+i==curr,1,"37;44;1","0;37");
        printf("%s",list?list[chkscr+i]:"?");
    }
    drawboxbtm(x,y+h-1,w,1,"0;37");
    drawshd(x,y+h,w);
}

char ctrlselect(char c, int *curr, int max)
{
    switch(c) {
        case 1: if(*curr > 0) (*curr)--; else *curr = max-1; break;
        case 2: if(*curr < max-1) (*curr)++; else *curr = 0; break;
        case 10: menu = 0; col = 0; c = 0; break;
    }
    return c;
}

void drawfilesel(void)
{
    int i;
    char tmp[64];
    uint64_t size;
    struct tm *lt;
    time_t now = time(NULL), diff;

    printf("\033[0;30;47m\033[%d;%dH  \033[30;1m",3,4);
    for(i = 0; i < col - 12; i++) printf(" ");
    printf("\033[37;1;47m  \033[B\033[40m  \033[%d;%dH\033[0;30;47m",3,7);
    drawtext(path, col - 12);
    drawboxtop(4,4,col-8,0,1,"");
    for(i = 0; i < row - 7; i++) {
        drawline(4,i+5,col-8,chkscr+i==chkl,1,"37;44;1","0;37");
        if(chkscr+i < numFiles && files) {
            printf("%c %s", files[chkscr+i].type == 0 ? '/' : (files[chkscr+i].type == 1 ? 'b' : ' '), files[chkscr+i].name);
            if(files[chkscr+i].type) {
                size = files[chkscr+i].size;
                if(size < 1024L*1024L)
                    sprintf(tmp, "%u", (unsigned int)size);
                else {
                    size >>= 20;
                    if(size < 1024L)
                        sprintf(tmp, "%u %s", (unsigned int)size, lang[L_MIB]);
                    else {
                        size >>= 10;
                        sprintf(tmp, "%u %s", (unsigned int)size, lang[L_GIB]);
                    }
                }
                printf("\033[%d;%dH%7s",i+5,col-32,tmp);
            }
            diff = now - files[chkscr+i].time;
            if(diff < 120) strcpy(tmp, lang[L_NOW]); else
            if(diff < 3600) sprintf(tmp, lang[L_MSAGO], (int)(diff/60)); else
            if(diff < 7200) sprintf(tmp, lang[L_HAGO], (int)(diff/60)); else
            if(diff < 24*3600) sprintf(tmp, lang[L_HSAGO], (int)(diff/3600)); else
            if(diff < 48*3600) strcpy(tmp, lang[L_YESTERDAY]); else {
                lt = localtime(&files[chkscr+i].time);
                if(diff < 7*24*3600) strcpy(tmp, lang[L_WDAY0 + lt->tm_wday]); else
                    sprintf(tmp, "%04d-%02d-%02d", lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday);
            }
            printf("\033[%d;%dH%s",i+5,col-24,tmp);
        }
    }
    drawboxbtm(4,row-2,col-8,1,"");
    drawshd(4,row-1,col-8);
}

char ctrlfilesel(char c)
{
    int l;
    char *s;

    switch(c) {
        case 1: if(chkl > 0) chkl--; else chkl = numFiles-1; break;
        case 2: if(chkl < numFiles-1) chkl++; else chkl = 0; break;
        case 3: case 10:
            if(!files[chkl].type) {
                strcat(path, files[chkl].name);
                readdirectory();
            } else {
                if(files && chkl < numFiles) {
                    strcpy(source, path);
                    strcat(source, files[chkl].name);
                } else memset(source, 0, sizeof(source));
                menu = 0; col = 0; c = 0; freefiles();
            }
        break;
        case 4: case 7:
            l = strlen(path); if(l && path[l - 1] == '/') path[l - 1] = 0;
            s = strrchr(path, '/'); if(s) *s = 0; else strcpy(path, "/");
            l = strlen(path); memset(path + l, 0, sizeof(path) - l);
            readdirectory();
        break;
    }
    if(chkl < chkscr) chkscr = chkl;
    if(chkl > chkscr + (row - 8)) chkscr = chkl - (row - 8);
    return c;
}

void main_addToCombobox(char *option)
{
    strncpy(targetList[numTargetList++], option, 128);
}

void main_getErrorMessage(void)
{
    main_errorMessage = errno ? strerror(errno) : NULL;
}

void main_onProgress(void *data)
{
    stream_t *ctx = (stream_t*)data;
    int r = 0, c = 0, i, progress;

    if(!ctx) {
        progress = 0;
        strcpy(status, lang[L_WAITING]);
    } else
        progress = stream_status(ctx, status, 0);

    getstdindim(&r, &c);
    if(r != row || c != col) mainRedraw();
    printf("\033[%d;%dH\033[46m",staty,statx);
    r = progress * 100 / statw; if(r > statw) r = statw;
    for(i = 0; i < r; i++) printf(" ");
    printf("\033[0;37;%dm", (t==terms[2]?40:100));
    for(; i < statw; i++) printf(" ");
    printf("\033[%d;%dH\033[30;47m",staty+1,statx);
    drawtext(status, statw);
    printf("\033[0m\033[%d;%dH\033[?25l",row,col);
}

void main_onError(char *msg)
{
    int i, x, y, w, h;

    if(msg && *msg) {
        i = mystrlen(lang[L_ERROR]) + 4;
        w = mystrlen(msg) + 4; if(w > i) i = w;
        w = mystrlen(main_errorMessage) + 4; if(w < i) w = i;
        h = main_errorMessage && *main_errorMessage ? 5 : 4;
        x = (col - w) / 2; if(x < 1) x = 1;
        y = (row - h) / 2; if(y < 1) y = 1;
        printf("\033[41;37;1m\033[%d;%dH",y,x);
        for(i = 0; i < w; i++) printf(" ");
        printf("\033[%d;%dH\033[41;33;1m%s\033[41;37;1m\033[%d;%dH",y,x+2,lang[L_ERROR],y+1,x);
        for(i = 0; i < w; i++) printf(" ");
        printf("\033[40m  \033[41;37;1m\033[%d;%dH",y+2,x);
        for(i = 0; i < w; i++) printf(" ");
        printf("\033[40m  \033[41;37;1m\033[%d;%dH",y+2,x+2);
        drawtext(msg, w - 4);
        if(main_errorMessage && *main_errorMessage) {
            printf("\033[40m  \033[41;37;1m\033[%d;%dH",y+3,x+2);
            drawtext(main_errorMessage, w - 4);
        }
        printf("\033[%d;%dH",y+h-1,x);
        for(i = 0; i < w; i++) printf(" ");
        printf("\033[40m  ");
        drawshd(x,y+h,w);
        printf("\033[0m\033[%d;%dH\033[?25l",row,col);
        main_errorMessage = NULL;
        getcsi();
        mainRedraw();
    }
}

/**
 * Function that reads from input and writes to disk
 */
static void *writerRoutine(void)
{
    int dst, numberOfBytesRead;
    int numberOfBytesWritten, numberOfBytesVerify, needWrite;
    static stream_t ctx;

    ctx.readSize = 0;
    dst = stream_open(&ctx, source, targetId >= 0 && targetId < DISKS_MAX && disks_targets[targetId] >= 1024);
    if(!dst) {
        dst = (int)((long int)disks_open(targetId, ctx.fileSize));
        if(dst > 0) {
            while(1) {
                if((numberOfBytesRead = stream_read(&ctx)) >= 0) {
                    if(numberOfBytesRead == 0) {
                        if(!ctx.fileSize) ctx.fileSize = ctx.readSize;
                        break;
                    } else {
                        errno = 0; needWrite = 1;
                        if(!force) {
                            numberOfBytesVerify = read(dst, ctx.verifyBuf, numberOfBytesRead);
                            if(numberOfBytesVerify == numberOfBytesRead &&
                                !memcmp(ctx.buffer, ctx.verifyBuf, numberOfBytesRead)) {
                                if(verbose > 1) printf("  numberOfBytesVerify %d matches disk, skipping write\n", numberOfBytesRead);
                                needWrite = 0;
                                main_onProgress(&ctx);
                            } else
                                lseek(dst, -((off_t)numberOfBytesVerify), SEEK_CUR);
                        }
                        if(needWrite) {
                            numberOfBytesWritten = (int)write(dst, ctx.buffer, numberOfBytesRead);
                            if(verbose > 1) printf("write(%d) numberOfBytesWritten %d errno=%d\n",
                                numberOfBytesRead, numberOfBytesWritten, errno);
                            if(numberOfBytesWritten == numberOfBytesRead) {
                                if(needVerify) {
                                    lseek(dst, -((off_t)numberOfBytesWritten), SEEK_CUR);
                                    numberOfBytesVerify = read(dst, ctx.verifyBuf, numberOfBytesWritten);
                                    if(verbose > 1) printf("  numberOfBytesVerify %d\n", numberOfBytesVerify);
                                    if(numberOfBytesVerify != numberOfBytesWritten ||
                                        memcmp(ctx.buffer, ctx.verifyBuf, numberOfBytesWritten)) {
                                        main_onError(lang[L_VRFYERR]);
                                        break;
                                    }
                                }
                                main_onProgress(&ctx);
                            } else {
                                if(errno) main_errorMessage = strerror(errno);
                                main_onError(lang[L_WRTRGERR]);
                                break;
                            }
                        }
                    }
                } else {
                    main_onError(lang[L_RDSRCERR]);
                    break;
                }
            }
            disks_close((void*)((long int)dst));
        } else {
            main_onError(lang[dst == -1 ? L_TRGERR : (dst == -2 ? L_UMOUNTERR : (dst == -4 ? L_COMMERR : L_OPENTRGERR))]);
        }
        stream_close(&ctx);
    } else {
        if(errno) main_errorMessage = strerror(errno);
        main_onError(lang[dst == 2 ? L_ENCZIPERR : (dst == 3 ? L_CMPZIPERR : (dst == 4 ? L_CMPERR : L_SRCERR))]);
    }
    stream_status(&ctx, status, 1);
    if(verbose) printf("Worker thread finished.\r\n");
    col = 0; mainRedraw();
    return NULL;
}

#if !defined(USE_WRONLY) || !USE_WRONLY

/**
 * Function that reads from disk and writes to output file
 */
static void *readerRoutine(void)
{
    int src, size, numberOfBytesRead;
    static stream_t ctx;
    char *env, fn[PATH_MAX];
    struct stat st;
    struct tm *lt;
    time_t now = time(NULL);
    int i;

    if(targetId >= 0 && targetId < DISKS_MAX && disks_targets[targetId] >= 1024) return NULL;

    ctx.readSize = 0;
    src = (int)((long int)disks_open(targetId, 0));
    if(src > 0) {
        fn[0] = 0;
        if(bkpdir && !stat(bkpdir, &st)) {
            strncpy(fn, bkpdir, sizeof(fn)-1);
        } else {
            if((env = getenv("HOME")))
                strncpy(fn, env, sizeof(fn)-1);
            else if((env = getenv("LOGNAME")))
                snprintf(fn, sizeof(fn)-1, "/home/%s", env);
            if(!fn[0]) strcpy(fn, ".");
            i = strlen(fn);
            strncpy(fn + i, "/Desktop", sizeof(fn)-1-i);
            if(stat(fn, &st)) {
                strncpy(fn + i, "/Downloads", sizeof(fn)-1-i);
                if(stat(fn, &st)) strcpy(fn, ".");
            }
        }
        i = strlen(fn);
        lt = localtime(&now);
        snprintf(fn + i, sizeof(fn)-1-i, "/usbimager-%04d%02d%02dT%02d%02d.dd%s",
            lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min,
            needCompress ? ".zst" : "");
        strcpy(source, fn);
        mainRedraw();
        if(!stream_create(&ctx, fn, needCompress, disks_capacity[targetId])) {
            while(ctx.readSize < ctx.fileSize) {
                errno = 0;
                size = ctx.fileSize - ctx.readSize < (uint64_t)buffer_size ? (int)(ctx.fileSize - ctx.readSize) : buffer_size;
                numberOfBytesRead = (int)read(src, ctx.buffer, size);
                if(verbose > 1) printf("read(%d) numberOfBytesRead %d errno=%d\n", size, numberOfBytesRead, errno);
                if(numberOfBytesRead == size) {
                    if(stream_write(&ctx, ctx.buffer, size)) {
                        main_onProgress(&ctx);
                    } else {
                        if(errno) main_errorMessage = strerror(errno);
                        main_onError(lang[L_WRIMGERR]);
                        break;
                    }
                } else {
                    if(errno) main_errorMessage = strerror(errno);
                    main_onError(lang[L_RDSRCERR]);
                    break;
                }
            }
            stream_close(&ctx);
            if(errno == ENOSPC) remove(fn);
        } else {
            if(errno) main_errorMessage = strerror(errno);
            main_onError(lang[L_OPENIMGERR]);
        }
        disks_close((void*)((long int)src));
    } else {
        main_onError(lang[src == -1 ? L_TRGERR : (src == -2 ? L_UMOUNTERR : (src == -4 ? L_COMMERR : L_OPENTRGERR))]);
    }
    stream_status(&ctx, status, 1);
    if(verbose) printf("Worker thread finished.\r\n");
    col = 0; mainRedraw();
    return NULL;
}

#endif

static void refreshTarget(void)
{
    memset(targetList, 0, sizeof(targetList));
    numTargetList = 0;
    disks_refreshlist();
    if(targetId >= numTargetList) targetId = numTargetList - 1;
}

void mainRedraw(void)
{
    int r = 0, c = 0, x, y, i, w, h, ty;
    char sel[16],ina[16],iab[16];
#if !defined(USE_WRONLY) || !USE_WRONLY
    char btntext[256];
#endif

    getstdindim(&r, &c);
    if(r != row || c != col) { row = r; col = c; bg(); }
#if !defined(USE_WRONLY) || !USE_WRONLY
    h = 12;
#else
    h = 10;
#endif
    w = col * 3 / 4; if(w < 42) w = 42;
    x = (col - w) / 2; if(x < 1) x = 1;
    y = (row - h) / 2; if(y < 1) y = 1;

    strcpy(sel,!menu?"37;44;1":"34;40;1");
    strcpy(ina,!menu?"0;37":"34;1");
    strcpy(iab,!menu?"30":"34");
    drawboxtop(x,y,w,0,!menu,ina);
    printf("\033[%s;%dm\033[%d;%dH%s%s%s \033[0;37;%dm",!menu?"0;30":ina,!menu?47:(t==terms[2]?46:100),y+1,x,t[PRE],t[HOR],t[SUF],
        (t==terms[2]?46:100));
    drawtext(source, w - 9);
    printf("\033[%s;%dm[...]\033[%s;%dm %s%s%s\033[40m  \033[%d;%dH\033[%s;%dm",
        mainsel==0?sel:(!menu?"0;30":"0;34"),mainsel==0?1:(!menu?47:(t==terms[2]?46:100)),
        !menu?"37;1":ina,!menu?47:(t==terms[2]?46:100),t[PRE],t[HOR],t[SUF],
        y+1,x+3, mainsel==0?sel:(!menu?"0;30":"0;34"),mainsel==0?1:(!menu?47:(t==terms[2]?46:100)));
    drawline(x,y+ 2,w,0,!menu,sel,ina);
#if !defined(USE_WRONLY) || !USE_WRONLY
    drawline(x,y+ 3,w,0,!menu,sel,ina);
    snprintf(btntext, sizeof(btntext)-1, "%s %s",t==terms[2]?"v":"▼", lang[L_WRITE]);
    i = mystrlen(btntext);
    printf("\033[%d;%dH\033[0;%d;%sm< \033[%sm%s\033[%sm >",y+3,x+w/3-i/2,!menu?47:(t==terms[2]?46:100),
        !menu && mainsel==1?sel:iab,!menu && mainsel==1?"33;44;1":iab, btntext, !menu && mainsel==1?sel:iab);
    snprintf(btntext, sizeof(btntext)-1, "%s %s",t==terms[2]?"^":"▲", lang[L_READ]);
    i = mystrlen(btntext);
    printf("\033[%d;%dH\033[0;%d;%sm< \033[%sm%s\033[%sm >",y+3,x+w*2/3-i/2,!menu?47:(t==terms[2]?46:100),
        !menu && mainsel==2?sel:iab,!menu && mainsel==2?"33;44;1":iab, btntext, !menu && mainsel==2?sel:iab);
    drawline(x,y+ 4,w,0,!menu,sel,ina);
    drawline(x,y+ 5,w,mainsel==3,!menu,sel,ina);
    printf("%s\033[%d;%dH[%s]", targetId >= 0 && targetId < numTargetList ? targetList[targetId] : "", y+5, x+w-5,t==terms[2]?"v":"▼");
    ty = y+5;
    drawline(x,y+ 6,w,0,!menu,sel,ina);
    drawline(x,y+ 7,w,0,!menu,sel,ina);
    printf("\033[0;%d;%sm[%s] %s", !menu?47:(t==terms[2]?46:100),!menu && mainsel==4?sel:iab,
        needVerify ? "X" : " ", lang[L_VERIFY]);
    printf("\033[%d;%dH\033[0;%d;%sm[%s] %s", y+7, x+w/2, !menu?47:(t==terms[2]?46:100),!menu && mainsel==5?sel:iab,
        needCompress ? "X" : " ", lang[L_COMPRESS]);
    if(blksizesel < 0 || blksizesel > 9) blksizesel = 0;
    printf("\033[%d;%dH\033[0;%d;%sm%s [%s]", y+7, x+w-10, !menu?47:(t==terms[2]?46:100),!menu && mainsel==6?sel:iab,
        blksizeList[blksizesel], t==terms[2]?"v":"▼");
#else
    drawline(x,y+ 3,w,mainsel==1,!menu,sel,ina);
    printf("%s\033[%d;%dH[%s]", targetId >= 0 && targetId < numTargetList ? targetList[targetId] : "", y+3, x+w-5,t==terms[2]?"v":"▼");
    ty = y+3;
    drawline(x,y+ 4,w,0,!menu,sel,ina);
    drawline(x,y+ 5,w,mainsel==2,!menu,sel,ina);
    i = mystrlen(lang[L_WRITE]);
    printf("\033[%d;%dH\033[0;%d;%sm< \033[%sm%s\033[%sm >",y+5,x+(w-i)/2,!menu?47:(t==terms[2]?46:100),
        !menu && mainsel==2?sel:iab,!menu && mainsel==2?"33;44;1":iab, lang[L_WRITE], !menu && mainsel==2?sel:iab);
#endif
    drawline(x,y+h-4,w,0,!menu,sel,ina);
    printf("\033[%s;%dm\033[%d;%dH%s%s%s ",!menu?"0;30":ina,!menu?47:(t==terms[2]?46:100),y+h-3,x,t[PRE],t[HOR],t[SUF]);
    statx = x + 2; staty = y + h - 3; statw = w - 4;
    printf("\033[0;37;%dm", (t==terms[2]?40:100));
    for(i = 0; i < w - 4; i++) printf(" ");
    printf("\033[%s;%dm %s%s%s\033[40m  ",!menu?"37;1":ina,!menu?47:(t==terms[2]?46:100),t[PRE],t[HOR],t[SUF]);
    drawline(x,y+h-2,w,0,!menu,sel,ina);
    drawboxbtm(x,y+h-1,w,!menu,ina);
    drawshd(x,y+h,w);

    /* popup menus */
    switch(menu) {
        case 1: drawfilesel(); break;
        case 2:
            if(targetId < 0 || targetId >= numTargetList) targetId = 0;
            drawselect(targetPtr, numTargetList, x+1, ty, w-2, targetId);
        break;
#if !defined(USE_WRONLY) || !USE_WRONLY
        case 3: drawselect(blksizeList, 10, x+w-10, y+7, 9, blksizesel); break;
#endif
    }
    printf("\033[0m\033[%d;%dH\033[?25l",row,col);
}

int main(int argc, char **argv)
{
    int i, j;
    char *lc = getenv("LANG"), c;
    char help[] = "USBImager " USBIMAGER_VERSION
#if USE_WRONLY
        "_wo"
#endif
#ifdef USBIMAGER_BUILD
        " (build " USBIMAGER_BUILD ")"
#endif
        " - MIT license, Copyright (C) 2020 bzt\r\n\r\n"
        "./usbimager [-v|-vv|-a|-f|-s[baud]|-S[baud]|-1|-2|-3|-4|-5|-6|-7|-8|-9|-L(xx)|-m(x)] <backup path>\r\n\r\n"
        "https://gitlab.com/bztsrc/usbimager\r\n\r\n";

    for(j = 1; j < argc && argv[j]; j++) {
        if(argv[j][0] == '-') {
            if(!strcmp(argv[j], "--version")) {
                printf(USBIMAGER_VERSION "\n");
                exit(0);
            }
            if(!strcmp(argv[j], "--help")) {
                printf("%s", help);
                exit(0);
            }
            for(i = 1; argv[j][i]; i++)
                switch(argv[j][i]) {
                    case 'f': force++; break;
                    case 'v':
                        verbose++;
                        if(verbose == 1) printf("%s", help);
                    break;
                    case 's':
                        disks_serial = 1;
                        if(argv[j][i+1] >= '0' && argv[j][i+1] <= '9') {
                            stream_baud(atoi(argv[j] + i + 1));
                            while(argv[j][i+1] >= '0' && argv[j][i+1] <= '9') i++;
                        }
                        break;
                    case 'S':
                        disks_serial = 2;
                        if(argv[j][i+1] >= '0' && argv[j][i+1] <= '9') {
                            stream_baud(atoi(argv[j] + i + 1));
                            while(argv[j][i+1] >= '0' && argv[j][i+1] <= '9') i++;
                        }
                        break;
                    case 'a': disks_all = 1; break;
                    case '1': blksizesel = 1; buffer_size = 2*1024*1024; break;
                    case '2': blksizesel = 2; buffer_size = 4*1024*1024; break;
                    case '3': blksizesel = 3; buffer_size = 8*1024*1024; break;
                    case '4': blksizesel = 4; buffer_size = 16*1024*1024; break;
                    case '5': blksizesel = 5; buffer_size = 32*1024*1024; break;
                    case '6': blksizesel = 6; buffer_size = 64*1024*1024; break;
                    case '7': blksizesel = 7; buffer_size = 128*1024*1024; break;
                    case '8': blksizesel = 8; buffer_size = 256*1024*1024; break;
                    case '9': blksizesel = 9; buffer_size = 512*1024*1024; break;
                    case 'L': lc = &argv[j][++i]; ++i; break;
                    case 'm': disks_maxsize = atoi(&argv[j][++i]); continue;
                }
        } else
            bkpdir = argv[j];
    }

    if(!lc) lc = "en";
    for(i = 0; i < NUMLANGS; i++) {
        if(!memcmp(lc, dict[i][0], strlen(dict[i][0]))) {
            lang = &dict[i][1];
            break;
        }
    }
    if(!lang) lang = &dict[0][1];

    if(verbose) {
        printf("LANG '%s', dict '%s', serial %d, buffer_size %d MiB, force %d\r\n",
            lc, lang[-1], disks_serial, buffer_size/1024/1024, force);
        printf("disks_maxsize %d GiB\r\n", disks_maxsize);
        if(disks_serial) printf("Serial %d,8,n,1\r\n", baud);
#if !defined(USE_WRONLY) || !USE_WRONLY
        if(bkpdir) printf("bkpdir '%s'\r\n", bkpdir);
#endif
    }

    t = terms[0];
    memset(source, 0, sizeof(source));
    memset(status, 0, sizeof(status));
    for(i = 0; i < DISKS_MAX; i++) targetPtr[i] = targetList[i];
    refreshTarget();
    memset(path, 0, sizeof(path));
    getcwd(path, sizeof(path)-1);
    setupstdin();
    do {
        mainRedraw();
        c = getcsi(); if(c >= '1' && c <= '3') { t = terms[c-'1']; col = 0; continue; }
        if(!menu) {
            switch(c) {
                case 1: case 4: if(mainsel > 0) mainsel--; else mainsel = NUMFLD; break;
                case 2: case 3: case 9: if(mainsel < NUMFLD) mainsel++; else mainsel = 0; break;
                case 10: case 32:
                    switch(mainsel) {
                        case 0: readdirectory(); menu = 1; break;
#if !defined(USE_WRONLY) || !USE_WRONLY
                        case 1: mainsel = 999; writerRoutine(); mainsel = 0; break;
                        case 2: mainsel = 999; readerRoutine(); mainsel = 0; break;
                        case 3: refreshTarget(); menu = 2; break;
                        case 4: needVerify ^= 1; break;
                        case 5: needCompress ^= 1; break;
                        case 6: menu = 3; break;
#else
                        case 1: refreshTarget(); menu = 2; break;
                        case 2: mainsel = 999; writerRoutine(); mainsel = 0; break;
#endif
                    }
                break;
            }
        } else
        if(c == 0x1b) {
            menu = 0; col = 0; c = 0; freefiles();
        } else
        switch(menu) {
            case 1: c = ctrlfilesel(c); break;
            case 2: c = ctrlselect(c, &targetId, numTargetList); break;
            case 3: c = ctrlselect(c, &blksizesel, 10); break;
/*
            case 1: c = ctrlselect(c, &selplat, numplatform); break;
            case 2: c = ctrlselect(c, &cmplr, sizeof(compilers)/sizeof(compilers[0])); break;
            case 3: c = ctrlchklist(c, compopts); break;
            case 4: c = ctrldiskimg(c); break;
            default:
                switch(opts[menu - 5].handler) {
                    case CHKLIST: c = ctrlchklist(c, opts[menu - 5].list); break;
                    case SELLYT: c = ctrlselect(c, &opts[menu - 5].value, ((kbd_t*)opts[2].list)[opts[2].value].num); break;
                    default: c = ctrlselect(c, &opts[menu - 5].value, opts[menu - 5].len); break;
                }
            break;
*/
        }
    } while(c!=0x1b);
    restorestdin();

    return 0;
}
