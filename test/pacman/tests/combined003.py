self.description = "Manually replace an installed dependency"

lp = pmpkg('foo')
self.addpkg2db('local', lp)

lp2 = pmpkg('bar')
lp2.depends = ['foo']
self.addpkg2db('local', lp2)

sp = pmpkg('baz')
sp.provides = ['foo']
self.addpkg2db('sync', sp)

self.args = "-X --install %s --uninstall %s" % (sp.name, lp.name)

self.addrule("PACMAN_RETCODE=0")
self.addrule("!PKG_EXIST=foo")
self.addrule("PKG_EXIST=bar")
self.addrule("PKG_EXIST=baz")
