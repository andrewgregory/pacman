self.description = "Install and remove packages in  a single transaction"

lp = pmpkg('foo')
self.addpkg2db('local', lp)

sp = pmpkg('bar')
sp.depends = ['foo']
self.addpkg2db('sync', sp)

sp2 = pmpkg('baz')
sp2.provides = ['foo']
self.addpkg2db('sync', sp2)

self.args = "-X --install %s --uninstall %s" % (sp.name, lp.name)

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=foo")
self.addrule("PKG_EXIST=bar")
self.addrule("PKG_EXIST=baz")
