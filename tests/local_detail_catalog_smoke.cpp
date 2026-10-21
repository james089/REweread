#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>
#define private public
#include "shelf_store.h"
#include "book_catalog_store.h"
#undef private
static void expect(bool v,const char*m){if(!v){qCritical("%s",m);std::exit(1);}}
static void write(const QString&p,const QByteArray&d){QFile f(p);expect(f.open(QIODevice::WriteOnly),"fixture write failed");expect(f.write(d)==d.size(),"short write");}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QTemporaryDir temp;expect(temp.isValid(),"temp failed");
 qputenv("RM_WEREAD_DATA_DIR",temp.path().toUtf8());
 QDir().mkpath(temp.filePath("books/fixture"));
 write(temp.filePath("books/fixture/full.epub"),QByteArray::fromBase64("UEsDBBQAAAAIAC2eVF0QVt+MUQAAAIIAAAARAAAAT0VCUFMvY29udGVudC5vcGazKUhMzk5MT7WzyU3My0xLLS6xs8ksSc1VyEyxVUrOSCwoSS2KNzJUUsgoSk2zVSpJrSjRz89L1avIKMnNUdLHptgIRXFJeT5CsT7CEn2YxQBQSwMEFAAAAAgALZ5UXSyez7VvAAAA3wAAAA0AAABPRUJQUy90b2MubmN4s8lLrrCzyUss800sANMB+Zl5JWCWT2JSao6dTUlqRYmdW2ZRcYmCWmJugbVCTn5yYo6NPljcRh+hMDk/ryQ1r0ShuCjZVgkkq5+fl6pXkVGSm6Okj8fs4FSgzhSiDCwpz0cYqI8wEY0J9os+yGcAUEsBAhQDFAAAAAgALZ5UXRBW34xRAAAAggAAABEAAAAAAAAAAAAAAIABAAAAAE9FQlBTL2NvbnRlbnQub3BmUEsBAhQDFAAAAAgALZ5UXSyez7VvAAAA3wAAAA0AAAAAAAAAAAAAAIABgAAAAE9FQlBTL3RvYy5uY3hQSwUGAAAAAAIAAgB6AAAAGgEAAAAA"));
 write(temp.filePath("book-status.json"),R"({"fixture":{"title":"Local title","author":"Local author","intro":"Local intro"},"cached":{"title":"Cached title","infoUpdatedAt":1}})");
 ShelfStore shelf;shelf.refreshBookDetails("fixture");
 expect(!shelf.refreshingDetails(),"downloaded detail started network helper");
 expect(shelf.detailProgress()==QStringLiteral("已显示本地信息"),"downloaded book did not use local detail");
 expect(shelf.detachedDetailBook().value("intro").toString()=="Local intro","local intro missing");
 shelf.refreshBookDetails("cached");expect(shelf.detailProgress()==QStringLiteral("已显示本地信息"),"cached metadata was ignored");
 BookCatalogStore catalog;catalog.loadCatalog("fixture","Fixture");
 expect(!catalog.running()&&catalog.chapters().size()==2,"offline catalog failed");
 expect(catalog.chapters()[0].toMap().value("title").toString()=="First & local","XML entities lost");
 expect(catalog.chapters()[1].toMap().value("chapterUid").toString()=="22","chapter UID mapping lost");
 write(temp.filePath("books/fixture/full.epub"),"broken");
 catalog.loadCatalog("fixture","Fixture");
 expect(!catalog.running()&&catalog.chapters().isEmpty(),"corrupt local book started network or leaked old catalog");
 expect(catalog.statusText().startsWith(QStringLiteral("本地目录不可用")),"corrupt local book did not fail locally");
 qInfo("local-detail-and-catalog-test passed");
}
