self.description = "Install and remove packages in  a single transaction"

lp = pmpkg('foo')
self.addpkg2db('local', lp)

sp = pmpkg("bar")
self.addpkg2db("sync", sp)

self.args = "-X --install %s --uninstall %s" % (sp.name, lp.name)

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=foo")
self.addrule("PKG_EXIST=bar")
