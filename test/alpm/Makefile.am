check_PROGRAMS = \
	cached_part_file.t \
	alpm_add_remove.t \
	alpm_list.t \
	alpm_filelist.t \
	alpm_initialize.t

noinst_PROGRAMS = $(check_PROGRAMS)

EXTRA_DIST = \
	$(check_PROGRAMS)

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-I$(top_srcdir)/lib/libalpm \
	-DLOCALEDIR=\"@localedir@\"

AM_CFLAGS = -D_GNU_SOURCE

LDADD = $(LTLIBINTL) $(top_builddir)/lib/libalpm/.libs/libalpm.la

%.t: %.c

# vim:set noet:
