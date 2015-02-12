# paths set at make time
conffile  = ${sysconfdir}/pacman.conf
dbpath    = ${localstatedir}/lib/pacman/
gpgdir    = ${sysconfdir}/pacman.d/gnupg/
cachedir  = ${localstatedir}/cache/pacman/pkg/
logfile   = ${localstatedir}/log/pacman.log
hookdir   = ${sysconfdir}/pacman.d/hooks/

bin_PROGRAMS = vercmp testpkg cleanupdelta pacman-conf

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-I$(top_srcdir)/lib/libalpm \
	-DLOCALEDIR=\"@localedir@\" \
	-DCONFFILE=\"$(conffile)\" \
	-DDBPATH=\"$(dbpath)\" \
	-DGPGDIR=\"$(gpgdir)\" \
	-DCACHEDIR=\"$(cachedir)\" \
	-DHOOKDIR=\"$(hookdir)\" \
	-DLOGFILE=\"$(logfile)\"

AM_CFLAGS = -pedantic -D_GNU_SOURCE $(WARNING_CFLAGS) \
	$(LIBARCHIVE_CFLAGS)

cleanupdelta_SOURCES = cleanupdelta.c
cleanupdelta_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

pacman_conf_SOURCES = pacman-conf.c \
	$(top_srcdir)/src/pacman/util.h \
	$(top_srcdir)/src/pacman/util.c \
	$(top_srcdir)/src/pacman/ini.h \
	$(top_srcdir)/src/pacman/ini.c \
	$(top_srcdir)/src/pacman/util-common.h \
	$(top_srcdir)/src/pacman/util-common.c \
	$(top_srcdir)/src/pacman/callback.h \
	$(top_srcdir)/src/pacman/callback.c \
	$(top_srcdir)/src/pacman/conf.h \
	$(top_srcdir)/src/pacman/conf.c
pacman_conf_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

testpkg_SOURCES = testpkg.c
testpkg_LDADD = $(top_builddir)/lib/libalpm/.libs/libalpm.la

vercmp_SOURCES = vercmp.c
vercmp_LDADD = $(top_builddir)/lib/libalpm/libalpm_la-version.lo

# vim:set noet:
