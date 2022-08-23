/*
 * usbimager/disks_linux.c
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
 * @brief Disk iteration and umount for Linux
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "lang.h"
#include "main.h"
#include "disks.h"

#if USE_UDISKS2
#include <udisks/udisks.h>
#include <gio/gunixfdlist.h>
extern char *main_errorMessage;
#endif

extern int fdatasync(int);
int usleep(unsigned long int);

/* disks_targets:
 * -1: invalid
 * 0 - 31: mmcblk devices
 * 'a' - 'z': sdX devices
 * 1024+: serial devices
 */
int disks_all = 0, disks_serial = 0, disks_targets[DISKS_MAX];
uint64_t disks_capacity[DISKS_MAX];
char *serials[DISKS_MAX], *skip[DISKS_MAX];
int serialdrivers = 0;

/* helper to read a string from a file */
void filegetcontent(char *fn, char *buf, int maxlen)
{
    FILE *f;
    char *s, *o, tmp[1024];

    memset(buf, 0, maxlen);
    f = fopen(fn, "r");
    if(f) {
        memset(tmp, 0, sizeof(tmp));
        if(!fread(tmp, maxlen - 1, 1, f)) {}
        fclose(f);
        for(s = tmp; *s && *s <= ' '; s++);
        for(o = buf; *s && *s != '\n';) *o++ = *s++;
        while(o > buf && *(o-1) <= ' ') o--;
        *o = 0;
        if(verbose > 1) {
            printf(" %s: ", fn);
            for(s = tmp; s < tmp + maxlen && *s; s++)
                printf("%02x ", *s);
            printf("trim() = '%s'\n", buf);
        }
    }
}

/**
 * Refresh target device list in the combobox
 */
