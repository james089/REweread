#include "reading_time_store.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <stdexcept>
#include <iostream>
using WereadDownloader::ReadingTimeStore;
static void check(bool ok){if(!ok)throw std::runtime_error("reading time assertion failed");}
int main(int argc,char **argv){QCoreApplication app(argc,argv);QTemporaryDir temp;check(temp.isValid());
 const QString scope(64,'a');const auto dir=temp.path()+"/accounts/"+scope;check(QDir().mkpath(dir));
 auto seconds=[&]{QFile f(dir+"/local-reading-time-v1.json");check(f.open(QIODevice::ReadOnly));return QJsonDocument::fromJson(f.readAll()).object().value("books").toObject().value("synthetic").toObject().value("seconds").toInt();};
 {
  ReadingTimeStore store(temp.path());check(store.open(scope,"synthetic","uuid","Synthetic book"));
  store.advance(5500,100);store.advance(500,101);check(store.save());check(seconds()==6);
  store.paused(true);store.advance(10000,102);check(store.save());check(seconds()==6);
  store.paused(false);store.advance(3600000,103);check(store.save());check(seconds()==6); // Missed sleep event.
  store.activity();for(int i=0;i<70;++i)store.advance(5000,104+i);check(store.save());check(seconds()==306);
  check(!store.active());store.activity();check(store.active());store.advance(5000,200);check(store.close());check(seconds()==311);
 }
 {ReadingTimeStore store(temp.path());check(store.open(scope,"synthetic","uuid","Synthetic book"));store.advance(1000,201);check(store.close());check(seconds()==312);}
 // Split using the configured IANA timezone rather than the device timezone.
 {QFile f(dir+"/local-reading-time-v1.json");check(f.open(QIODevice::WriteOnly));f.write(R"({"version":1,"books":{"synthetic":{"seconds":10}}})");}
 const QTimeZone zone("America/Chicago");check(zone.isValid());const QDate date(2026,10,22);const auto midnight=QDateTime(date.addDays(1),QTime(0,0),zone).toSecsSinceEpoch();
 check(ReadingTimeStore::dayInTimeZone(midnight*1000-1,zone)=="2026-10-22");check(ReadingTimeStore::dayInTimeZone(midnight*1000,zone)=="2026-10-23");
 {ReadingTimeStore store(temp.path());store.setTimeZone(zone);check(store.open(scope,"synthetic","uuid","Synthetic book"));store.advance(5000,midnight+2);check(store.close());}
 {QFile f(dir+"/local-reading-time-v1.json");check(f.open(QIODevice::ReadOnly));const auto row=QJsonDocument::fromJson(f.readAll()).object().value("books").toObject().value("synthetic").toObject();
  const auto days=row.value("days").toObject();check(row.value("seconds").toInt()==15 && days.value("2026-10-22").toInt()==3 && days.value("2026-10-23").toInt()==2);}
 {ReadingTimeStore store(temp.path());check(store.open(scope,"synthetic","uuid","Synthetic book"));store.advance(1000,midnight+3);check(store.close());check(seconds()==16);}
 QFile broken(dir+"/local-reading-time-v1.json");check(broken.open(QIODevice::WriteOnly));broken.write("broken");broken.close();
 {ReadingTimeStore store(temp.path());check(!store.open(scope,"synthetic","uuid","Synthetic book"));check(!store.active());}
 check(broken.open(QIODevice::ReadOnly));check(broken.readAll()=="broken");
 std::cout<<"reading time: configured timezone boundary, fractional time, pause, sleep-gap refusal, idle cutoff, restart persistence, corrupt-data refusal ok\n";
}
