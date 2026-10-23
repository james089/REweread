import fs from 'node:fs';

const worker = fs.readFileSync('WereadDownloaderPlugin/src/worker.cpp', 'utf8');
const controller = fs.readFileSync('WereadDownloaderPlugin/native/controller.cpp', 'utf8');
const qml = fs.readFileSync('WereadDownloaderPlugin/qml/DownloaderPage.qml', 'utf8');
const header = fs.readFileSync('WereadDownloaderPlugin/native/controller.h', 'utf8');
const uiTest = fs.readFileSync('WereadDownloaderPlugin/tests/controller_test.cpp', 'utf8');

if (worker.includes('"redownload-cache"') || controller.includes('redownloadCache(')
    || header.includes('redownloadCache(') || qml.includes('重新下载缓存')) {
  throw new Error('separate cache-redownload action must not be exposed');
}
if (!worker.includes('"clear-cache"') || !controller.includes('clearCache(')
    || !header.includes('clearCache(') || !qml.includes('deleteCacheButton')
    || !qml.includes('text: "删除缓存"') || !uiTest.includes('manual delete-cache action remains')) {
  throw new Error('existing manual delete-cache action must remain available');
}
const cacheButtonAt = qml.indexOf('objectName: "deleteCacheButton"');
const cacheButton = qml.slice(cacheButtonAt, qml.indexOf('onClicked: Downloader.clearCache', cacheButtonAt));
if (!cacheButton.includes('bookRow.book.importState === "removed"') || cacheButton.includes('importState === "imported"')) {
  throw new Error('delete-cache action should be visible only for removed native books');
}
console.log('manual delete-cache retained for removed books; redownload action removed');
