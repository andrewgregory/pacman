/*
 * Copyright 2015 Andrew Gregory <andrew.gregory.8@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Project URL: http://github.com/andrewgregory/pactest.c
 */

#ifndef PACTEST_C
#define PACTEST_C

#define PACTEST_C_VERSION 1.0

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>

#include <alpm.h>

typedef struct pt_env_t {
    alpm_handle_t *handle;
    alpm_list_t *dbs;
    alpm_list_t *pkgs;
    char *dbpath;
    char *root;
    int dbfd;
    int rootfd;
} pt_env_t;

typedef struct pt_db_t {
    char *name;
    alpm_list_t *pkgs;
} pt_db_t;

enum pt_ftype {
    PT_FTYPE_FILE,
    PT_FTYPE_SYMLINK,
    PT_FTYPE_DIRECTORY
};

typedef struct pt_pkg_file_t {
    enum pt_ftype type;
    char *path;
    char *contents;
} pt_pkg_file_t;

typedef struct pt_pkg_t {
    alpm_list_t *backup;
    alpm_list_t *checkdepends;
    alpm_list_t *conflicts;
    alpm_list_t *depends;
    alpm_list_t *files;
    alpm_list_t *groups;
    alpm_list_t *licenses;
    alpm_list_t *makedepends;
    alpm_list_t *optdepends;
    alpm_list_t *provides;
    alpm_list_t *replaces;
    char *arch;
    char *base;
    char *builddate;
    char *csize;
    char *desc;
    char *filename;
    char *installdate;
    char *isize;
    char *name;
    char *packager;
    char *scriptlet;
    char *version;
} pt_pkg_t;

void _pt_pkg_file_free(pt_pkg_file_t *f) {
    if(f == NULL) { return; }
    free(f->path);
    free(f->contents);
    free(f);
}

void _pt_db_free(pt_db_t *db) {
    if(db == NULL) { return; }
    alpm_list_free(db->pkgs);
    free(db->name);
    free(db);
}

void _pt_pkg_free(pt_pkg_t *pkg) {
    if(pkg == NULL) { return; }
    alpm_list_free_inner(pkg->files, (alpm_list_fn_free) _pt_pkg_file_free);
    alpm_list_free(pkg->files);
    free(pkg->name);
    free(pkg);
}

/******************************************
 * file system utilities
 ******************************************/

char *pt_path(pt_env_t *pt, const char *path) {
    static char fullpath[PATH_MAX];
    if(path[0] == '/') {
        strcpy(fullpath, path);
    } else {
        snprintf(fullpath, PATH_MAX, "%s/%s", pt->root, path);
    }
    return fullpath;
}

void pt_rmrfat(int dd, const char *path) {
    if(!unlinkat(dd, path, 0)) {
        return;
    } else {
        struct dirent *de;
        DIR *d;
        int fd;

        switch(errno) {
            case ENOENT:
                return;
            case EPERM:
            case EISDIR:
                break;
            default:
                /* not a directory */
                return;
        }

        fd = openat(dd, path, O_DIRECTORY);
        d = fdopendir(fd);
        if(!d) { return; }
        for(de = readdir(d); de != NULL; de = readdir(d)) {
            if(strcmp(de->d_name, "..") != 0 && strcmp(de->d_name, ".") != 0) {
                char name[PATH_MAX];
                snprintf(name, PATH_MAX, "%s/%s", path, de->d_name);
                pt_rmrfat(dd, name);
            }
        }
        closedir(d);
        unlinkat(dd, path, AT_REMOVEDIR);
    }
}

int pt_mkdirat(int dd, int mode, const char *path) {
    struct stat buf;
    char dir[PATH_MAX], *c = dir;
    if(strlen(path) > PATH_MAX) { errno = ENAMETOOLONG; return -1; }
    strcpy(dir, path);
    while((c = strchr(c + 1, '/'))) {
        *c = '\0';
        if(fstatat(dd, dir, &buf, 0) != 0 && mkdirat(dd, dir, mode) != 0) {
            return -1;
        }
        *c = '/';
    }
    return 0;
}

