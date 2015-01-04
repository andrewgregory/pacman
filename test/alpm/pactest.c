#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <alpm.h>
#include <limits.h>

typedef struct pt_env_t {
    int rootfd;
    char *root, *dbpath;
    alpm_list_t *dbs, *pkgs;
    alpm_handle_t *handle;
} pt_env_t;

typedef struct pt_db_t {
    char *name;
    alpm_list_t *pkgs;
} pt_db_t;

typedef struct pt_pkg_t {
    char *base, *filename, *name, *version, *desc, *packager, *arch;
    alpm_list_t *files, *depends, *conflicts, *provides;
    alpm_list_t *optdepends, *makedepends, *checkdepends;
    alpm_list_t *licenses, *replaces, *groups, *backup;
    char *builddate, *installdate, *csize, *isize;
    char *scriptlet;
} pt_pkg_t;

void _pt_rmrf(const char *path) {
    if(!unlink(path)) {
        return;
    } else {
        struct dirent *de;
        DIR *d;
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

        d = opendir(path);
        if(!d) { return; }
        for(de = readdir(d); de != NULL; de = readdir(d)) {
            if(strcmp(de->d_name, "..") != 0 && strcmp(de->d_name, ".") != 0) {
                char name[PATH_MAX];
                snprintf(name, PATH_MAX, "%s/%s", path, de->d_name);
                _pt_rmrf(name);
            }
        }
        closedir(d);
        rmdir(path);
    }
}

int mkdirp(const char *path, int mode) {
    char *dir = strdup(path);
    char p[PATH_MAX] = "";
    char *c;
    int ret = 0;
    for(c = strtok(dir, "/"); c; c = strtok(NULL, "/")) {
        struct stat buf;
        strcat(p, "/");
        strcat(p, c);
        if(stat(p, &buf) == 0) { continue; }
        if((ret = mkdir(p, mode)) != 0) { break; }
    }
    free(dir);
    return ret;
}

int pt_mkdir(pt_env_t *pt, int mode, const char *path) {
    char *dir = strdup(path);
    char p[PATH_MAX] = "";
    char *c;
    int ret = 0;
    for(c = strtok(dir, "/"); c; c = strtok(NULL, "/")) {
        struct stat buf;
        strcat(p, c);
        strcat(p, "/");
        if(stat(p, &buf) == 0) { continue; }
        if((ret = mkdirat(pt->rootfd, p, mode)) != 0) { break; }
    }
    free(dir);
    return ret;
}

