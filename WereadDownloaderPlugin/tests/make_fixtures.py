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