int pt_mkpdirat(int dd, int mode, const char *path) {
    char *c;
    if((c = strrchr(path, '/'))) {
        char dir[PATH_MAX] = "";
        if(c - path + 1 > PATH_MAX) { errno = ENAMETOOLONG; return -1; }
        strncat(dir, path, c - path + 1);
        return pt_mkdirat(dd, mode, dir);
    }
    return 0;
}

int pt_grepat(int dd, const char *path, const char *needle) {
    int fd;
    char buf[LINE_MAX];
    FILE *f;
    if((fd = openat(dd, path, O_RDONLY)) == -1) { return 0; }
    if((f = fdopen(fd, "r")) == NULL) { return 0; }
    while(fgets(buf, sizeof(buf), f)) {
        if(strstr(buf, needle)) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

int pt_symlinkat(int dd, const char *path, const char *target) {
    pt_mkpdirat(dd, 0700, path);
    return symlinkat(target, dd, path);
}

int pt_writeat(int dd, const char *path, const char *contents) {
    int fd, flags = O_CREAT | O_WRONLY | O_TRUNC;
    pt_mkpdirat(dd, 0700, path);
    if((fd = openat(dd, path, flags, 0644)) == -1) {
        return -1;
    }
    write(fd, contents, strlen(contents));
    close(fd);
    return 0;
}

FILE *pt_fopenat(int dirfd, const char *path, const char *mode) {
    int fd, flags = 0, rwflag = 0;
    FILE *stream;
    switch(*(mode++)) {
        case 'r': rwflag = O_RDONLY; break;
        case 'w': rwflag = O_WRONLY; flags |= O_CREAT | O_TRUNC; break;
        case 'a': rwflag = O_WRONLY; flags |= O_CREAT | O_APPEND; break;
        default: errno = EINVAL; return NULL;
    }
    if(mode[1] == 'b') { mode++; }
    if(mode[1] == '+') { mode++; rwflag = O_RDWR; }
    while(*mode) {
        switch(*(mode++)) {
            case 'e': flags |= O_CLOEXEC; break;
            case 'x': flags |= O_EXCL; break;
        }
    }
    if((fd = openat(dirfd, path, flags | rwflag, 0666)) < 0) { return NULL; }
    if((stream = fdopen(fd, mode)) == NULL) { close(fd); return NULL; }
    return stream;
}

/******************************************
 * environment creation
 ******************************************/

pt_env_t *pt_new(const char *dbpath) {
    pt_env_t *pt;
    char root[] = "/tmp/pactest-XXXXXX";
    if(dbpath == NULL) { dbpath = "var/lib/pacman/"; }
    if(mkdtemp(root) == NULL) { return NULL; }
    if((pt = calloc(sizeof(pt_env_t), 1)) == NULL) { return NULL; }
    pt->root = strdup(root);
    pt->rootfd = open(pt->root, O_DIRECTORY);
    pt->dbpath = strdup(pt_path(pt, dbpath));
    pt_mkdirat(pt->rootfd, 0700, pt->dbpath);
    pt->dbfd = openat(pt->rootfd, pt->dbpath, O_DIRECTORY);
    pt_writeat(pt->dbfd, "local/ALPM_DB_VERSION", "9");
    return pt;
}

alpm_handle_t *pt_initialize(pt_env_t *pt, alpm_errno_t *err) {
    if(err) { *err = 0; }
    pt->handle = alpm_initialize(pt->root, pt->dbpath, err);
    return pt->handle;
}

int pt_pkg_writeat(int dd, const char *path, pt_pkg_t *pkg) {
    alpm_list_t *i;
    char *buf;
    size_t buflen;
    FILE *contents;
    int fd;
    struct archive *a = archive_write_new();
    struct archive_entry *e;
    char *c;

    if((c = strrchr(path, '.'))) {
        if(strcmp(c, ".bz2") == 0) {
            archive_write_add_filter_bzip2(a);
        } else if(strcmp(c, ".gz") == 0) {
            archive_write_add_filter_gzip(a);
        } else if(strcmp(c, ".xz") == 0) {
            archive_write_add_filter_xz(a);
        } else if(strcmp(c, ".lz") == 0) {
            archive_write_add_filter_lzip(a);
        } else if(strcmp(c, ".Z") == 0) {
            archive_write_add_filter_compress(a);
        }
    }

    pt_mkpdirat(dd, 0700, path);
    fd = openat(dd, path, O_CREAT | O_WRONLY, 0644);;
    archive_write_set_format_ustar(a);
    archive_write_open_fd(a, fd);

    contents = open_memstream(&buf, &buflen);
    fprintf(contents, "%s = %s\n", "pkgname", pkg->name);
    fprintf(contents, "%s = %s\n", "pkgver", pkg->version);
    fprintf(contents, "%s = %s\n", "arch", pkg->arch);
    fclose(contents);

    e = archive_entry_new();
    archive_entry_set_pathname(e, ".PKGINFO");
    archive_entry_set_filetype(e, AE_IFREG);
    archive_entry_set_perm(e, 0644);
    archive_entry_set_size(e, buflen);
    archive_write_header(a, e);
    archive_write_data(a, buf, buflen);
    free(buf);

    for(i = pkg->files; i; i = i->next) {
        pt_pkg_file_t *f = i->data;
        size_t len = f->contents ? strlen(f->contents) : 0;
        archive_entry_clear(e);
        archive_entry_set_pathname(e, f->path);
        switch(f->type) {
            case PT_FTYPE_FILE:
                archive_entry_set_filetype(e, AE_IFREG);
                archive_entry_set_perm(e, 0644);
                break;
            case PT_FTYPE_SYMLINK:
                archive_entry_set_filetype(e, AE_IFLNK);
                archive_entry_set_perm(e, 0644);
                break;
            case PT_FTYPE_DIRECTORY:
                archive_entry_set_filetype(e, AE_IFDIR);
                archive_entry_set_perm(e, 0755);
                break;
        }
        archive_entry_set_size(e, len);
        archive_write_header(a, e);
        archive_write_data(a, f->contents, len);
    }

    archive_entry_free(e);
    archive_write_free(a);
    close(fd);
    return 0;
}

int _pt_fwrite_dbentry(FILE *f, const char *section, const char *value) {
    if(value == NULL) { return 0; }
    return fprintf(f, "%%%s%%\n%s\n\n", section, value);
}

void _pt_fwrite_dblist(FILE *f, const char *section, alpm_list_t *values) {
    fprintf(f, "%%%s%%\n", section);
    while(values) {
        fprintf(f, "%s\n", (char *) values->data);
        values = values->next;
    }
    fputc('\n', f);
}

int pt_install_db(pt_env_t *pt, pt_db_t *db) {
    alpm_list_t *i;
    struct archive *a = archive_write_new();
    struct archive_entry *e = archive_entry_new();
    char path[PATH_MAX];
    int fd;

    pt_mkdirat(pt->dbfd, 0755, "sync/");
    sprintf(path, "sync/%s.db", db->name);
    fd = openat(pt->dbfd, path, O_CREAT | O_WRONLY, 0644);

    archive_write_set_format_ustar(a);
    archive_write_open_fd(a, fd);
    for(i = db->pkgs; i; i = i->next) {
        pt_pkg_t *pkg = i->data;
        size_t buflen = 0;
        char *buf;
        FILE *f;

        sprintf(path, "%s-%s/", pkg->name, pkg->version);
        archive_entry_clear(e);
        archive_entry_set_pathname(e, path);
        archive_entry_set_filetype(e, AE_IFDIR);
        archive_entry_set_perm(e, 0755);
        archive_write_header(a, e);

        f = open_memstream(&buf, &buflen);
        _pt_fwrite_dblist(f, "DEPENDS", pkg->depends);
        _pt_fwrite_dblist(f, "CONFLICTS", pkg->conflicts);
        _pt_fwrite_dblist(f, "PROVIDES", pkg->provides);
        _pt_fwrite_dblist(f, "OPTDEPENDS", pkg->optdepends);
        _pt_fwrite_dblist(f, "MAKEDEPENDS", pkg->makedepends);
        _pt_fwrite_dblist(f, "CHECKDEPENDS", pkg->checkdepends);
        fclose(f);

        sprintf(path, "%s-%s/depends", pkg->name, pkg->version);
        archive_entry_clear(e);
        archive_entry_set_pathname(e, path);
        archive_entry_set_filetype(e, AE_IFREG);
        archive_entry_set_perm(e, 0644);
        archive_entry_set_size(e, buflen);
        archive_write_header(a, e);
        archive_write_data(a, buf, buflen);
        free(buf);

        f = open_memstream(&buf, &buflen);
        _pt_fwrite_dbentry(f, "FILENAME", pkg->filename);
        _pt_fwrite_dbentry(f, "NAME", pkg->name);
        _pt_fwrite_dbentry(f, "ARCH", pkg->arch);
        _pt_fwrite_dbentry(f, "BASE", pkg->base);
        _pt_fwrite_dbentry(f, "VERSION", pkg->version);
        _pt_fwrite_dbentry(f, "DESC", pkg->desc);
        _pt_fwrite_dblist(f, "GROUPS", pkg->groups);
        _pt_fwrite_dbentry(f, "CSIZE", "200");
        _pt_fwrite_dbentry(f, "ISIZE", pkg->isize);
        fclose(f);

        sprintf(path, "%s-%s/desc", pkg->name, pkg->version);
        archive_entry_clear(e);
        archive_entry_set_pathname(e, path);
        archive_entry_set_filetype(e, AE_IFREG);
        archive_entry_set_perm(e, 0644);
        archive_entry_set_size(e, buflen);
        archive_write_header(a, e);
        archive_write_data(a, buf, buflen);
        free(buf);
    }

    archive_entry_free(e);
    archive_write_free(a);
    close(fd);
    return 0;
}

int pt_add_pkg_to_localdb(pt_env_t *pt, pt_pkg_t *pkg) {
    alpm_list_t *i;
    char path[PATH_MAX] = "";
    int fd;
    FILE *f;

    snprintf(path, PATH_MAX, "local/%s-%s", pkg->name, pkg->version);
    pt_mkdirat(pt->dbfd, 0700, path);

    /* TODO: write mtree file */

    snprintf(path, PATH_MAX, "local/%s-%s/files", pkg->name, pkg->version);
    fd = openat(pt->dbfd, path, O_CREAT | O_WRONLY, 0644);
    f = fdopen(fd, "w");
    fprintf(f, "%%%s%%\n", "FILES");
    for(i = pkg->files; i; i = i->next) {
        /* TODO: fill in parent directories? */
        pt_pkg_file_t *file = i->data;
        fprintf(f, "%s\n", file->path);
    }
    fputc('\n', f);
    fprintf(f, "%%%s%%\n", "BACKUP");
    for(i = pkg->backup; i; i = i->next) {
        pt_pkg_file_t *file = i->data;
        fprintf(f, "%s\n", file->path);
    }
    fputc('\n', f);
    fclose(f);

    snprintf(path, PATH_MAX, "local/%s-%s/desc", pkg->name, pkg->version);
    fd = openat(pt->dbfd, path, O_CREAT | O_WRONLY, 0644);
    f = fdopen(fd, "w");
    _pt_fwrite_dbentry(f, "FILENAME", pkg->filename);
    _pt_fwrite_dbentry(f, "NAME", pkg->name);
    _pt_fwrite_dbentry(f, "BASE", pkg->base);
    _pt_fwrite_dbentry(f, "VERSION", pkg->version);
    _pt_fwrite_dbentry(f, "DESC", pkg->desc);
    _pt_fwrite_dbentry(f, "CSIZE", pkg->csize);
    _pt_fwrite_dbentry(f, "ISIZE", pkg->isize);
    _pt_fwrite_dblist(f, "GROUPS", pkg->groups);
    _pt_fwrite_dblist(f, "DEPENDS", pkg->depends);
    _pt_fwrite_dblist(f, "CONFLICTS", pkg->conflicts);
    _pt_fwrite_dblist(f, "PROVIDES", pkg->provides);
    _pt_fwrite_dblist(f, "OPTDEPENDS", pkg->optdepends);
    _pt_fwrite_dblist(f, "MAKEDEPENDS", pkg->makedepends);
    _pt_fwrite_dblist(f, "CHECKDEPENDS", pkg->checkdepends);
    fclose(f);

    return 0;
}

pt_pkg_file_t *pt_pkg_add_file(pt_pkg_t *pkg, const char *path, const char *contents) {
    pt_pkg_file_t *f;
    if((f = calloc(sizeof(pt_pkg_file_t), 1)) == NULL) { return NULL; }
    if((f->path = strdup(path)) == NULL) { _pt_pkg_file_free(f); return NULL; }
    if((f->contents = strdup(contents)) == NULL) { _pt_pkg_file_free(f); return NULL; }
    pkg->files = alpm_list_add(pkg->files, f);
    return f;
}

pt_pkg_file_t *pt_pkg_add_symlink(pt_pkg_t *pkg, const char *path, const char *dest) {
    pt_pkg_file_t *f;
    if((f = calloc(sizeof(pt_pkg_file_t), 1)) == NULL) { return NULL; }
    if((f->path = strdup(path)) == NULL) { _pt_pkg_file_free(f); return NULL; }
    if((f->contents = strdup(dest)) == NULL) { _pt_pkg_file_free(f); return NULL; }
    f->type = PT_FTYPE_SYMLINK;
    pkg->files = alpm_list_add(pkg->files, f);
    return f;
}

pt_pkg_file_t *pt_pkg_add_dir(pt_pkg_t *pkg, const char *path) {
    pt_pkg_file_t *f;
    if((f = calloc(sizeof(pt_pkg_file_t), 1)) == NULL) { return NULL; }
    if((f->path = strdup(path)) == NULL) { _pt_pkg_file_free(f); return NULL; }
    f->type = PT_FTYPE_DIRECTORY;
    pkg->files = alpm_list_add(pkg->files, f);
    return f;
}

int pt_install_pkg(pt_env_t *pt, pt_pkg_t *pkg) {
    alpm_list_t *i;
    pt_add_pkg_to_localdb(pt, pkg);
    for(i = pkg->files; i; i = i->next) {
        pt_pkg_file_t *f = i->data;
        switch(f->type) {
            case PT_FTYPE_FILE:
                pt_writeat(pt->rootfd, f->path, f->contents);
                break;
            case PT_FTYPE_SYMLINK:
                pt_symlinkat(pt->rootfd, f->path, f->contents);
                break;
            case PT_FTYPE_DIRECTORY:
                pt_mkdirat(pt->rootfd, 0755, f->path);
                break;
        }
    }
    return 0;
}

void pt_cleanup(pt_env_t *pt) {
    if(pt == NULL) { return; }
    close(pt->rootfd);
    pt_rmrfat(AT_FDCWD, pt->root);
    free(pt->root);
    free(pt->dbpath);
    alpm_release(pt->handle);
    alpm_list_free_inner(pt->dbs, (alpm_list_fn_free)_pt_db_free);
    alpm_list_free_inner(pt->pkgs, (alpm_list_fn_free)_pt_pkg_free);
    alpm_list_free(pt->dbs);
    alpm_list_free(pt->pkgs);
    free(pt);
}

pt_db_t *pt_db_new(pt_env_t *pt, const char *dbname) {
    pt_db_t *db = calloc(sizeof(pt_db_t), 1);
    db->name = strdup(dbname);
    pt->dbs = alpm_list_add(pt->dbs, db);
    return db;
}

pt_pkg_t *pt_pkg_new(pt_env_t *pt, const char *pkgname, const char *pkgver) {
    pt_pkg_t *pkg = calloc(sizeof(pt_pkg_t), 1);
    pkg->name = strdup(pkgname);
    pkg->version = strdup(pkgver);
    pkg->arch = strdup("any");
    if(pt != NULL) { pt->pkgs = alpm_list_add(pt->pkgs, pkg); }
    return pkg;
}

void pt_sarray_cat(const char **sarray, ...) {
    size_t idx = 0;
    va_list ap;
    va_start(ap, sarray);
    while(sarray[idx] != NULL) { idx++; }
    while((sarray[idx++] = va_arg(ap, const char *)) != NULL);
    va_end(ap);
}

void pt_sarray_cpy(const char **sarray, size_t idx, ...) {
    va_list ap;
    va_start(ap, idx);
    while((sarray[idx++] = va_arg(ap, const char *)) != NULL);
    va_end(ap);
}

int pt_fexecve(int fd, char *const argv[], char *const envp[],
        int cwd, FILE *in, FILE *out, FILE *err) {
    int opipe[2], epipe[2], ipipe[2];
    pid_t pid;
    if( pipe(opipe) || pipe(epipe) || pipe(ipipe) ) { return -1; }

#define _PT_CLOSE(fd) while(close(fd) == -1 && errno == EINTR)
#define _PT_DUP(oldfd, newfd) while(dup2(oldfd, newfd) == -1 && errno == EINTR)
#define _PT_MAX(a, b) ( a > b ? a : b)
#define _PT_FD_SET(f, fd, s) if(f) { FD_SET(fd, s); } else { FD_CLR(fd, s); }

    if((pid = fork()) == -1) {
        return -1;
    } else if(pid == 0) {
        /* child */
        _PT_DUP(ipipe[0], STDIN_FILENO);
        _PT_DUP(opipe[1], STDOUT_FILENO);
        _PT_DUP(epipe[1], STDERR_FILENO);

        _PT_CLOSE(ipipe[0]);
        _PT_CLOSE(ipipe[1]);
        _PT_CLOSE(opipe[0]);
        _PT_CLOSE(opipe[1]);
        _PT_CLOSE(epipe[0]);
        _PT_CLOSE(epipe[1]);

        if(cwd >= 0 && fchdir(cwd) != 0) { return 0; }

        fexecve(fd, argv, envp);

        return -1;
    } else {
        /* parent */
        int status, nfds = _PT_MAX(_PT_MAX(opipe[0], epipe[0]), ipipe[1]) + 1;
        FILE *obak = out, *ebak = err; /* save values for flush */
        fd_set readfds, writefds;

        _PT_CLOSE(ipipe[0]);
        _PT_CLOSE(opipe[1]);
        _PT_CLOSE(epipe[1]);

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        _PT_FD_SET(in, ipipe[1], &writefds);
        FD_SET(epipe[0], &readfds);
        FD_SET(opipe[0], &readfds);

        while(select(nfds, &readfds, &writefds, NULL, NULL) > 0) {
            size_t r;
            char buf[LINE_MAX];

            if(FD_ISSET(ipipe[1], &writefds)) {
                r = fread(buf, 1, 1, in);
                printf("writing %c %zd...\n", buf[0], r);
                if(r < 1 || write(ipipe[1], buf, 1) < 0) { in = NULL; }
            }
            if(FD_ISSET(opipe[0], &readfds)) {
                r = read(opipe[0], buf, LINE_MAX);
                if(r && out) {
                    if( fwrite(buf, 1, r, out) == 0) { out = NULL; }
                    else { fflush(out); }
                }
            }
            if(FD_ISSET(epipe[0], &readfds)) {
                r = read(epipe[0], buf, LINE_MAX);
                if(r && err) {
                    if(fwrite(buf, 1, r, err) == 0) { err = NULL; }
                    else { fflush(err); }
                }
            }

            _PT_FD_SET(in, ipipe[1], &writefds);
            FD_SET(epipe[0], &readfds);
            FD_SET(opipe[0], &readfds);

            if(waitpid(pid, &status, WNOHANG) != 0) {
                /* slurp any remaining input and break */
                while(out && (r = read(epipe[0], buf, LINE_MAX)) > 0) {
                    if(fwrite(buf, 1, r, out) == 0) { out = NULL; }
                }
                while(err && (r = read(epipe[0], buf, LINE_MAX)) > 0) {
                    if(fwrite(buf, 1, r, err) == 0) { err = NULL; }
                }
                break;
            }
        }

        if(obak) { fflush(obak); }
        if(ebak) { fflush(ebak); }

        _PT_CLOSE(ipipe[1]);
        _PT_CLOSE(opipe[0]);
        _PT_CLOSE(epipe[0]);

        return WEXITSTATUS(status);
    }

#undef _PT_MAX
#undef _PT_FD_SET
#undef _PT_CLOSE
#undef _PT_DUP
}

int pt_execve(const char *path, char *const argv[], char *const envp[],
        int cwd, FILE *in, FILE *out, FILE *err) {
    int fd = open(path, O_RDONLY);
    int ret = pt_fexecve(fd, argv, envp, cwd, in, out, err);
    close(fd);
    return ret;
}

/*****************************************
 * ALPM helpers
 *****************************************/

alpm_db_t *pt_alpm_get_db(alpm_handle_t *h, const char *dbname) {
    alpm_list_t *i;
    for(i = alpm_get_syncdbs(h); i; i = i->next) {
        if(strcmp(alpm_db_get_name(i->data), dbname) == 0) { return i->data; }
    }
    return NULL;
}

alpm_pkg_t *pt_alpm_get_pkg(alpm_handle_t *h, const char *pkgname) {
    char *c = strchr(pkgname, '/');
    if(c && strncmp(pkgname, "local", c - pkgname) == 0) {
        alpm_db_t *db = alpm_get_localdb(h);
        return alpm_db_get_pkg(db, c + 1);
    } else if(c) {
        alpm_list_t *i;
        for(i = alpm_get_syncdbs(h); i; i = i->next) {
            if(strncmp(alpm_db_get_name(i->data), pkgname, c - pkgname) == 0) {
                return alpm_db_get_pkg(i->data, c + 1);
            }
        }
    } else {
        alpm_list_t *i;
        for(i = alpm_get_syncdbs(h); i; i = i->next) {
            alpm_pkg_t *p = alpm_db_get_pkg(i->data, pkgname);
            if(p) { return p; }
        }
    }
    return NULL;
}

/*****************************************
 * Tests
 *****************************************/

int pt_test_pkg_installed(pt_env_t *pt, pt_pkg_t *pkg) {
    struct stat buf;
    char dbpath[PATH_MAX];
    snprintf(dbpath, PATH_MAX, "local/%s-%s", pkg->name, pkg->version);
    return fstatat(pt->dbfd, dbpath, &buf, AT_SYMLINK_NOFOLLOW) == 0
        && S_ISDIR(buf.st_mode);
}

char *pt_test_pkg_version(pt_env_t *pt, const char *pkgname) {
    static char version[PATH_MAX];
    int dd = openat(pt->dbfd, "local", O_DIRECTORY);
    DIR *dirp = fdopendir(dd);
    struct dirent entry, *result;

    version[0] = '\0';

    while(readdir_r(dirp, &entry, &result) == 0 && result) {
        char *c, *dname = entry.d_name;
        for(c = dname + strlen(dname); c > dname && *c != '-'; c--);
        for(c--; c > dname && *c != '-'; c--);
        if(c > dname && strncmp(dname, pkgname, c - dname) == 0) {
            strcpy(version, c + 1);
            break;
        }
    }

    closedir(dirp);
    return version[0] == '\0' ? NULL : version;
}

int pt_test_file_exist(pt_env_t *pt, const char *path) {
    struct stat buf;
    return fstatat(pt->rootfd, path, &buf, AT_SYMLINK_NOFOLLOW) == 0;
}

#endif /* PACTEST_C */
