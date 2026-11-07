"""Generate synthetic, temporary EPUBs; never load user books or credentials."""
import pathlib
import struct
import sys
import warnings
import zipfile

root = pathlib.Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
container = b'''<?xml version="1.0"?><container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>'''
package = '''<?xml version="1.0"?><package xmlns="http://www.idpf.org/2007/opf" xmlns:dc="http://purl.org/dc/elements/1.1/" version="2.0" unique-identifier="original"><metadata><dc:identifier id="original">synthetic-publisher-id</dc:identifier><dc:title>Synthetic fixture</dc:title><dc:language>zh</dc:language>{extra}</metadata><manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="{spine}"/></spine></package>'''

def make(name, *, extra="", spine="chapter", missing=False, traversal=False, duplicate=False):
    path = root / (name + ".epub")
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("mimetype", b"application/epub+zip", compress_type=zipfile.ZIP_STORED)
        archive.writestr("META-INF/container.xml", container, compress_type=zipfile.ZIP_DEFLATED)
        archive.writestr("OEBPS/content.opf", package.format(extra=extra, spine=spine), compress_type=zipfile.ZIP_DEFLATED)
        if not missing:
            archive.writestr("OEBPS/chapter.xhtml", '<html xmlns="http://www.w3.org/1999/xhtml"><body><p>合成正文，仅测试。</p></body></html>', compress_type=zipfile.ZIP_DEFLATED)
        if traversal:
            archive.writestr("../escape", "synthetic")
        if duplicate:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                archive.writestr("mimetype", b"application/epub+zip")
    return path

make("valid")
make("traversal", traversal=True)
make("duplicate", duplicate=True)
make("missing-resource", missing=True)
make("bad-spine", spine="nonexistent")
make("ambiguous", extra='<dc:identifier>urn:weread:book:a</dc:identifier><dc:identifier>urn:weread:book:b</dc:identifier><meta name="weread-downloader-plugin" content="1"/>')
damaged = bytearray(make("checksum").read_bytes())
damaged[damaged.index(b"application/epub+zip")] ^= 1
(root / "checksum.epub").write_bytes(damaged)
encrypted = bytearray(make("encrypted").read_bytes())
struct.pack_into("<H", encrypted, 6, 1)
(root / "encrypted.epub").write_bytes(encrypted)

def rewrite_fixture(name, chapter, *, extra_items='', extra_files=None, extra_spine=''):
    with zipfile.ZipFile(root / 'valid.epub') as source:
        files = {n: source.read(n) for n in source.namelist()}
    files['OEBPS/chapter.xhtml'] = chapter.encode()
    opf = files['OEBPS/content.opf'].decode().replace('</manifest>', extra_items+'</manifest>').replace('</spine>',extra_spine+'</spine>')
    files['OEBPS/content.opf'] = opf.encode()
    files.update(extra_files or {})
    with zipfile.ZipFile(root / (name+'.epub'),'w') as target:
        for path, data in files.items():
            target.writestr(path,data,compress_type=zipfile.ZIP_STORED if path=='mimetype' else zipfile.ZIP_DEFLATED)

xhtml=lambda body: '<html xmlns="http://www.w3.org/1999/xhtml"><body>'+body+'</body></html>'
rewrite_fixture('duplicate-attributes',xhtml('<p>完整合成正文</p><img data-w="1" data-w="2"/>'))
rewrite_fixture('malformed-chapter',xhtml('<p>合成正文</div>'))
rewrite_fixture('missing-inline-image',xhtml('<img src="images/missing.png"/>'))
rewrite_fixture('missing-inline-style',xhtml('<link rel="stylesheet" href="missing.css"/>'))
rewrite_fixture('missing-css-asset',xhtml('<p>合成正文</p>'),extra_items='<item id="style" href="style.css" media-type="text/css"/>',extra_files={'OEBPS/style.css':b'p{background:url("images/missing.png");}'})
rewrite_fixture('duplicate-spine',xhtml('<p>合成正文</p>'),extra_spine='<itemref idref="chapter"/>')
rewrite_fixture('valid-doctype','<!DOCTYPE html>'+xhtml('<p title="a &gt; b">合成正文</p>'))
rewrite_fixture('valid-links',xhtml('<a href="#part">链接</a><a href="https://example.test/">外部链接</a><p id="part">合成正文</p>'))

rewrite_fixture('missing-inline-css-image',xhtml('<p style="background:url(missing.png)">合成正文</p>'))
rewrite_fixture('missing-style-element-image',xhtml('<style>p{background:url(missing.png)}</style><p>合成正文</p>'))
