/*
 * usbimager/main_gtk.c
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
 * @brief Native GTK User interface for Linux
 *
 */

/* wether to start a separate thread for reader / writer */
/*#define USE_THREADS*/

#include <errno.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include "lang.h"
#include "stream.h"
#include "disks.h"

char **lang = NULL;
extern char *dict[NUMLANGS][NUMTEXTS + 1];

static char *bkpdir = NULL;
static GtkWidget *mainwin = NULL, *vbox, *hbox1, *source, *sourceButton, *writeButton, *target, *pbar, *status, *hboxhack;
#if !defined(USE_WRONLY) || !USE_WRONLY
GtkWidget *hbox2, *hbox3, *readButton, *verify, *compr, *blksize;
#endif
static int blksizesel = 0;
#ifdef USE_THREADS
GThread *thrd = NULL;
#endif
char *main_errorMessage;

void main_addToCombobox(char *option)
{
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(target), NULL, option);
}

void main_getErrorMessage()
{
    main_errorMessage = errno ? strerror(errno) : NULL;
}

static int onDone(void *data)
{
#if !defined(USE_WRONLY) || !USE_WRONLY
    int targetId;
#endif
    if(mainwin) {
        gtk_widget_set_sensitive(source, TRUE);
        gtk_widget_set_sensitive(sourceButton, TRUE);
        gtk_widget_set_sensitive(target, TRUE);
        gtk_widget_set_sensitive(writeButton, TRUE);
#if !defined(USE_WRONLY) || !USE_WRONLY
        targetId = gtk_combo_box_get_active(GTK_COMBO_BOX(target));
        if(targetId < 0 || targetId >= DISKS_MAX || disks_targets[targetId] < 1024)
            gtk_widget_set_sensitive(readButton, TRUE);
        gtk_widget_set_sensitive(verify, TRUE);
        gtk_widget_set_sensitive(compr, TRUE);
        gtk_widget_set_sensitive(blksize, TRUE);
#endif
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), 0);
        gtk_label_set_label(GTK_LABEL(status), (char*)data);
    }
    main_errorMessage = NULL;
    return 0;
}

void main_onDone(void *data)
{
    if(mainwin) {
#ifdef USE_THREADS
        g_idle_add(onDone, data);
#else
        onDone(data);
#endif
    }
}

static int onProgress(void *data)
{
    char textstat[128];
    int pos = 0;
    if(mainwin) {
        if(data)
            pos = stream_status((stream_t*)data, textstat, 0);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), (gdouble)pos/100.0);
        gtk_label_set_label(GTK_LABEL(status), !data ? lang[L_WAITING] : textstat);
    }
    return 0;
}

void main_onProgress(void *data)
{
    if(mainwin) {
#ifdef USE_THREADS
        g_idle_add(onProgress, data);
#else
        onProgress(data);
        while(gtk_events_pending()) gtk_main_iteration();
#endif
    }
}

static int onThreadError(void *data)
{
    GtkWidget *mbox = gtk_message_dialog_new(GTK_WINDOW(mainwin), 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", (char*)data);
    gtk_window_set_title(GTK_WINDOW(mbox), main_errorMessage && *main_errorMessage ? main_errorMessage : lang[L_ERROR]);
    gtk_window_set_default_size(GTK_WINDOW(mbox), 200, 64);
    gtk_dialog_run(GTK_DIALOG(mbox));
    gtk_widget_destroy(mbox);
    return 0;
}

void main_onThreadError(void *data)
{
#ifdef USE_THREADS
    g_idle_add(onThreadError, data);
#else
    onThreadError(data);
#endif
}


/**
 * Function that reads from input and writes to disk
 */
static void *writerRoutine(void *data)
{
    int dst, needVerify, numberOfBytesRead;
    int numberOfBytesWritten, numberOfBytesVerify, needWrite, targetId = gtk_combo_box_get_active(GTK_COMBO_BOX(target));
    static char lpStatus[128];
    static stream_t ctx;
    (void)data;
#if !defined(USE_WRONLY) || !USE_WRONLY
    needVerify = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(verify));
#else
    needVerify = 1;
#endif

    ctx.fileSize = 0;
    dst = stream_open(&ctx, (char*)gtk_entry_get_text(GTK_ENTRY(source)), targetId >= 0 && targetId < DISKS_MAX && disks_targets[targetId] >= 1024);
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
                                            onThreadError(lang[L_VRFYERR]);
                                        break;
                                    }
                                }
                                main_onProgress(&ctx);
                            } else {
                                if(errno) main_errorMessage = strerror(errno);
                                main_onThreadError(lang[L_WRTRGERR]);
                                break;
                            }
                        }
                    }
                } else {
                    main_onThreadError(lang[L_RDSRCERR]);
                    break;
                }
            }
            disks_close((void*)((long int)dst));
        } else {
            main_onThreadError(lang[dst == -1 ? L_TRGERR : (dst == -2 ? L_UMOUNTERR : (dst == -4 ? L_COMMERR : L_OPENTRGERR))]);
        }
        stream_close(&ctx);
    } else {
        if(errno) main_errorMessage = strerror(errno);
        main_onThreadError(lang[dst == 2 ? L_ENCZIPERR : (dst == 3 ? L_CMPZIPERR : (dst == 4 ? L_CMPERR : L_SRCERR))]);
    }
    stream_status(&ctx, lpStatus, 1);
    main_onDone(&lpStatus);
    if(verbose) printf("Worker thread finished.\r\n");
    return NULL;
}