void disks_refreshlist()
{
    DIR *dir;
    struct dirent *de;
    char str[1024], vendorName[128], productName[128], path[512], *skip[8];
    uint64_t size;
    int i = 0, j = 0, k, sizeInGbTimes10;
    char *unit, *c, *p, *d;
    FILE *f;

    memset(disks_targets, 0xff, sizeof(disks_targets));
    memset(disks_capacity, 0, sizeof(disks_capacity));
#if DISKS_TEST
    disks_targets[i++] = 'T';
    main_addToCombobox("sdT ./test.bin");
#endif
    /* get list of system disks */
    f = fopen("/proc/self/mountinfo", "r");
    if(f) {
        if(verbose > 1)
            printf("detected sysdisks:");
        while(!feof(f)) {
            if(fgets(str, sizeof(str), f)) {
                for(k = 0, c = str, p = d = NULL; k < 11 && *c && *c != '\n';) {
                    if(k == 5 || k == 10) *c++ = 0;
                    while(*c == ' ' || *c == '\t') c++;
                    if(k == 4) p = c;
                    if(k == 9) d = c;
                    k++;
                    while(*c && *c != ' ' && *c != '\t' && *c != '\n') c++;
                }
                if(p && d && !memcmp(d, "/dev/", 5) && (!strcmp(p, "/") || !memcmp(p, "/boot", 5)
                    || !memcmp(p, "/home", 5) || !memcmp(p, "/var", 4) || !memcmp(p, "/usr", 4) || !memcmp(p, "/srv", 4))) {
                        if(d[5] == 'm')
                            for(c = d + 5; *c && *c != 'p'; c++);
                        else
                            for(c = d + 5; *c && (*c < '0' || *c > '9'); c++);
                        *c = 0;
                        skip[j] = malloc(c - d - 5 + 1);
                        if(skip[j]) {
                            memcpy(skip[j], d + 5, c - d - 5 + 1);
                            if(verbose > 1)
                                printf(" %s", skip[j]);
                            j++;
                            if(j >= 8) break;
                        } else break;
                }
            }
        }
        fclose(f);
        if(verbose > 1)
            printf("\n");
    } else
    if(verbose > 1)
        printf("unable to get mount points???\n");

    dir = opendir("/sys/block");
    if(dir) {
        while((de = readdir(dir))) {
            if(verbose > 1) printf("%s: ", de->d_name);
            if((de->d_name[0] != 's' || de->d_name[1] != 'd') &&
                (de->d_name[0] != 'm' || de->d_name[1] != 'm')) {
                    if(verbose > 1) printf("SKIP\n");
                    continue;
            }
            if(!disks_all) {
                for(k = 0; k < j && strcmp(de->d_name, skip[k]); k++);
                if(k != j) {
                    if(verbose > 1) printf("SKIP sysdisk\n");
                    continue;
                }
            }
            /* some mmc card driver do not set this... */
            /* and some SATA to USB converters either, see issue #19. Better not to check at all */
            /*
            sprintf(path, "/sys/block/%s/removable", de->d_name);
            filegetcontent(path, vendorName, 2);
            if(vendorName[0] != '1') {
                if(verbose > 1) printf("SKIP non-removable\n");
                continue;
            }
            */
            sprintf(path, "/sys/block/%s/ro", de->d_name);
            filegetcontent(path, vendorName, 2);
            if(vendorName[0] != '0') {
                if(verbose > 1) printf("SKIP read-only\n");
                continue;
            }
            size = 0;
            sprintf(path, "/sys/block/%s/size", de->d_name);
            filegetcontent(path, vendorName, sizeof(vendorName));
            size = (uint64_t)atoll(vendorName) * 512UL;
#if USE_WRONLY
            if(!disks_all && size/1024L > DISKS_MAXSIZE*1024L*1024L) {
                if(verbose > 1) printf("SKIP too big\n");
                continue;
            }
#endif
            if(verbose > 1) printf("OK size %s", vendorName);
            memset(vendorName, 0, sizeof(vendorName));
            sprintf(path, "/sys/block/%s/device/vendor", de->d_name);
            filegetcontent(path, vendorName, sizeof(vendorName));
            sprintf(path, "/sys/block/%s/device/model", de->d_name);
            filegetcontent(path, productName, sizeof(productName));
            str[0] = 0;
            if(size) {
                sizeInGbTimes10 = (int)((uint64_t)(10 * size) >> 30L);
                if(sizeInGbTimes10 < 10) { unit = lang[L_MIB]; sizeInGbTimes10 = (int)((uint64_t)(10 * (size + 1024L*1024L-1L)) >> 20L); }
                else unit = lang[L_GIB];
                snprintf(str, sizeof(str)-1, "%s [%d.%d %s] %s %s", de->d_name,
                    sizeInGbTimes10 / 10, sizeInGbTimes10 % 10, unit, vendorName, productName);
            } else
                snprintf(str, sizeof(str)-1, "%s %s %s", de->d_name, vendorName, productName);
            str[128] = 0;
            disks_capacity[i] = size;
            disks_targets[i++] = de->d_name[0] == 's' ? de->d_name[2] : atoi(de->d_name + 6);
            main_addToCombobox(str);
            if(i >= DISKS_MAX) break;
        }
        closedir(dir);
    }
    for(k = 0; k < j; k++) free(skip[k]);
    if(disks_serial) {
        if(!serialdrivers) {
            f = fopen("/proc/tty/drivers", "r");
            if(f) {
                while(!feof(f)) {
                    if(fgets(str, sizeof(str), f)) {
                        for(k = 0, c = str, p = NULL; k < 11 && *c && *c != '\n';) {
                            if(k == 2) *c++ = 0;
                            while(*c == ' ' || *c == '\t') c++;
                            if(k == 1 && !memcmp(c, "/dev/", 5)) p = c + 5;
                            if(k == 4 && memcmp(c, "serial\n", 7)) p = NULL;
                            k++;
                            while(*c && *c != ' ' && *c != '\t' && *c != '\n') c++;
                        }
                        if(p) {
                            serials[serialdrivers] = (char*)malloc(strlen(p)+1);
                            if(serials[serialdrivers]) strcpy(serials[serialdrivers], p);
                            serialdrivers++;
                        }
                    }
                }
                fclose(f);
            }
        }
        for(k = 0; k < serialdrivers; k++)
            if(serials[k]) {
                dir = opendir("/dev");
                if(dir) {
                    while((de = readdir(dir))) {
                        if(memcmp(de->d_name, serials[k], strlen(serials[k]))) continue;
                        disks_targets[i++] = 1024 + k*256 + atoi(de->d_name + strlen(serials[k]));
                        sprintf(str, "%s %s", de->d_name, lang[L_SERIAL]);
                        main_addToCombobox(str);
                    }
                    closedir(dir);
                }
            }
    }
}

