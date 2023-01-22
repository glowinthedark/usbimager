/*
 * usbimager/main_libui.c
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
 * @brief User interface for Linux and MacOSX using libui
 *
 */

#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include "lang.h"
#include "stream.h"
#include "disks.h"
#include "libui/ui.h"

char **lang = NULL;
extern char *dict[NUMLANGS][NUMTEXTS + 1];

static char *bkpdir = NULL;
static uiWindow *mainwin = NULL;
static uiButton *sourceButton;
static uiEntry *source;
static uiBox *targetCont;
static uiCombobox *target;
#if !defined(USE_WRONLY) || !USE_WRONLY
static uiCheckbox *verify;
static uiCheckbox *compr;
static uiCombobox *blksize;
static uiButton *readButton;
#endif
static uiButton *writeButton;
static uiProgressBar *pbar;
static uiLabel *status;
static int blksizesel = 0;
pthread_t thrd;
pthread_attr_t tha;
char *main_errorMessage;

void main_addToCombobox(char *option)
{
    uiComboboxAppend(target, option);
}

void main_getErrorMessage()
{
    main_errorMessage = errno ? strerror(errno) : NULL;
}

static void onDone(void *data)
{
#if !defined(USE_WRONLY) || !USE_WRONLY
    int targetId;
#endif
    if(mainwin) {
        uiControlEnable(uiControl(source));
        uiControlEnable(uiControl(sourceButton));
        uiControlEnable(uiControl(target));
        uiControlEnable(uiControl(writeButton));
#if !defined(USE_WRONLY) || !USE_WRONLY
        targetId = uiComboboxSelected(target);
        if(targetId < 0 || targetId >= DISKS_MAX || disks_targets[targetId] < 1024)
            uiControlEnable(uiControl(readButton));
        uiControlEnable(uiControl(verify));
        uiControlEnable(uiControl(compr));
        uiControlEnable(uiControl(blksize));
#endif
        uiProgressBarSetValue(pbar, 0);
        uiLabelSetText(status, (char*)data);
    }
    main_errorMessage = NULL;
}

void main_onDone(void *data)
{
    if(mainwin)
        uiQueueMain(onDone, data);
}

static void onProgress(void *data)
{
    char textstat[128];
    int pos = 0;
    if(mainwin) {
        if(data)
            pos = stream_status((stream_t*)data, textstat, 0);
        uiProgressBarSetValue(pbar, pos);
        uiLabelSetText(status, !data ? lang[L_WAITING] : textstat);
    }
}

void main_onProgress(void *data)
{
    if(mainwin)
        uiQueueMain(onProgress, data);
}

static void onThreadError(void *data)
{
    uiMsgBoxError(mainwin, main_errorMessage && *main_errorMessage ? main_errorMessage : lang[L_ERROR], (char*)data);
}

/**
 * Function that reads from input and writes to disk
 */
static void *writerRoutine(void *data)
{
    int dst, needVerify, numberOfBytesRead;
    int numberOfBytesWritten, numberOfBytesVerify, needWrite, targetId = uiComboboxSelected(target);
    static char lpStatus[128];
    static stream_t ctx;
    (void)data;
#if !defined(USE_WRONLY) || !USE_WRONLY
    needVerify = uiCheckboxChecked(verify);
#else
    needVerify = 1;
#endif

    ctx.fileSize = 0;
    dst = stream_open(&ctx, uiEntryText(source), targetId >= 0 && targetId < DISKS_MAX && disks_targets[targetId] >= 1024);
    if(!dst) {
        dst = (int)((long int)disks_open(targetId, ctx.fileSize));
        if(dst > 0) {
            while(mainwin) {
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
                                            uiQueueMain(onThreadError, lang[L_VRFYERR]);
                                        break;
                                    }
                                }
                                main_onProgress(&ctx);
                            } else {
                                if(errno) main_errorMessage = strerror(errno);
                                uiQueueMain(onThreadError, lang[L_WRTRGERR]);
                                break;
                            }
                        }
                    }
                } else {
                    uiQueueMain(onThreadError, lang[L_RDSRCERR]);
                    break;
                }
            }
            disks_close((void*)((long int)dst));
        } else {
            uiQueueMain(onThreadError, lang[dst == -1 ? L_TRGERR : (dst == -2 ? L_UMOUNTERR : (dst == -4 ? L_COMMERR : L_OPENTRGERR))]);
        }
        stream_close(&ctx);
    } else {
        if(errno) main_errorMessage = strerror(errno);
        uiQueueMain(onThreadError, lang[dst == 2 ? L_ENCZIPERR : (dst == 3 ? L_CMPZIPERR : (dst == 4 ? L_CMPERR : L_SRCERR))]);
    }
    stream_status(&ctx, lpStatus, 1);
    main_onDone(&lpStatus);
    if(verbose) printf("Worker thread finished.\r\n");
    return NULL;
}

