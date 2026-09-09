/* fakevt.c - LD_PRELOAD shim for X.Org 1.20 (RHEL9) in containers.
 *
 * X 1.20's xf86OpenConsole() / parse_vt_settings() require a working VT:
 * they call open("/dev/tty0") then VT and KD ioctls to claim a console.
 * In a container there is no real VT; these ioctls fail and X 1.20 (RHEL9
 * build) treats the failure as fatal.
 *
 * This shim intercepts:
 *   - open/open64/openat/openat64 for /dev/ttyN -> opens /dev/null
 *   - ioctl for all VT/KD requests on those fds -> returns success
 *   - close on those fds -> untracks and forwards
 *
 * The nvidia DDX does NOT depend on VTs for rendering (uses GPU scanout).
 *
 * Key insights from RHEL9 X 1.20 runtime trace:
 *   - X opens /dev/tty0 (not /dev/tty1-63)
 *   - First ioctl is 0x5600 (_IO(0x56,0)) with a pointer arg (fill 0)
 *   - Then VT_GETMODE(0x5603), VT_ACTIVATE(0x5606), VT_SWITCH(0x5607),
 *     VT_WAITACTIVE(0x5601), VT_SETMODE(0x5602)
 *   - Then non-standard keyboard ioctls 0x4b3a, 0x4b44, 0x4b45
 *   - SET/ACTIVATE ioctls pass an INTEGER arg (VT number, mode) NOT a
 *     pointer. GET ioctls pass a pointer. Never dereference small values.
 *
 * Build: gcc -shared -fPIC -o /usr/local/lib/fakevt.so /usr/local/src/fakevt.c
 * Usage: LD_PRELOAD=/usr/local/lib/fakevt.so Xorg :1 ...
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

/* struct vt_stat (kernel definition) */
struct vt_stat {
    unsigned char v_active;
    unsigned char v_signal;
    unsigned char v_state;
    unsigned char v_lsession;
};
/* Linux VT ioctls: _IO(0x56, nr) = (0x56 << 8) | nr */
#define VTNR_GETSTATE   0x5661  /* _IO(0x56, 0x61) */
#define VTNR_OPENQRY    0x5660  /* _IO(0x56, 0x60) */
/* Linux KD ioctls: _IO('R'=0x52, nr) = (0x52 << 8) | nr */
#define KDNR_SETMODE    0x5200  /* _IO('R', 0x00) */
#define KDNR_GETMODE    0x5201  /* _IO('R', 0x01) */

/* Track fds we opened as fake VTs */
static int fake_fds[64];
static int fake_count = 0;

static int is_fake_fd(int fd)
{
    for (int i = 0; i < fake_count; i++)
        if (fake_fds[i] == fd) return 1;
    return 0;
}

static void add_fake_fd(int fd)
{
    if (fd >= 0 && fake_count < 64)
        fake_fds[fake_count++] = fd;
}

static void remove_fake_fd(int fd)
{
    for (int i = 0; i < fake_count; i++) {
        if (fake_fds[i] == fd) {
            fake_fds[i] = fake_fds[fake_count - 1];
            fake_count--;
            return;
        }
    }
}

/* Check if pathname is /dev/ttyN (N = 0-99) or /dev/tty (exactly) */
static int is_tty_path(const char *pathname)
{
    if (!pathname) return 0;
    if (strncmp(pathname, "/dev/tty", 8) != 0) return 0;
    const char *p = pathname + 8;
    if (*p == '\0') return 1;            /* /dev/tty */
    if (*p == '0') return 1;             /* /dev/tty0 */
    if (*p >= '1' && *p <= '9') {        /* /dev/tty1 - /dev/tty99 */
        if (p[1] == '\0') return 1;
        if (p[1] >= '0' && p[1] <= '9' && p[2] == '\0') return 1;
    }
    return 0;
}

