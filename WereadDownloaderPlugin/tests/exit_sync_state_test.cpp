#include "exit_sync_state.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <stdexcept>
#include <iostream>
using namespace WereadDownloader;
static void check(bool value){if(!value)throw std::runtime_error("exit signature assertion failed");}
int main(int argc,char **argv){QCoreApplication app(argc,argv);QTemporaryDir dir;check(dir.isValid());const auto base=dir.path()+"/synthetic";
 auto write=[&](const QString &suffix,const QByteArray &data){QFile f(base+suffix);check(f.open(QIODevice::WriteOnly));check(f.write(data)==data.size());};
 check(exitPositionSignature(base).isEmpty());
 write(".metadata",R"({"lastOpenedPage":1,"lastOpened":"100"})");write(".content",R"({"pageCount":10,"font":"a"})");write(".pdf","synthetic-pdf");
 const auto first=exitPositionSignature(base);check(first.size()==64 && exitPositionSignature(base,1)==first);
 write(".metadata",R"({"lastOpenedPage":1,"lastOpened":"200"})");check(exitPositionSignature(base)==first); // Merely reopening is not a changed position.
 check(exitPositionSignature(base,2)!=first); // A page change is not a percentage comparison.
 write(".content",R"({"pageCount":10,"font":"b"})");check(exitPositionSignature(base)!=first); // Same page in a new layout must not be skipped.
 write(".content",R"({"pageCount":10,"font":"a"})");write(".pdf","synthetic-changed-pdf");check(exitPositionSignature(base)!=first);
 write(".metadata",R"({"lastOpenedPage":1,"deleted":true})");check(exitPositionSignature(base).isEmpty());
 std::cout<<"exit signatures: unchanged opening ignored, page/layout/PDF changes detected, invalid source refused ok\n";
}