/**
 * Return mount points and bookmarks file
 */
char *disks_volumes(int *num, char ***mounts)
{
    FILE *m;
    int k = 1;
    char buf[PATH_MAX], *c, *path, *recent = NULL;
    char *env, fn[PATH_MAX];
    struct stat st;

    *mounts = (char**)realloc(*mounts, ((*num) + 5) * sizeof(char*));
    if(!*mounts) return NULL;

    if((env = getenv("XDG_USER_DATA"))) {
        snprintf(fn, sizeof(fn)-1, "%s/recently-used.xbel", env);
        if(!(k = stat(fn, &st))) {
            (*mounts)[*num] = recent = (char*)malloc(strlen(fn)+1);
            if((*mounts)[*num]) { strcpy((*mounts)[*num], fn); (*num)++; }
        }
        fn[0] = 0;
    }
    if((env = getenv("HOME"))) {
        if(k) {
            snprintf(fn, sizeof(fn)-1, "%s/.local/share/recently-used.xbel", env);
            if(!stat(fn, &st)) {
                (*mounts)[*num] = recent = (char*)malloc(strlen(fn)+1);
                if((*mounts)[*num]) { strcpy((*mounts)[*num], fn); (*num)++; }
            }
        }
        strncpy(fn, env, sizeof(fn)-1);
    } else if((env = getenv("LOGNAME")))
        snprintf(fn, sizeof(fn)-1, "/home/%s", env);
    if(fn[0] && !stat(fn, &st)) {
        (*mounts)[*num] = (char*)malloc(strlen(fn)+1);
        if((*mounts)[*num]) { strcpy((*mounts)[*num], fn); (*num)++; }
        k = strlen(fn);
        strncat(fn, "/Desktop", sizeof(fn)-1);
        if(!stat(fn, &st)) {
            (*mounts)[*num] = (char*)malloc(strlen(fn)+1);
            if((*mounts)[*num]) { strcpy((*mounts)[*num], fn); (*num)++; }
        }
        fn[k] = 0;
        strncat(fn, "/Downloads", sizeof(fn)-1);
        if(!stat(fn, &st)) {
            (*mounts)[*num] = (char*)malloc(strlen(fn)+1);
            if((*mounts)[*num]) { strcpy((*mounts)[*num], fn); (*num)++; }
        }
    }

    (*mounts)[*num] = (char*)malloc(2);
    if((*mounts)[*num]) { strcpy((*mounts)[*num], "/"); (*num)++; }

    m = fopen("/proc/self/mountinfo", "r");
    if(m) {
        while(!feof(m)) {
            if(fgets(buf, sizeof(buf), m)) {
                for(k = 0, c = buf, path = NULL; k < 11 && *c && *c != '\n';) {
                    if(k == 5) *c++ = 0;
                    while(*c == ' ' || *c == '\t') c++;
                    if(k == 4) path = c;
                    k++;
                    while(*c && *c != ' ' && *c != '\t' && *c != '\n') c++;
                }
                if(!path || !path[0] || !memcmp(path, "/dev", 4) || !memcmp(path, "/sys", 4) || !memcmp(path, "/var/", 5) ||
                    !memcmp(path, "/run", 4) || !memcmp(path, "/proc", 5) || !strcmp(path, "/")) continue;
                *mounts = (char**)realloc(*mounts, ((*num) + 1) * sizeof(char*));
                if(*mounts) {
                    (*mounts)[*num] = (char*)malloc(strlen(path)+1);
                    if((*mounts)[*num]) { strcpy((*mounts)[*num], path); (*num)++; }
                }
            }
        }
        fclose(m);
    }
    return recent;
}

/**
 * Lock, umount and open the target disk for writing
 */
void *disks_open(int targetId, uint64_t size)
{
    struct termios termios;
    int ret = 0, l, k, tiobaud;
    char deviceName[64], *c, *d, buf[1024], unesc[1024], *path, *device;
    FILE *m;
#if USE_UDISKS2
    UDisksClient *client;
    GError *error = NULL;
    GUnixFDList *fdlist;
    gint *fds = NULL, numfd;
    struct stat st;
    UDisksFilesystem *filesystem;
    UDisksBlock *block = NULL;
    GVariantBuilder builder;
    GVariant *options;
    GList *objects;
    GList *o;
#endif

    if(targetId < 0 || targetId >= DISKS_MAX || disks_targets[targetId] == -1) return (void*)-1;
    if(size && disks_capacity[targetId] && size > disks_capacity[targetId]) return (void*)-1;

    if(disks_targets[targetId] >= 1024) {
        k = disks_targets[targetId] - 1024;
        l = k & 255; k >>= 8;
        if(k >= serialdrivers) return (void*)-1;
        sprintf(deviceName, "/dev/%s%d", serials[k], l);
        errno = 0;
        ret = open(deviceName, O_RDWR | O_NOCTTY | O_NONBLOCK | O_EXCL);
        if(verbose)
            printf("disks_open(%s) serial\r\n  fd=%d errno=%d err=%s\r\n",
                deviceName, ret, errno, strerror(errno));
        if(ret < 0 || errno) {
            main_getErrorMessage();
            return NULL;
        }
        if(isatty(ret)) {
            if(tcgetattr(ret, &termios) != -1) {
                termios.c_cc[VTIME] = 0;
                termios.c_cc[VMIN] = 0;
                termios.c_iflag = 0;
                termios.c_oflag = 0;
                termios.c_cflag = CS8 | CREAD | CLOCAL;
                termios.c_lflag = 0;
                switch(baud) {
                    case 57600:   tiobaud =   B57600; break;
                    case 230400:  tiobaud =  B230400; break;
                    case 460800:  tiobaud =  B460800; break;
                    case 500000:  tiobaud =  B500000; break;
                    case 576000:  tiobaud =  B576000; break;
                    case 921600:  tiobaud =  B921600; break;
                    case 1000000: tiobaud = B1000000; break;
                    case 1152000: tiobaud = B1152000; break;
                    case 1500000: tiobaud = B1500000; break;
                    case 2000000: tiobaud = B2000000; break;
                    case 2500000: tiobaud = B2500000; break;
                    case 3000000: tiobaud = B3000000; break;
                    case 3500000: tiobaud = B3500000; break;
                    case 4000000: tiobaud = B4000000; break;
                    default: tiobaud = B115200; break;
                }
                if((cfsetispeed(&termios, tiobaud) < 0) || (cfsetospeed(&termios, tiobaud) < 0))
                {
                    if(verbose)
                        printf(" failed to set %d baud errno=%d err=%s\r\n", baud, errno, strerror(errno));
                    goto sererr;
                }
                if(tcsetattr(ret, TCSAFLUSH, &termios) == -1) {
                    if(verbose)
                        printf(" failed to set attr errno=%d err=%s\r\n", errno, strerror(errno));
                    goto sererr;
                }
            } else {
                if(verbose)
                    printf(" tcgetattr error errno=%d err=%s\r\n", errno, strerror(errno));
sererr:         main_getErrorMessage();
                close(ret);
                return (void*)-4;
            }
            if(disks_serial == 2) {
                /* Raspbootin Serial Protocol:
                 *   \003\003\003 - client to server
                 *   4 bytes little-endian, size of image - server to client
                 *   'OK' or 'SE' (size error) - client to server
                 *   image content - server to client
                 */
                if(verbose) printf("  awaiting client\r\n");
                for(k = 0; k < 3;) {
                    /* fd is non-blocking, give chance to process X11 window close events */
                    main_onProgress(NULL);
                    if(read(ret, &buf, 1)) {
                        if(buf[0] == 3) k++;
                        else {
                            k = 0;
                            if(verbose) printf("%c", buf[0]);
                        }
                    }
                    usleep(1000);
                }
                fcntl(ret, F_SETFL, 0);
                if(verbose) printf("\r\n  client connected\r\n");
                if(write(ret, &size, 4) != 4) {
                    if(verbose)
                        printf(" unable to send size errno=%d err=%s\r\n", errno, strerror(errno));
                    goto sererr;
                }
                if(read(ret, &buf, 2) != 2 || buf[0] != 'O' || buf[1] != 'K') {
                    if(verbose)
                        printf(" didn't received ACK from client, got '%c%c' errno=%d err=%s\r\n",
                            buf[0], buf[1], errno, strerror(errno));
                    goto sererr;
                }
            } else
                fcntl(ret, F_SETFL, 0);
        }
        return (void*)((long int)ret);
    }

#if DISKS_TEST
    if((char)disks_targets[targetId] == 'T') {
        sprintf(deviceName, "./test.bin");
        unlink(deviceName);
        errno = 0;
        ret = open(deviceName, O_RDWR | O_EXCL | O_CREAT, 0644);
        if(verbose)
            printf("disks_open(%s)\r\n  fd=%d errno=%d err=%s\r\n",
                deviceName, ret, errno, strerror(errno));
        if(ret < 0 || errno) {
            main_getErrorMessage();
            return NULL;
        }
        return (void*)((long int)ret);
    }
#endif
    if((char)disks_targets[targetId] >= 'a')
        sprintf(deviceName, "/dev/sd%c", (char)disks_targets[targetId]);
    else
        sprintf(deviceName, "/dev/mmcblk%d", disks_targets[targetId]);
    if(verbose) printf("disks_open(%s)\r\n", deviceName);

    m = fopen("/proc/self/mountinfo", "r");
    if(m) {
        l = strlen(deviceName);
        while(!feof(m)) {
            if(fgets(buf, sizeof(buf), m)) {
                for(k = 0, c = buf, path = device = NULL; k < 11 && *c && *c != '\n';) {
                    if(k == 5 || k == 10) *c++ = 0;
                    while(*c == ' ' || *c == '\t') c++;
                    if(k == 4) path = c;
                    if(k == 9) device = c;
                    k++;
                    while(*c && *c != ' ' && *c != '\t' && *c != '\n') c++;
                }
                if(device && !strncmp(device, deviceName, l)) {
                    if(!path || !strcmp(path, "/") || !strcmp(path, "/boot")) {
                        if(verbose) printf("  failsafe not umounting (%s)\r\n", path);
                        fclose(m); return (void*)-2;
                    }
                    memset(unesc, 0, sizeof(unesc));
                    for(c = path, d = unesc; *c && d < unesc + sizeof(unesc) - 1; c++, d++) {
                        if(*c == '\\') {
                            c++;
                            switch(*c) {
                                case 't': *d = '\t'; break;
                                case 'r': *d = '\r'; break;
                                case 'n': *d = '\n'; break;
                                case '0': *d = (c[1] - '0') * 8 + (c[2] - '0'); c += 2; break;
                            }
                        } else
                            *d = *c;
                    }
                    if(verbose) printf("  umount(%s)\r\n", unesc);
                    if(umount2(unesc, MNT_FORCE)) {
#if USE_UDISKS2
                        if(verbose > 1) printf("  trying udisks2 umount\r\n");
                        /* fallback to udisks2 umount */
                        if((errno == EPERM || errno == EACCES) && !stat(unesc, &st) && (client = udisks_client_new_sync(NULL, NULL))) {
                            objects = g_dbus_object_manager_get_objects(udisks_client_get_object_manager (client));
                            for (o = objects, filesystem = NULL; o != NULL; o = o->next) {
                                block = udisks_object_peek_block(UDISKS_OBJECT(o->data));
                                if(block != NULL && udisks_block_get_device_number(block) == st.st_dev) {
                                    filesystem = udisks_object_peek_filesystem(g_object_ref(UDISKS_OBJECT(o->data)));
                                    break;
                                }
                            }
                            g_list_foreach(objects, (GFunc) g_object_unref, NULL);
                            g_list_free(objects);
                            g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
                            error = NULL;
                            if(!filesystem || !udisks_filesystem_call_unmount_sync(filesystem,
                                g_variant_builder_end(&builder), NULL, &error)) {
                                    main_errorMessage = error ? error->message : "No block device???";
                                    if(verbose) printf("  udisks2 umount err=%s\r\n", main_errorMessage);
                                    g_object_unref(client);
                                    fclose(m);
                                    return (void*)-2;
                            }
                            g_object_unref(client);
                        } else
#endif
                        {
                            if(verbose) printf("  errno=%d err=%s\r\n", errno, strerror(errno));
                            main_getErrorMessage();
                            fclose(m);
                            return (void*)-2;
                        }
                    }
                }
            }
        }
        fclose(m);
    }

    errno = 0;
    ret = open(deviceName, O_RDWR | O_SYNC | O_EXCL);
    if(verbose) printf("  fd=%d errno=%d err=%s\r\n", ret, errno, strerror(errno));
    if(ret < 0 || errno) {
#if USE_UDISKS2
        /* fallback to udisks2 open_device */
        if((errno == EPERM || errno == EACCES) && !stat(deviceName, &st) && (client = udisks_client_new_sync(NULL, NULL))) {
            objects = g_dbus_object_manager_get_objects(udisks_client_get_object_manager(client));
            for(o = objects, block = NULL; o != NULL; o = o->next) {
                block = udisks_object_peek_block(UDISKS_OBJECT(o->data));
                if(block != NULL && udisks_block_get_device_number(block) == st.st_rdev) break;
                else block = NULL;
            }
            error = NULL; main_errorMessage = NULL;
            g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
            g_variant_builder_add(&builder, "{sv}", "flags", g_variant_new_int32(O_SYNC | O_EXCL));
            options = g_variant_builder_end(&builder);
            g_variant_ref_sink(options);
            fdlist = g_unix_fd_list_new();
            if(!block || !udisks_block_call_open_device_sync(block, "rw", options, NULL, NULL, &fdlist, NULL, &error) ||
               !(fds = g_unix_fd_list_steal_fds(fdlist, &numfd)) || !fds || !numfd) {
                    main_errorMessage = error ? error->message : "No block device???";
                    ret = 0;
            } else
                ret = fds[0];
            if(verbose) printf("  udisks2 open_device fd=%d err=%s\r\n", ret, main_errorMessage);
            if(fds) g_free(fds);
            g_list_foreach(objects, (GFunc)g_object_unref, NULL);
            g_list_free(objects);
            g_object_unref(fdlist);
            g_object_unref(options);
            g_object_unref(client);
        } else
#endif
        {
            main_getErrorMessage();
            return NULL;
        }
    }
    return (void*)((long int)ret);
}

/**
 * Close the target disk
 */
void disks_close(void *data)
{
    int fd = (int)((long int)data);
    fdatasync(fd);
    close(fd);
    if(verbose) printf("disks_close(%d)\r\n", fd);
}