/* Determine if an ioctl third-arg is a pointer (vs. an integer value).
 * On x86_64, user-space pointers are >= 0x1000 and < canonical limit.
 * Small values are integer args (VT numbers, modes, etc). */
static int ioctl_arg_is_ptr(long arg)
{
    return (arg > 0x1000 && arg < 0x7FFFFFFFFFFF);
}

/* --- open family interception --- */

int open(const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    static int (*real_open)(const char *, int, ...) = NULL;
    if (!real_open) real_open = dlsym(RTLD_NEXT, "open");

    if (is_tty_path(pathname)) {
        int fd = real_open("/dev/null", O_RDWR);
        add_fake_fd(fd);
        return fd;
    }
    return real_open(pathname, flags, mode);
}

int open64(const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    static int (*real_open)(const char *, int, ...) = NULL;
    if (!real_open) real_open = dlsym(RTLD_NEXT, "open");

    if (is_tty_path(pathname)) {
        int fd = real_open("/dev/null", O_RDWR);
        add_fake_fd(fd);
        return fd;
    }
    return real_open(pathname, flags | O_LARGEFILE, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    static int (*real_openat)(int, const char *, int, ...) = NULL;
    if (!real_openat) real_openat = dlsym(RTLD_NEXT, "openat");

    if (dirfd == AT_FDCWD && is_tty_path(pathname)) {
        int fd = real_openat(AT_FDCWD, "/dev/null", O_RDWR);
        add_fake_fd(fd);
        return fd;
    }
    return real_openat(dirfd, pathname, flags, mode);
}

int openat64(int dirfd, const char *pathname, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    static int (*real_openat)(int, const char *, int, ...) = NULL;
    if (!real_openat) real_openat = dlsym(RTLD_NEXT, "openat");

    if (dirfd == AT_FDCWD && is_tty_path(pathname)) {
        int fd = real_openat(AT_FDCWD, "/dev/null", O_RDWR);
        add_fake_fd(fd);
        return fd;
    }
    return real_openat(dirfd, pathname, flags | O_LARGEFILE, mode);
}

/* --- ioctl interception --- */

int ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    va_start(ap, request);
    long arg = va_arg(ap, long);
    va_end(ap);

    /* Intercept ALL ioctls on fake tty fds.
     * In a container there's no real VT, so any ioctl on a fake tty
     * should return success. Fill pointer args with sensible defaults. */
    if (is_fake_fd(fd)) {
        if (ioctl_arg_is_ptr(arg)) {
            if (request == VTNR_GETSTATE) {
                struct vt_stat *st = (struct vt_stat *)arg;
                st->v_active = 0;
                st->v_signal = 0;
                st->v_state  = 0;
                st->v_lsession = 0;
            } else if (request == KDNR_GETMODE) {
                *(int *)arg = 1; /* KD_GRAPHICS */
            } else {
                /* VT_OPENQRY, 0x5600, and other GET ioctls: zero the buffer */
                *(int *)arg = 0;
            }
        }
        /* Integer args (VT_ACTIVATE=1, KDSETMODE=KD_GRAPHICS, etc):
         * nothing to fill, just return success. */
        return 0;
    }

    /* Intercept KD/keyboard ioctls on ANY fd. X may call these on a
     * non-tracked fd after internal close/reopen cycles. */
    if (request == KDNR_SETMODE || request == KDNR_GETMODE ||
        request == 0x4b3a || request == 0x4b44 || request == 0x4b45) {
        if (ioctl_arg_is_ptr(arg) &&
            (request == KDNR_GETMODE || request == 0x4b3a)) {
            *(int *)arg = 1;
        }
        return 0;
    }

    /* All other ioctls: forward via syscall to avoid variadic issues */
    return (int)syscall(SYS_ioctl, fd, request, arg);
}

/* --- close interception --- */

int close(int fd)
{
    remove_fake_fd(fd);
    return (int)syscall(SYS_close, fd);
}