static void onWriteButtonClicked(uiButton *b, void *data)
{
    (void)b;
    (void)data;
    uiControlDisable(uiControl(source));
    uiControlDisable(uiControl(sourceButton));
    uiControlDisable(uiControl(target));
    uiControlDisable(uiControl(writeButton));
#if !defined(USE_WRONLY) || !USE_WRONLY
    uiControlDisable(uiControl(readButton));
    uiControlDisable(uiControl(verify));
    uiControlDisable(uiControl(compr));
    uiControlDisable(uiControl(blksize));
#endif
    uiProgressBarSetValue(pbar, 0);
    uiLabelSetText(status, "");
    main_errorMessage = NULL;

    if(verbose) printf("Starting worker thread for writing.\r\n");
    pthread_create(&thrd, &tha, writerRoutine, NULL);
}

#if !defined(USE_WRONLY) || !USE_WRONLY
static void onSourceSet(void *data)
{
    uiControlEnable(uiControl(source));
    uiEntrySetText(source, data);
    uiControlDisable(uiControl(source));
}

/**
 * Function that reads from disk and writes to output file
 */
static void *readerRoutine(void *data)
{
    int src, size, needCompress = uiCheckboxChecked(compr), numberOfBytesRead;
    static char lpStatus[128];
    static stream_t ctx;
    char *env, fn[PATH_MAX];
    struct stat st;
    struct tm *lt;
    time_t now = time(NULL);
    int i, targetId = uiComboboxSelected(target);
    (void)data;

    if(targetId >= 0 && targetId < DISKS_MAX && disks_targets[targetId] >= 1024) return NULL;

    ctx.fileSize = 0;
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
        uiQueueMain(onSourceSet, fn);

        if(!stream_create(&ctx, fn, needCompress, disks_capacity[targetId])) {
            while(mainwin && ctx.readSize < ctx.fileSize) {
                errno = 0;
                size = ctx.fileSize - ctx.readSize < (uint64_t)buffer_size ? (int)(ctx.fileSize - ctx.readSize) : buffer_size;
                numberOfBytesRead = (int)read(src, ctx.buffer, size);
                if(verbose > 1) printf("read(%d) numberOfBytesRead %d errno=%d\n", size, numberOfBytesRead, errno);
                if(numberOfBytesRead == size) {
                    if(stream_write(&ctx, ctx.buffer, size)) {
                        main_onProgress(&ctx);
                    } else {
                        if(errno) main_errorMessage = strerror(errno);
                        uiQueueMain(onThreadError, lang[L_WRIMGERR]);
                        break;
                    }
                } else {
                    if(errno) main_errorMessage = strerror(errno);
                    uiQueueMain(onThreadError, lang[L_RDSRCERR]);
                    break;
                }
            }
            stream_close(&ctx);
            if(errno == ENOSPC) remove(fn);
        } else {
            if(errno) main_errorMessage = strerror(errno);
            uiQueueMain(onThreadError, lang[L_OPENIMGERR]);
        }
        disks_close((void*)((long int)src));
    } else {
        uiQueueMain(onThreadError, lang[src == -1 ? L_TRGERR : (src == -2 ? L_UMOUNTERR : (src == -4 ? L_COMMERR : L_OPENTRGERR))]);
    }
    stream_status(&ctx, lpStatus, 1);
    main_onDone(&lpStatus);
    if(verbose) printf("Worker thread finished.\r\n");
    return NULL;
}

static void onReadButtonClicked(uiButton *b, void *data)
{
    (void)b;
    (void)data;

    uiControlDisable(uiControl(source));
    uiControlDisable(uiControl(sourceButton));
    uiControlDisable(uiControl(target));
    uiControlDisable(uiControl(writeButton));
    uiControlDisable(uiControl(readButton));
    uiControlDisable(uiControl(verify));
    uiControlDisable(uiControl(compr));
    uiControlDisable(uiControl(blksize));
    uiProgressBarSetValue(pbar, 0);
    uiLabelSetText(status, "");
    main_errorMessage = NULL;
    if(verbose) printf("Starting worker thread for reading.\r\n");
    pthread_create(&thrd, &tha, readerRoutine, NULL);
}

