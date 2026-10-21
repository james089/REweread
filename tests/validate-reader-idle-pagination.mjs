import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const qml = fs.readFileSync('apps/weread-qt/Main.qml','utf8');
function method(name) {
 const start=qml.indexOf(`function ${name}(`); assert(start>=0);
 const opening=qml.indexOf('{',start); let depth=1,end=opening+1;
 while(depth){if(qml[end]==='{')depth++;if(qml[end]==='}')depth--;end++;}
 return qml.slice(start,end);
}
let scheduled=0, stopped=0, extensions=[];
const timer={bookId:'',signature:'',restart(){scheduled++;},stop(){stopped++;}};
const root={screenName:'reader',currentBookId:'book-a',readerPaginationDirty:false,selfTestMode:'',readerPaginationSignature:'layout-a',readerCachedPageCount:2,pageIndex:0,showReaderSettings:false,showReaderCatalog:false,annotationMode:false,pendingFreeInkStrokes:[],readerTextTopY:()=>0,readerInkStoreAllowed:()=>false,closeReaderSocialPopup(){},readerPageCountFor(){return this.readerCachedPageCount;},clamp:(v,a,b)=>Math.max(a,Math.min(b,v)),refreshReaderPageCache(){},loadReaderInkForCurrentPage(){},scheduleReaderSocialPrefetch(){},requestReaderContinuationDownload(){},extendReaderPaginationWindow(n){extensions.push(n);this.readerCachedPageCount+=n;return true;}};
const context=vm.createContext({root,paginationAheadTimer:timer,readerInkCanvas:{clearLive(){}},readerStore:{saveProgress(){}},console});
for(const name of ['ensureReaderPaginationWindowAhead','prefetchReaderPaginationPage','setReaderPage']){
 vm.runInContext(method(name),context);root[name]=context[name];
}
root.ensureReaderPaginationWindowAhead();
assert.equal(scheduled,1);assert.equal(extensions.length,0,'scheduling must not paginate synchronously');
root.setReaderPage(1);assert.equal(root.pageIndex,1);assert.equal(extensions.length,0,'cached turn must not compute extra pages');
for(let i=0;i<10;i++)root.prefetchReaderPaginationPage('book-a','layout-a');
assert.equal(root.readerCachedPageCount-root.pageIndex,6);assert(extensions.every(n=>n===1),'idle slices must be single-page');
let before=extensions.length;
root.prefetchReaderPaginationPage('old-book','layout-a');root.prefetchReaderPaginationPage('book-a','old-layout');
assert.equal(extensions.length,before,'stale callbacks must do no work');
root.readerCachedPageCount=2;root.pageIndex=0;root.annotationMode=true;
root.prefetchReaderPaginationPage('book-a','layout-a');assert.equal(extensions.length,before,'inking must not trigger prefetch');root.annotationMode=false;
root.setReaderPage(2);assert.equal(root.pageIndex,2);assert.equal(extensions.length,before+1);assert.equal(extensions.at(-1),1,'cache miss must compute only the required next page');
root.extendReaderPaginationWindow=()=>false;scheduled=0;
root.prefetchReaderPaginationPage('book-a','layout-a');assert.equal(scheduled,0,'EOF must not create a timer loop');
root.screenName='shelf';root.ensureReaderPaginationWindowAhead();assert(stopped>0);
// Exercise the real extension routine: ordered, nonduplicated pages through EOF.
const body='x'.repeat(65);
Object.assign(root,{readerPageStarts:[0,10],readerPageImages:['',''],readerCachedPageCount:2,readerTextTopMargin:0,pageIndex:0,readerImageTextTopFor:()=>0,readerNextPageOffset:s=>Math.min(body.length,s+10),readerImageEntrySource:()=>'',readerImageEntryTextStart:()=>-1,readerImageForPageRange:()=>({}),readerAppendTailImagePage:()=>false});
context.readerStore.bodyText=body;
vm.runInContext(method('extendReaderPaginationWindow'),context);
while(context.extendReaderPaginationWindow(1)){}
assert.deepEqual(Array.from(root.readerPageStarts),[0,10,20,30,40,50,60]);assert.equal(root.pageIndex,0,'prefetch must not turn the page');
console.log('reader idle pagination behavior ok');
