#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QImage>
#include <cstdlib>
#define private public
#include "reader_store.h"
#undef private
static void expect(bool ok, const char *msg) { if (!ok) { qCritical("%s", msg); std::exit(1); } }
static void write(const QString &path, const QByteArray &data) { QFile f(path); expect(f.open(QIODevice::WriteOnly), "fixture write failed"); expect(f.write(data)==data.size(), "fixture incomplete"); }
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    expect(dir.isValid(), "temp dir failed");
    qputenv("RM_WEREAD_DATA_DIR", dir.path().toUtf8());
    const QString base=dir.filePath("books/fixture");
    QDir().mkpath(base+"/epub-expanded/OEBPS/text");
    write(base+"/full.epub", "fixture");
    write(base+"/epub-expanded/OEBPS/content.opf", "<package/>");
    QByteArray chapter="<html><head><title>Fixture</title></head><body><h1>Chapter</h1>";
    for(int i=0;i<150;i++) chapter+="<p>Temporary body text for opening performance and cache equivalence.</p>";
    QDir().mkpath(base+"/epub-expanded/OEBPS/images");
    QImage image(12,20,QImage::Format_RGB32); image.fill(Qt::white);
    expect(image.save(base+"/epub-expanded/OEBPS/images/probe.png"), "image fixture failed");
    chapter+="<img src=\"../images/probe.png\"/></body></html>";
    for(int i=0;i<40;i++) write(base+QString("/epub-expanded/OEBPS/text/%1.xhtml").arg(i,3,10,QChar('0')),chapter);
    ReaderStore first; QElapsedTimer timer; timer.start(); first.loadBook("fixture","Fixture");
    const auto cold=timer.elapsed(); const auto body=first.bodyText();
    expect(!first.chapters().isEmpty(), "fixture did not load");
    expect(QFile::exists(base+"/reader-document-v1.json"), "derived cache missing");
    ReaderStore second; timer.restart(); second.loadBook("fixture","Fixture"); const auto warm=timer.elapsed();
    expect(second.bodyText()==body && second.chapters()==first.chapters() && second.footnotes()==first.footnotes() && second.imageSources()==first.imageSources(), "cache changed document coordinates");
    expect(second.canReuseBook("fixture"), "unchanged loaded book must be reusable");
    QVariantMap window;
    window.insert("starts",QVariantList{0,10});window.insert("images",QVariantList{"",""});window.insert("page",1);
    second.savePageWindow("fixture","font42",window);
    expect(!second.loadPageWindow("fixture","font42",10).isEmpty(),"saved page window did not restore");
    expect(second.loadPageWindow("fixture","font46",10).isEmpty(),"changed typography reused page window");
    expect(second.loadPageWindow("fixture","font42",11).isEmpty(),"different reading offset reused page window");
    write(base+"/reader-window-v1.json","broken");
    expect(second.loadPageWindow("fixture","font42",10).isEmpty(),"corrupt page window was accepted");
    second.savePageWindow("fixture","font42",window);
    const auto fingerprint=second.documentFingerprint(base+"/epub-expanded", second.chapterPaths(base+"/epub-expanded"));
    write(base+"/reader-document-v1.json", "broken");
    expect(!second.restoreDocumentCache("fixture",fingerprint), "corrupt cache accepted");
    second.loadBook("fixture","Fixture"); expect(second.bodyText()==body, "corrupt cache did not rebuild");
    chapter.replace("Temporary", "Different");
    write(base+"/epub-expanded/OEBPS/text/000.xhtml",chapter);
    expect(!second.canReuseBook("fixture"), "modified source must reject in-memory resume");
    ReaderStore changed; changed.loadBook("fixture","Fixture");
    expect(changed.loadPageWindow("fixture","font42",10).isEmpty(),"changed source reused page window");
    expect(changed.bodyText()!=body, "source change reused stale cache");
    qInfo("document-cache-test passed cold-ms=%lld warm-ms=%lld",cold,warm);
}