static void refreshBlkSize(uiCombobox *cb, void *data)
{
    int current = uiComboboxSelected(blksize);
    (void)cb;
    (void)data;
    buffer_size = (1UL << current) * 1024UL * 1024UL;
}
#endif

static void refreshTarget(uiCombobox *cb, void *data)
{
    int current = uiComboboxSelected(target);
    char btntext[256];
    (void)cb;
    (void)data;
    uiBoxDelete(targetCont, 0);
    target = uiNewCombobox();
    uiBoxAppend(targetCont, uiControl(target), 1);
    uiComboboxOnSelected(target, refreshTarget, NULL);
    disks_refreshlist();
    uiComboboxSetSelected(target, current);
    if(current >= 0 && current < DISKS_MAX && disks_targets[current] >= 1024) {
#if !defined(USE_WRONLY) || !USE_WRONLY
        snprintf(btntext, sizeof(btntext)-1, "▼ %s", lang[L_SEND]);
        uiControlDisable(uiControl(readButton));
#else
        snprintf(btntext, sizeof(btntext)-1, "%s", lang[L_SEND]);
#endif
    } else {
#if !defined(USE_WRONLY) || !USE_WRONLY
        snprintf(btntext, sizeof(btntext)-1, "▼ %s", lang[L_WRITE]);
        uiControlEnable(uiControl(readButton));
#else
        snprintf(btntext, sizeof(btntext)-1, "%s", lang[L_WRITE]);
#endif
    }
    uiButtonSetText(writeButton, btntext);
}

static void onSelectClicked(uiButton *b, void *data)
{
    uiEntry *entry = uiEntry(data);
    char *filename;

    (void)b;
    refreshTarget(target, NULL);
    uiLabelSetText(status, "");
    filename = uiOpenFile(mainwin);
    if (filename == NULL) return;
    uiEntrySetText(entry, filename);
    uiFreeText(filename);
}

static int onClosing(uiWindow *w, void *data)
{
    (void)w;
    (void)data;
    if(thrd) { pthread_cancel(thrd); thrd = 0; }
    uiQuit();
    mainwin = NULL;
    return 1;
}

static int onShouldQuit(void *data)
{
    if(thrd) { pthread_cancel(thrd); thrd = 0; }
    uiControlDestroy(uiControl(uiWindow(data)));
    mainwin = NULL;
    return 1;
}

int main(int argc, char **argv)
{
    uiInitOptions options;
    const char *err;
    uiGrid *grid;
    uiBox *vbox;
    uiBox *bbox;
#if !defined(USE_WRONLY) || !USE_WRONLY
    uiLabel *sep;
#endif
    int i, j;
    char *lc = getenv("LANG"), btntext[256];
    char help[] = "USBImager " USBIMAGER_VERSION
#if USE_WRONLY
        "_wo"
#endif
#if USE_UDISKS2
        "_udisks2"
#endif
#ifdef USBIMAGER_BUILD
        " (build " USBIMAGER_BUILD ")"
#endif
        " - MIT license, Copyright (C) 2020 bzt\r\n\r\n"
        "./usbimager [-v|-vv|-a|-f|-s[baud]|-S[baud]|-1|-2|-3|-4|-5|-6|-7|-8|-9|-L(xx)] <backup path>\r\n\r\n"
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
                }
        } else
            bkpdir = argv[j];
    }

#if MACOSX
    if(!lc) lc = disks_getlang();
#endif
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
        if(disks_serial) printf("Serial %d,8,n,1\r\n", baud);
#if !defined(USE_WRONLY) || !USE_WRONLY
        if(bkpdir) printf("bkpdir '%s'\r\n", bkpdir);
#endif
    }

    pthread_attr_init(&tha);
    memset(&thrd, 0, sizeof(pthread_t));

    memset(&options, 0, sizeof (uiInitOptions));
    err = uiInit(&options);
    if (err != NULL) {
        fprintf(stderr, "error initializing libui: %s", err);
        uiFreeInitError(err);
        return 1;
    }

    mainwin = uiNewWindow("USBImager " USBIMAGER_VERSION, 480, 160, 1);
    uiWindowOnClosing(mainwin, onClosing, NULL);
    uiOnShouldQuit(onShouldQuit, mainwin);
    uiWindowSetMargined(mainwin, 1);

    grid = uiNewGrid();
    uiGridSetPadded(grid, 1);
    uiWindowSetChild(mainwin, uiControl(grid));

    sourceButton = uiNewButton("...");
    source = uiNewEntry();
    uiButtonOnClicked(sourceButton, onSelectClicked, source);
    uiGridAppend(grid, uiControl(source), 0, 0, 7, 1, 1, uiAlignFill, 0, uiAlignFill);
    uiGridAppend(grid, uiControl(sourceButton), 7, 0, 1, 1, 0, uiAlignFill, 0, uiAlignFill);

