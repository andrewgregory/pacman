self.description = 'switch to new server after partially downloading a package (resume)'
self.require_capability("curl")

self.cachepkgs = False

# NOTE: part is not a prefix of full in order to detect whether alpm resumed or
# restarted the download with the new server
content_part   = 'foo bar'
content_full   = 'foo baz quux'
content_expect = 'foo bar quux'

pkg = pmpkg('test')
pkg.csize = len(content_full)
self.addpkg2db('sync', pkg)

pkg_url = '/' + pkg.filename()

# advertise the full content, but serve a smaller response
url_broken = self.add_simple_http_server({
    pkg_url: {
        'headers': { 'Content-Length': str(len(content_full)) },
        'body': content_part,
    },
})
url_working = self.add_simple_http_server({ pkg_url: content_full, })

self.db['sync'].option['Server'] = [ url_broken, url_working ]
self.db['sync'].syncdir = False

self.args = '-Sw test'

self.addrule('PACMAN_RETCODE=0')
self.addrule('CACHE_FCONTENTS=%s|%s' % (pkg.filename(), content_expect))
self.addrule('!CACHE_FCONTENTS=%s|%s' % (pkg.filename(), content_full))
