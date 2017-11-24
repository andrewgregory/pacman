self.description = "Sysupgrade with ignored package prevent other upgrade"

lp1 = pmpkg("glibc", "1.0-1")
lp2 = pmpkg("gcc-libs", "1.0-1")
lp2.depends = ["glibc>=1.0-1"]
lp3 = pmpkg("pcre", "1.0-1")
lp3.depends = ["gcc-libs"]

for p in lp1, lp2, lp3:
	self.addpkg2db("local", p)

sp1 = pmpkg("glibc", "1.0-2")
sp2 = pmpkg("gcc-libs", "1.0-2")
sp2.depends = ["glibc>=1.0-2"]
sp3 = pmpkg("pcre", "1.0-2")
sp3.depends = ["gcc-libs"]

for p in sp1, sp2, sp3:
	self.addpkg2db("sync", p)

self.args = "-Su --ignore %s --ask=16" % sp1.name

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_VERSION=glibc|1.0-1")
self.addrule("PKG_VERSION=gcc-libs|1.0-1")
self.addrule("PKG_VERSION=pcre|1.0-2")
self.expectfailure = True