static void onWriteButtonClicked(GtkButton *btn, gpointer data)
{
    (void)btn;
    (void)data;
    gtk_widget_set_sensitive(source, FALSE);
    gtk_widget_set_sensitive(sourceButton, FALSE);
    gtk_widget_set_sensitive(target, FALSE);
    gtk_widget_set_sensitive(writeButton, FALSE);
#if !defined(USE_WRONLY) || !USE_WRONLY
    gtk_widget_set_sensitive(readButton, FALSE);
    gtk_widget_set_sensitive(verify, FALSE);
    gtk_widget_set_sensitive(compr, FALSE);
    gtk_widget_set_sensitive(blksize, FALSE);
#endif
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), 0);
    gtk_label_set_label(GTK_LABEL(status), "");
    main_errorMessage = NULL;

    if(verbose) printf("Starting worker thread for writing.\r\n");
#ifdef USE_THREADS
    thrd = g_thread_new("writer", writerRoutine, NULL);
#else
    writerRoutine(NULL);
#endif
}

#if !defined(USE_WRONLY) || !USE_WRONLY

/**
 * Function that reads from disk and writes to output file
 */
static void *readerRoutine(void *data)
{
    int src, size, needCompress = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(compr)), numberOfBytesRead;
    static char lpStatus[128];
    static stream_t ctx;
    char *env, fn[PATH_MAX];
    struct stat st;
    struct tm *lt;
    time_t now = time(NULL);
    int i, targetId = gtk_combo_box_get_active(GTK_COMBO_BOX(target));
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
        gtk_entry_set_text(GTK_ENTRY(source), fn);

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
                        main_onThreadError(lang[L_WRIMGERR]);
                        break;
                    }
                } else {
                    if(errno) main_errorMessage = strerror(errno);
                    main_onThreadError(lang[L_RDSRCERR]);
                    break;
                }
            }
            stream_close(&ctx);
            if(errno == ENOSPC) remove(fn);
        } else {
            if(errno) main_errorMessage = strerror(errno);
            main_onThreadError(lang[L_OPENIMGERR]);
        }
        disks_close((void*)((long int)src));
    } else {
        main_onThreadError(lang[src == -1 ? L_TRGERR : (src == -2 ? L_UMOUNTERR : (src == -4 ? L_COMMERR : L_OPENTRGERR))]);
    }
    stream_status(&ctx, lpStatus, 1);
    main_onDone(&lpStatus);
    if(verbose) printf("Worker thread finished.\r\n");
    return NULL;
}

static void onReadButtonClicked(GtkButton *btn, gpointer data)
{
    (void)btn;
    (void)data;

    gtk_widget_set_sensitive(source, FALSE);
    gtk_widget_set_sensitive(sourceButton, FALSE);
    gtk_widget_set_sensitive(target, FALSE);
    gtk_widget_set_sensitive(writeButton, FALSE);
    gtk_widget_set_sensitive(readButton, FALSE);
    gtk_widget_set_sensitive(verify, FALSE);
    gtk_widget_set_sensitive(compr, FALSE);
    gtk_widget_set_sensitive(blksize, FALSE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), 0);
    gtk_label_set_label(GTK_LABEL(status), "");
    main_errorMessage = NULL;
    if(verbose) printf("Starting worker thread for reading.\r\n");
#ifdef USE_THREADS
    thrd = g_thread_new("reader", readerRoutine, NULL);
#else
    readerRoutine(NULL);
#endif
}

static void refreshBlkSize(GtkWidget *w, gpointer data)
{
    int current = gtk_combo_box_get_active(GTK_COMBO_BOX(blksize));
    (void)w;
    (void)data;
    buffer_size = (1UL << current) * 1024UL * 1024UL;
}
#endif

static void refreshTarget(GtkWidget *w, gpointer data)
{
    int current = gtk_combo_box_get_active(GTK_COMBO_BOX(target));
    char btntext[256];
    GdkEvent *evt = gtk_get_current_event();
    (void)w;
    (void)data;
/*
 * fuck me... these are always return FALSE... how am I supposed to query if the popup is actually to be shown and not closed????
 * only grab-notify signal works, but that's sent on open and close too, without telling which one it is...
 *
printf("refreshTarget evt '%x' focus %d vis %d grab %d default %d state %d flags %d\n", evt ? evt->type : 0,
  gtk_widget_has_focus(target), gtk_widget_has_visible_focus(target), gtk_widget_has_grab(target), gtk_widget_has_default(target),
  gtk_widget_get_state(target), gtk_widget_get_state_flags(target));
*/
    if(evt && evt->type != 4) return;
    gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(target))));
    disks_refreshlist();
    /* we should resize the popup now, but neither of these work... */
    gtk_widget_compute_expand(target, GTK_ORIENTATION_VERTICAL);
    gtk_widget_queue_resize(target);
    gtk_widget_queue_allocate(target);
    gtk_widget_queue_draw(target);
    gtk_widget_show_all(GTK_WIDGET(mainwin));
    while(gtk_events_pending()) gtk_main_iteration();
    if(current < 0 || current >= gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtk_combo_box_get_model(GTK_COMBO_BOX(target))), NULL))
        current = 0;
    gtk_combo_box_set_active(GTK_COMBO_BOX(target), current);
    if(current >= 0 && current < DISKS_MAX && disks_targets[current] >= 1024) {
#if !defined(USE_WRONLY) || !USE_WRONLY
        snprintf(btntext, sizeof(btntext)-1, "▼ %s", lang[L_SEND]);
        gtk_widget_set_sensitive(readButton, FALSE);
#else
        snprintf(btntext, sizeof(btntext)-1, "%s", lang[L_SEND]);
#endif
    } else {
#if !defined(USE_WRONLY) || !USE_WRONLY
        snprintf(btntext, sizeof(btntext)-1, "▼ %s", lang[L_WRITE]);
        gtk_widget_set_sensitive(readButton, TRUE);
#else
        snprintf(btntext, sizeof(btntext)-1, "%s", lang[L_WRITE]);
#endif
    }
    gtk_button_set_label(GTK_BUTTON(writeButton), btntext);
}

static void onSelectClicked(GtkButton *btn, gpointer data)
{
    char *fn;
    GtkWidget *chooser = gtk_file_chooser_dialog_new("Open", NULL, GTK_FILE_CHOOSER_ACTION_OPEN, "gtk-cancel",
        GTK_RESPONSE_CANCEL, "gtk-open", GTK_RESPONSE_ACCEPT, NULL);
    (void)btn;
    (void)data;
    if(gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        if(fn) {
            gtk_entry_set_text(GTK_ENTRY(source), fn);
            free(fn);
        }
    }
    gtk_widget_destroy(chooser);
}

static void onClosing(GtkWidget *w, gpointer data)
{
    (void)w;
    (void)data;
#ifdef USE_THREADS
    if(thrd) { g_thread_unref(thrd); thrd = NULL; }
#endif
    gtk_widget_destroy(mainwin);
    gtk_main_quit();
    mainwin = NULL;
}

int main(int argc, char **argv)
{
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
        printf("disks_maxsize %d GiB\r\n", disks_maxsize);
        if(disks_serial) printf("Serial %d,8,n,1\r\n", baud);
#if !defined(USE_WRONLY) || !USE_WRONLY
        if(bkpdir) printf("bkpdir '%s'\r\n", bkpdir);
#endif
    }

    gtk_init(&argc, &argv);

    mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mainwin), "USBImager " USBIMAGER_VERSION);
    gtk_window_set_default_size(GTK_WINDOW(mainwin), 480, 160);
    g_signal_connect(mainwin, "destroy", G_CALLBACK(onClosing), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(vbox, 5);
    gtk_widget_set_margin_start(vbox, 5);
    gtk_widget_set_margin_bottom(vbox, 5);
    gtk_widget_set_margin_end(vbox, 5);
    gtk_container_add(GTK_CONTAINER(mainwin), vbox);

    hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, TRUE, 0);

    source = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox1), source, TRUE, TRUE, 0);

    sourceButton = gtk_button_new_with_label("...");
    g_signal_connect(sourceButton, "clicked", G_CALLBACK(onSelectClicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox1), sourceButton, FALSE, TRUE, 0);

    hboxhack = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

#if !defined(USE_WRONLY) || !USE_WRONLY
    hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, TRUE, 0);

    snprintf(btntext, sizeof(btntext)-1, "▼ %s", lang[L_WRITE]);
    writeButton = gtk_button_new_with_label(btntext);
    g_signal_connect(writeButton, "clicked", G_CALLBACK(onWriteButtonClicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), writeButton, TRUE, TRUE, 5);

    snprintf(btntext, sizeof(btntext)-1, "▲ %s", lang[L_READ]);
    readButton = gtk_button_new_with_label(btntext);
    g_signal_connect(readButton, "clicked", G_CALLBACK(onReadButtonClicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), readButton, TRUE, TRUE, 5);

    target = gtk_combo_box_text_new();
    /* according to the doc, this should be it, but it crashes the app with
(usbimager:1668493): GLib-GObject-WARNING **: 10:08:55.855: ../glib/gobject/gsignal.c:2620: signal 'popup-shown' is invalid for instance '0x55ad9483e220' of type 'GtkComboBoxText'
Segmentation fault (core dumped)
    */
    /* g_signal_connect(target, "popup-shown", G_CALLBACK(refreshTarget), NULL); */
/*
    gtk_widget_set_can_focus(target, 1);
    gtk_widget_set_focus_on_click(target, 1);
    g_signal_connect(target, "button-press-event", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "key-press-event", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "enter-notify-event", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "focus-in-event", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "focus", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "popdown", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "popup", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "popup-menu", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "popup-shown", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "state-changed", G_CALLBACK(refreshTarget), NULL);
    g_signal_connect(target, "grab-focus", G_CALLBACK(refreshTarget), NULL);
*/
    g_signal_connect(target, "grab-notify", G_CALLBACK(refreshTarget), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), hboxhack, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hboxhack), target, TRUE, TRUE, 2);

    hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox3, FALSE, TRUE, 0);

    verify = gtk_check_button_new_with_label(lang[L_VERIFY]);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verify), 1);
    gtk_box_pack_start(GTK_BOX(hbox3), verify, TRUE, TRUE, 5);

    compr = gtk_check_button_new_with_label(lang[L_COMPRESS]);
    gtk_box_pack_start(GTK_BOX(hbox3), compr, TRUE, TRUE, 5);

    blksize = gtk_combo_box_text_new();
    g_signal_connect(target, "changed", G_CALLBACK(refreshBlkSize), NULL);
    gtk_box_pack_start(GTK_BOX(hbox3), blksize, FALSE, TRUE, 5);
    for(i = 0; i < 10; i++) {
        sprintf(btntext, "%3dM", (1<<i));
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(blksize), NULL, btntext);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(blksize), blksizesel);

#else
    target = gtk_combo_box_text_new();
    g_signal_connect(target, "grab-notify", G_CALLBACK(refreshTarget), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), hboxhack, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hboxhack), target, TRUE, TRUE, 2);

    snprintf(btntext, sizeof(btntext)-1, "%s", lang[L_WRITE]);
    writeButton = gtk_button_new_with_label(btntext);
    g_signal_connect(writeButton, "clicked", G_CALLBACK(onWriteButtonClicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), writeButton, FALSE, TRUE, 5);

#endif
    pbar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), pbar, TRUE, TRUE, 0);

    status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(status), 0);
    gtk_box_pack_start(GTK_BOX(vbox), status, FALSE, FALSE, 2);

    refreshTarget(target, NULL);

    gtk_widget_show_all(GTK_WIDGET(mainwin));
    gtk_main();
#ifdef USE_THREADS
    if(thrd) { g_thread_unref(thrd); thrd = NULL; }
#endif
    return 0;
}