#if !defined(USE_WRONLY) || !USE_WRONLY
    bbox = uiNewHorizontalBox();

    uiGridAppend(grid, uiControl(bbox), 0, 1, 8, 1, 0, uiAlignFill, 0, uiAlignFill);
    snprintf(btntext, sizeof(btntext)-1, "▼ %s", lang[L_WRITE]);
    writeButton = uiNewButton(btntext);
    uiButtonOnClicked(writeButton, onWriteButtonClicked, NULL);
    uiBoxAppend(bbox, uiControl(writeButton), 1);

    sep = uiNewLabel(" ");
    uiBoxAppend(bbox, uiControl(sep), 0);

    snprintf(btntext, sizeof(btntext)-1, "▲ %s", lang[L_READ]);
    readButton = uiNewButton(btntext);
    uiButtonOnClicked(readButton, onReadButtonClicked, NULL);
    uiBoxAppend(bbox, uiControl(readButton), 1);

    targetCont = uiNewHorizontalBox();
    uiGridAppend(grid, uiControl(targetCont), 0, 2, 8, 1, 0, uiAlignFill, 0, uiAlignFill);
    target = uiNewCombobox();
    uiBoxAppend(targetCont, uiControl(target), 1);
    uiComboboxOnSelected(target, refreshTarget, NULL);

    verify = uiNewCheckbox(lang[L_VERIFY]);
    uiCheckboxSetChecked(verify, 1);
    uiGridAppend(grid, uiControl(verify), 0, 3, 3, 1, 0, uiAlignFill, 0, uiAlignFill);

    compr = uiNewCheckbox(lang[L_COMPRESS]);
    uiGridAppend(grid, uiControl(compr), 3, 3, 4, 1, 0, uiAlignFill, 0, uiAlignFill);

    blksize = uiNewCombobox();
    uiGridAppend(grid, uiControl(blksize), 7, 3, 1, 1, 0, uiAlignFill, 0, uiAlignFill);
    for(i = 0; i < 10; i++) {
        sprintf(btntext, "%3dM", (1<<i));
        uiComboboxAppend(blksize, btntext);
    }
    uiComboboxSetSelected(blksize, blksizesel);
    uiComboboxOnSelected(blksize, refreshBlkSize, NULL);

    pbar = uiNewProgressBar();
    uiGridAppend(grid, uiControl(pbar), 0, 4, 8, 1, 0, uiAlignFill, 4, uiAlignFill);

    vbox = uiNewVerticalBox();
    uiGridAppend(grid, uiControl(vbox), 0, 5, 8, 1, 0, uiAlignFill, 0, uiAlignFill);
#else
    targetCont = uiNewHorizontalBox();
    uiGridAppend(grid, uiControl(targetCont), 0, 1, 8, 1, 0, uiAlignFill, 0, uiAlignFill);
    target = uiNewCombobox();
    uiBoxAppend(targetCont, uiControl(target), 1);
    uiComboboxOnSelected(target, refreshTarget, NULL);

    bbox = uiNewHorizontalBox();
    uiGridAppend(grid, uiControl(bbox), 0, 2, 8, 1, 0, uiAlignFill, 0, uiAlignFill);
    snprintf(btntext, sizeof(btntext)-1, "%s", lang[L_WRITE]);
    writeButton = uiNewButton(btntext);
    uiButtonOnClicked(writeButton, onWriteButtonClicked, NULL);
    uiBoxAppend(bbox, uiControl(writeButton), 1);

    pbar = uiNewProgressBar();
    uiGridAppend(grid, uiControl(pbar), 0, 3, 8, 1, 0, uiAlignFill, 4, uiAlignFill);

    vbox = uiNewVerticalBox();
    uiGridAppend(grid, uiControl(vbox), 0, 4, 8, 1, 0, uiAlignFill, 0, uiAlignFill);
#endif
    refreshTarget(target, NULL);

    status = uiNewLabel("");
    uiBoxAppend(vbox, uiControl(status), 0);

    uiControlShow(uiControl(mainwin));
    uiMain();
    if(thrd) { pthread_cancel(thrd); thrd = 0; }
    pthread_attr_destroy(&tha);
    uiQuit();
    return 0;
}
