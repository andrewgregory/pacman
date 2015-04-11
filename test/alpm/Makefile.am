check_PROGRAMS = \
	alpm_add_remove.t \
	alpm_filelist.t \
	alpm_initialize.t \
	alpm_list.t \
	alpm_list_append.t \
	alpm_list_shift.t \
	cached_part_file.t \
	remote_sync.t

noinst_PROGRAMS = $(check_PROGRAMS)

EXTRA_DIST = \
	$(check_PROGRAMS)

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-I$(top_srcdir)/lib/libalpm \
	-DLOCALEDIR=\"@localedir@\"

AM_CFLAGS = -g -D_GNU_SOURCE -pthread

LDADD = $(LTLIBINTL) $(top_builddir)/lib/libalpm/.libs/libalpm.la

%.t: %.c

# vim:set noet:
