USBImager
=========

<img src="https://gitlab.com/bztsrc/usbimager/raw/master/src/misc/icon32.png">
[USBImager](https://bztsrc.gitlab.io/usbimager) is a really really simple GUI application that writes compressed disk images to USB drives
and creates backups. Available platforms: Windows, MacOSX and Linux. Its interface is as simple as it gets, totally bloat-free.

| Platform     | Frontend     | Description                  |
|--------------|--------------|------------------------------|
| Windows      | [GDI](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-i686-win-gdi.zip)<br>[GDI wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-i686-win-gdi.zip) | native interface<br>simplified, write-only interface |
| MacOSX       | [Cocoa](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-intel-macosx-cocoa.zip)<br>[Cocoa wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-intel-macosx-cocoa.zip) | native interface<br>simplified, write-only interface |
| Ubuntu LTS   | [GTK+](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-amd64.deb)<br>[GTK+ wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-amd64.deb) | same as the Linux PC GTK version with udisks2 support, but in .deb format<br>simplified, write-only interface |
| RaspiOS      | [GTK+](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-armhf.deb)<br>[GTK+ wo](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7_wo-armhf.deb) | same as the Raspberry Pi GTK version with udisks2 support, but in .deb format<br>simplified, write-only interface |
| Arch/Manjaro | [GTK+](https://aur.archlinux.org/packages/usbimager/)<br>[GTK+](https://aur.archlinux.org/packages/usbimager-bin/)<br>[X11](https://aur.archlinux.org/packages/usbimager-x11/) | same as the Linux PC GTK version with udisks2 support, but in an AUR package<br>generated from the binaries<br>minimal X11 version |
| Linux PC     | [X11](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-x86_64-linux-x11.zip)<br>[GTK+](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-x86_64-linux-gtk.zip) | recommended<br>compatibility (requires udisks2) |
| Raspberry Pi | [X11](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-armv7l-linux-x11.zip)<br>[X11](https://gitlab.com/bztsrc/usbimager/raw/binaries/usbimager_1.0.7-aarch64-linux-x11.zip) | native interface, AArch32 (armv7l)<br>native interface, AArch64 (arm64) |

Screenshots
-----------

<img src="https://gitlab.com/bztsrc/usbimager/raw/master/usbimager.png">

Installation
------------

1. download one of the `usbimager_*.zip` archives from the [repo](https://gitlab.com/bztsrc/usbimager/tree/binaries) for your desktop (less than 192 Kilobytes each)
2. extract to: `C:\Program Files` (Windows), `/Applications` (MacOSX) or `/usr` (Linux)
3. Enjoy!

You can use the executable in the archive as-is, the other files only provide integration with your desktop (icons and such). It will autodetect your
operating system's configured language, and if dictionary found, it will greet you in your language.

On Ubuntu LTS and Raspbian machines you can also download the deb version, which then can be installed by the
```
sudo dpkg -i usbimager_*.deb
```
command.

Support the Development by Donation
-----------------------------------

If you like it or find it useful, your donation of any amount will be very much appreciated:<br>
<a href="bitcoin:3EsdxN1ZsX5JkLgk3uR4ybHLDX5i687dkx"><img src="https://gitlab.com/bztsrc/usbimager/raw/master/donate.png"><br>BTC 3EsdxN1ZsX5JkLgk3uR4ybHLDX5i687dkx</a>

Features
--------

- Open Source and MIT licensed
- Portable executable, no installation needed, just extract the archives
- Small. Really small, few kilobytes only, yet has no dependencies
- No privacy concerns nor advertisements like with etch*r, fully GDPR compatible
- Minimalist, multilingual, native interface on all platforms
- Tries to be bullet-proof and avoids overwriting of the system disk
- Makes synchronized writes, that is, all data is on disk when the progressbar reaches 100%
- Can verify writing by comparing the disk to the image
- Can read raw disk images: .img, .bin, .raw, .iso, .dd, etc.
- Can read compressed images on-the-fly: .gz, .bz2, .xz, .zst
- Can read archives on-the-fly: .zip (PKZIP and ZIP64), .zzz (ZZZip), .tar, .cpio, .pax (*)
- Can create backups in raw and ZStandard compressed format
- Can send images to microcontrollers over serial line
- Available in 17 languages

(* - for archives with multiple files, the first file in the archive is used as input)

Comparition
-----------

| Description                    | balenaEtcher  | WIN32 Disk Imager | USBImager |
|--------------------------------|---------------|-------------------|-----------|
| Multiplatform                  | ✔             | ✗                 | ✔         |
| Minimum Windows                | Win 7         | Win XP            | Win XP    |
| Minimum MacOSX (1)             | ?             | ✗                 | 10.13     |
| Available on Raspbian          | ✗             | ✗                 | ✔         |
| Program size (2)               | 130 Mb        | ✗                 | 300 Kb    |
| Dependencies                   | lots, ~300 Mb | Qt, ~8 Mb         | ✗ none    |
| Spyware-free and ad-free       | ✗             | ✔                 | ✔         |
| Native interface               | ✗             | ✗                 | ✔         |
| Guarantee on data writes (3)   | ✗             | ✗                 | ✔         |
| Verify data written            | ✔             | ✗                 | ✔         |
| Compressed images              | ✔             | ✗                 | ✔         |
| Raw write time (4)             | 23:16         | 23:28             | 24:05     |
| Compressed write time (4)      | 01:12:51      | ✗                 | 30:47     |

(1) - the provided binary was compiled under 10.14 (because that's what I have), however it was reported that you can compile the source under 10.13 too without problems.

(2) - the portable executable's size on Windows platform. I couldn't download an official pre-compiled version of WIN32 Disk Imager, just the source.

(3) - USBImager uses only non-buffered IO operations to make sure data is physically written to disk

(4) - measurements performed by @CaptainMidnight on Windows 10 Pro using a SanDisk Ulta 32GB A1 device. Raw image file size was 31,166,976 Kb, the bzip2 compressed image size was 1,887,044 Kb. WIN32 Disk Imager was unable to uncompress the image file, therefore the resulting card was unbootable.

Usage
-----

If you can't write to the target device (you get "permission denied" errors), then:

__Windows__: right-click on usbimager.exe and use the "Run as Administrator" option.

__MacOSX__: 10.14 an up: go to "System Preferences", "Security & Privacy" and "Privacy". Add USBImager to the list of "Full Disk Access". Alternatively
run from a Terminal as *sudo /Applications/USBImager.app/Contents/MacOS/usbimager* (this latter is the only way under 10.13).

__Linux__:  this should not be an issue as USBImager comes with setgid bit set. If not, then you can use *sudo chgrp disk usbimager && sudo chmod g+s usbimager*
to set it. Alternatively add your user to the "disk" group (see "ls -la /dev|grep -e ^b" to find out which group your OS is using).
__Should be no need__ for *sudo /usr/bin/usbimager*, just make sure your user has write access to the devices, that's the Principle of Least Privilege.

### Interface

1. row: image file
2. row: operations, write and read respectively
3. row: device selection
4. row: options: verify write, compress output and buffer size respectively

For X11 I made everything from scratch to avoid dependencies. Clicking and keyboard navigation works as expected: <kbd>Tab</kbd> and <kbd>Shift</kbd> +
<kbd>Tab</kbd> switches the input field, <kbd>Enter</kbd> selects. Plus in Open File dialog <kbd>Left</kbd> / <kbd>BackSpace</kbd> goes one directory up,
<kbd>Right</kbd> / <kbd>Enter</kbd> goes one directory down (or selects the item if it's not a directory). You can use <kbd>Shift</kbd> + <kbd>Up</kbd> /
<kbd>Down</kbd> to change the sorting order. "Recently Used" files also supported (through freedesktop.org's [Desktop Bookmarks](https://freedesktop.org/wiki/Specifications/desktop-bookmark-spec/) Standard).

### Writing Image File to Device

1. select an image by clicking on "..." in the 1st row
2. select a device by clicking on the 3rd row
3. click on the first button (Write) in the 2nd row

With this operation, the file format and the compression is autodetected. Please note that the remaining time is just an estimate. Some
compressed files do not store the uncompressed file size, for those you will see "x MiB so far" in the status bar. Their remaining time will be
less accurate, just an approximation of an estimation using the ratio of compressed position / compressed size (in short it is truly
nothing more than a rough estimate).

If "Verify" is clicked, then each block is read back from the disk and compared to the original image.

The last option, the selection box selects the buffer size to use. The image file will be processed in this big chunks. Keep in
mind that the actual memory requirement is threefold, because there's one buffer for the compressed data, one for the uncompressed data,
and one for the data read back for verification.

### Creating Backup Image File from Device

1. select a device by clicking on the 3rd row
2. click on the second button (Read) in the 2nd row
3. the image file will be saved on your Desktop, its name is in the 1st row

The generated image file is in the form "usbimager-(date)T(time).dd", generated with the current timestamp. If "Compress" option is checked, then a ".zst" suffix will
be added, and the image will be compressed using ZStandard. It has much better compression ratio than gzip deflate. For raw images the remaining
time is accurate, however for compression it highly depends on the time taken by the compression algorithm, which in turn depends on the data,
so remaining time is just an estimate.

Note: on Linux, if ~/Desktop is not found, then ~/Downloads will be used. If even that doesn't exists, then the image file will be saved in your home directory. On
other platforms the Desktop always exists, but if by any chance not, then the current directory is used. On all platforms, if an existing
directory is specified on the command line, that is used to save backups.

### Advanced Functionalities

| Flag                | Description         |
|---------------------|---------------------|
| -v/-vv              | Be verbose          |
| -Lxx                | Force language      |
| -1..9               | Set buffer size     |
| -a                  | List all devices    |
| -s\[baud]/-S\[baud] | Use serial devices  |
| --version           | Prints version      |
| (dir)               | First non-flag is the backup directory |

For Windows users: right-click on usbimager.exe, and select "Create Shortcut". Then right-click on the newly created ".lnk" file, and
select "Properties". On the "Shortcut" tab, in the "Target" field, you can add the flags. On the "Security" tab, you can also set
to run USBImager as Administrator if you have problems accessing the raw disk devices.

The first argument which is not a flag (does not start with '-') is used as the backup image directory.

With '-a', all devices will be listed, even system disks and large disks. With this you can seriously damage your computer, be careful.

Flags can be given separately (like "usbimager -v -s -2") or at once ("usbimager -2vs"), the order doesn't matter. For flags that set
the same thing, only the last taken into account (for example "-124" is the same as "-4").

The '-v' and '-vv' flags will make USBImager to be verbose, and it will print out details to the console. That is stdout on Linux and MacOSX
(so run this in a Terminal), and on Windows a spearate window will be opened for messages.

The last two character of '-Lxx' flag can be "en", "es", "de", "fr" etc. Using this flag forces a specific language dictionary and avoids
automatic detection. If there's no such dictionary, then English is used.

The number flags sets the buffer size to the power of two Megabytes (0 = 1M, 1 = 2M, 2 = 4M, 3 = 8M, 4 = 16M, ... 9 = 512M). When not
specified, buffer size defaults to 1 Megabyte.

If you start USBImager with the '-s' flag (lowercase), then it will allow you to send images to serial ports as well. For this, your user
has to be the member of the "uucp" or "dialout" groups (differs in distributions, use "ls -la /dev/|grep tty" to see which one). In this
case on the client side:
1. the client must wait indefinitely for the first byte to arrive, then store that byte into a buffer
2. then it must read further bytes with a timeout (let's say 250 ms or 500 ms) and store those in the buffer as well
3. when the timeout occurs, the image is received.

The '-S' flag (uppercase) is similar, but then USBImager will do a [raspbootin](https://github.com/bztsrc/raspi3-tutorial/tree/master/14_raspbootin64) hand-shake
on the serial line:
1. USBImager awaits for the client
2. client sends 3 bytes, '\003\003\003' (3 times <kbd>Ctrl</kbd>+<kbd>C</kbd>)
3. USBImager sends size of the image, 4 bytes in little-endian (size = 4th byte * 16777216 + 3rd byte * 65536 + 2nd byte * 256 + 1st byte)
4. client responds with two bytes, either 'OK' or 'SE' (size error)
5. if the response was OK, then USBImager sends the image, size bytes
6. when the client got the sizeth byte, the image is received.

For both case the serial line is set to 115200 baud, 8 data bits, no parity, 1 stop bit. For serial transfers, USBImager does not uncompress the image to minimize
transfer times, so that has to be done on the client side. For a simple boot loader that's compatible with USBImager, take a look at
[Image Receiver](https://gitlab.com/bztsrc/imgrecv) (available for RPi1, 2, 3, 4 and IBM PC BIOS machines). Also used to send
emergency initial ramdisks to [BOOTBOOT](https://gitlab.com/bztsrc/bootboot) compliant boot loaders.

If you want to use a different baud, just simply add it to the flag, like "-s57600" or "-S230400". Valid values are:
57600, 115200, 230400, 460800, 500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000, 4000000

WARNING: not every serial port supports all baud rates. Check you device's manual.

Compilation
-----------

Please refer to the Appendix in the [manual](https://gitlab.com/bztsrc/usbimager/-/raw/master/usbimager-manual.pdf).

Known Issues
------------

None. If you find any, please use the [issue tracker](https://gitlab.com/bztsrc/usbimager/issues).

Authors
-------

- libui: Pietro Gagliardi
- bzip2: Julian R. Seward
- xz: Igor Pavlov and Lasse Collin
- zlib: Mark Adler
- zstd: FB, various contributors (see http://www.zstd.net)
- zip format: bzt (no PKWARE-related lib nor source was used in this project)
- usbimager: bzt

Contributors
------------

I'd like to say thanks to @mattmiller, @MisterEd, @the_scruss, @rpdom, @DarkElvenAngel, and especially to @tvjon, @CaptainMidnight and @gitlabhack for testing
USBImager on various platforms with various devices.

My thanks for checking and fixing the translations goes to: @mline, @vordenken (German), @epoch1970, @JumpZero (French), and @hansotten, @zonstraal (Dutch), @ller (Russian), @zaval (Ukrainian), @lmarmisa (Spanish), @otani (Japanese), @ngedizaydindogmus (Turkish), @coltrane (Portuguese), @Matthaiks (Polish).

Further thanks to @munntjlx and @lfomartins for compiling USBImager on MacOS for me, and to @tido- for the Ubuntu debs.

Bests,

bzt