int pt_grep(pt_env_t *pt, const char *path, const char *needle) {
    int fd;
    char buf[LINE_MAX];
    FILE *f;
    while(path[0] == '/') { path++; }
    if((fd = openat(pt->rootfd, path, O_RDONLY)) == -1) { return 0; }
    if((f = fdopen(fd, "r")) == NULL) { return 0; }
    while(fgets(buf, sizeof(buf), f)) {
        if(strstr(buf, needle)) { fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

int pt_spew(pt_env_t *pt, const char *path, const char *contents) {
    int fd;
    char *c;
    while(path[0] == '/') { path++; }
    if((c = strrchr(path, '/'))) {
        char *dir = strndup(path, c - path);
        pt_mkdir(pt, 0700, dir);
        free(dir);
    }
    if((fd = openat(pt->rootfd, path, O_CREAT | O_WRONLY | O_TRUNC, 0700)) == -1) {
        return 0;
    }
    write(fd, contents, strlen(contents));
    close(fd);
    return 0;
}

pt_env_t *pt_new(void) {
    pt_env_t *pt;
    char tmpl[] = "/tmp/pactest-XXXXXX";
    const char *dbpath = "/var/lib/pacman/";
    if(mkdtemp(tmpl) == NULL) { return NULL; }
    if((pt = calloc(sizeof(pt_env_t), 1)) == NULL) { return NULL; }
    pt->root = strdup(tmpl);
    pt->rootfd = open(pt->root, O_DIRECTORY);
    pt->dbpath = malloc(strlen(pt->root) + strlen(dbpath) + 1);
    sprintf(pt->dbpath, "%s%s", pt->root, dbpath);
    return pt;
}

alpm_handle_t *pt_initialize(pt_env_t *pt, alpm_errno_t *err) {
    mkdirp(pt->dbpath, 0700);
    if(err) { *err = 0; }
    pt->handle = alpm_initialize(pt->root, pt->dbpath, err);
    return pt->handle;
}

void write_list(struct archive *a, const char *section, alpm_list_t *values) {
    alpm_list_t *i;
    archive_write_data(a, "%", 1);
    archive_write_data(a, section, strlen(section));
    archive_write_data(a, "\n", 2);
    for(i = values; i; i = i->next) {
        const char *buf = i->data;
        archive_write_data(a, buf, strlen(buf));
        archive_write_data(a, "\n", 1);
    }
    archive_write_data(a, "\n", 1);
}

void write_entry(struct archive *a, const char *section, const char *value) {
    alpm_list_t *i;
    archive_write_data(a, "%", 1);
    archive_write_data(a, section, strlen(section));
    archive_write_data(a, "%\n", 2);
    archive_write_data(a, value, strlen(value));
    archive_write_data(a, "\n", 1);
}

int pt_install_db(pt_env_t *pt, pt_db_t *db) {
    alpm_list_t *i;
    struct archive *a = archive_write_new();
    struct archive_entry *e;
    char dbpath[PATH_MAX];
    sprintf(dbpath, "%s/sync/", pt->dbpath);
    mkdirp(dbpath, 0700);
    strcat(dbpath, db->name);
    strcat(dbpath, ".db");
    archive_write_set_format_ustar(a);
    archive_write_open_filename(a, dbpath);
    for(i = db->pkgs; i; i = i->next) {
        pt_pkg_t *pkg = i->data;
        char fpath[PATH_MAX];

        sprintf(fpath, "%s-%s/depends", pkg->name, pkg->version);
        e = archive_entry_new();
        archive_entry_set_pathname(e, fpath);
        archive_write_header(a, e);
        write_list(a, "DEPENDS", pkg->depends);
        write_list(a, "CONFLICTS", pkg->conflicts);
        write_list(a, "PROVIDES", pkg->provides);
        write_list(a, "OPTDEPENDS", pkg->optdepends);
        write_list(a, "MAKEDEPENDS", pkg->makedepends);
        write_list(a, "CHECKDEPENDS", pkg->checkdepends);
        archive_entry_free(e);

        sprintf(fpath, "%s-%s/desc", pkg->name, pkg->version);
        e = archive_entry_new();
        archive_entry_set_pathname(e, fpath);
        archive_write_header(a, e);
        write_entry(a, "FILENAME", pkg->filename);
        write_entry(a, "NAME", pkg->name);
        write_entry(a, "BASE", pkg->base);
        write_entry(a, "VERSION", pkg->version);
        write_entry(a, "DESC", pkg->desc);
        write_list(a, "GROUPS", pkg->groups);
        write_entry(a, "CSIZE", pkg->csize);
        write_entry(a, "ISIZE", pkg->isize);
        archive_entry_free(e);
    }
    archive_write_free(a);
    return 0;
}

int pt_cache_pkg(pt_env_t *pt, pt_pkg_t *p) {
    struct archive *a;
    a = archive_write_new();
    alpm_list_t *i;
    return 0;
}

int pt_install_pkg(pt_env_t *pt, pt_pkg_t *pkg) {
    return 0;
}

void pt_cleanup(pt_env_t *pt) {
    if(pt != NULL) { 
        close(pt->rootfd);
        _pt_rmrf(pt->root);
        free(pt->root);
        free(pt->dbpath);
        alpm_release(pt->handle);
        free(pt);
    }
}
