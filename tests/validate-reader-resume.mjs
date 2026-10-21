import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const qml=fs.readFileSync('apps/weread-qt/Main.qml','utf8');
function method(name){const start=qml.indexOf(`function ${name}(`);assert(start>=0);let at=qml.indexOf('{',start),depth=1,end=at+1;while(depth){if(qml[end]==='{')depth++;if(qml[end]==='}')depth--;end++;}return qml.slice(start,end);}
let ink=0,prefetch=0,scheduled=0,checks=0;
const root={selfTestMode:'',currentBookId:'book',screenName:'detail',readerPaginationDirty:false,readerPaginationSignature:'font42',currentReaderTextStart:120,readerPaginationBuildSignature:()=> 'font42',sampleReadingClock(){},loadReaderInkForCurrentPage(){ink++;},ensureReaderPaginationWindowAhead(){prefetch++;}};
const store={savedTextOffset:()=>120,canReuseBook(){checks++;return true;}};
const timer={restart(){scheduled++;}};
const context=vm.createContext({root,readerStore:store,deferredReaderOpenTimer:timer,console});
for(const name of ['resumeLoadedReader','openCachedReader']){vm.runInContext(method(name),context);root[name]=context[name];}
root.openCachedReader('book','Title');assert.equal(root.screenName,'reader');assert.equal(ink,1);assert.equal(scheduled,0,'warm resume must not queue loading');assert.equal(checks,1);
root.screenName='detail';root.openCachedReader('other','Other');assert.equal(root.screenName,'reader');assert.equal(root.readerOpening,true,'cold open must hide old book');assert.equal(scheduled,1);assert.equal(root.deferredReaderBookId,'other');
root.screenName='detail';root.readerPaginationDirty=true;assert.equal(root.resumeLoadedReader('book'),false,'changed layout must not resume old page');
root.readerPaginationDirty=false;store.savedTextOffset=()=>121;assert.equal(root.resumeLoadedReader('book'),false,'changed reading position must not reuse current page');
store.savedTextOffset=()=>120;store.canReuseBook=()=>false;assert.equal(root.resumeLoadedReader('book'),false,'changed source must not reuse current page');
assert(qml.includes('readerStore.loadPageWindow(bookId, root.readerPaginationBuildSignature(), savedOffset)'), 'cold open must try validated saved pagination');
assert(qml.includes('finally { root.readerOpening = false }'),'failed deferred load must release the opening overlay');
console.log('reader resume behavior ok');
