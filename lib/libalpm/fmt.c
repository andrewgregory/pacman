#include <alpm.h>

typedef void* (*_alpm_fmt_field_accessor)(void *item);
typedef int (*_alpm_fmt_field_formatter)(void *item, const char *template, const char *spec, FILE *f);

typedef struct _alpm_fmt_field_map_t {
	const char *name;
	_alpm_fmt_field_accessor accessor;
	_alpm_fmt_field_formatter formatter;
} _alpm_fmt_field_map_t;

static int _alpm_fmt_process(alpm_list_t *items, _alpm_fmt_field_map_t *fields, const char *template, FILE *f) {
	mfmt_t *mfmt = mfmt_parse(template);
	if(!mfmt) {
		return -1;
	}

	for(alpm_list_t *i = items; i; i = i->next) {
		for(size_t j = 0; j < mfmt->token_count; j++) {
			mfmt_token_t *t = mfmt->tokens[j];
			if(t->type == MFMT_TOKEN_LITERAL) {
				fputs(t->string, f);
			} else if(t->type == MFMT_TOKEN_SUBSTITUTION) {
				char *fname, *ftemplate, *fspec;
				_alpm_fmt_field_map_t *field = _alpm_fmt_lookup_field(fields, fname);
				if(!field) {
					mfmt_free(mfmt);
					return -1;
				}
				if(field->formatter(field->accessor(i->data), ftemplate, fspec, f) != 0) {
					mfmt_free(mfmt);
					return -1;
				}
			}
		}
	}

	mfmt_free(mfmt);
	return 0;
}

int _alpm_fmt_pkg(alpm_pkg_t *pkg, const char *template, const char *spec, FILE *f) {
	alpm_list_t l = { .data = pkg };
	return alpm_fmt_pkglist(&l);
}

int alpm_fmt_pkg(const char *template, FILE *f, alpm_pkg_t *pkg) {
	alpm_list_t l = { .data = pkg };
	return alpm_fmt_pkgs(template, f, &l);
}

int alpm_fmt_pkgs(const char *template, FILE *f, alpm_list_t *pkgs) {
	return _alpm_fmt_pkglist(pkgs, template, NULL, f);
}

int _alpm_fmt_str(const char *string, const char *template, const char *spec, FILE *f) {
	if(template || spec) {
		return -1;
	}
	fputs(string, f);
}

int _alpm_fmt_db(alpm_db_t *db, const char *template, const char *spec, FILE *f) {
	if(template || spec) {
		return -1;
	}
	return _alpm_fmt_str(alpm_db_get_name(db), NULL, NULL, f)
}

int _alpm_fmt_pkglist(alpm_list_t *pkgs, const char *template, const char *spec, FILE *f) {
	_alpm_fieldmap pkgfields[] = {
		{"name", alpm_pkg_get_name, _alpm_fmt_str},
		{"desc", alpm_pkg_get_desc, _alpm_fmt_str},
		{"url", alpm_pkg_get_url, _alpm_fmt_str},
		{"packager", alpm_pkg_get_packager, _alpm_fmt_str},
		{"version", alpm_pkg_get_version, _alpm_fmt_str},
		{"base", alpm_pkg_get_base, _alpm_fmt_str},
		{"filename", alpm_pkg_get_filename, _alpm_fmt_str},
		{"md5sum", alpm_pkg_get_md5sum, _alpm_fmt_str},
		{"sha256sum", alpm_pkg_get_sha256sum, _alpm_fmt_str},
		{"arch", alpm_pkg_get_arch, _alpm_fmt_str},

		/* {"licenses", alpm_pkg_get_reason, _alpm_fmt_strlist}, */
		/* {"groups", alpm_pkg_get_reason, _alpm_fmt_strlist}, */

		/* {"depends", alpm_pkg_get_depends, _alpm_fmt_deplist}, */
		/* {"optdepends", alpm_pkg_get_optdepends, _alpm_fmt_deplist}, */
		/* {"checkdepends", alpm_pkg_get_checkdepends, _alpm_fmt_deplist}, */
		/* {"makedepends", alpm_pkg_get_makedepends, _alpm_fmt_deplist}, */
		/* {"conflicts", alpm_pkg_get_conflicts, _alpm_fmt_deplist}, */
		/* {"provides", alpm_pkg_get_provides, _alpm_fmt_deplist}, */
		/* {"replaces", alpm_pkg_get_replaces, _alpm_fmt_deplist}, */

		/* {"origin", alpm_pkg_get_origin, _alpm_fmt_pkgfrom}, */

		/* {"builddate", alpm_pkg_get_builddate, _alpm_fmt_timestamp}, */
		/* {"installdate", alpm_pkg_get_installdate, _alpm_fmt_timestamp}, */

		/* {"reason", alpm_pkg_get_reason, _alpm_fmt_pkgreason}, */

		/* {"files", alpm_pkg_get_files, _alpm_fmt_filelist}, */

		/* {"backup", alpm_pkg_get_backup, _alpm_fmt_backuplist}, */

		{"db", alpm_pkg_get_db, _alpm_fmt_db},

		/* {"validation", alpm_pkg_get_validation, _alpm_fmt_validation}, */

		/* {"size", alpm_pkg_get_size, _alpm_fmt_offset}, */
		/* {"isize", alpm_pkg_get_isize, _alpm_fmt_offset}, */
		/* {"downloadsize", alpm_pkg_get_download_size, _alpm_fmt_offset}, */

		{NULL, NULL, NULL},
	};
	template = template ? template : "{name}";

	return _alpm_fmt_process(pkgfields, pkg, template, spec, f);
}
