import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const qml=fs.readFileSync('apps/weread-qt/Main.qml','utf8');
function method(name){const start=qml.indexOf(`function ${name}(`);assert(start>=0);let end=qml.indexOf('{',start)+1,depth=1;while(depth){if(qml[end]==='{')depth++;if(qml[end]==='}')depth--;end++;}return qml.slice(start,end);}
let inkLoads=0, clears=0, formats=0, boxes=0;
const root={readerInkEnabled:false,readerPerfBaseline:false,selfTestMode:'',currentBookId:'fixture',readerCachedPageCount:3,pageIndex:1,readerPageStarts:[0,10,20],readerPageImages:[],readerTextTopMargin:0,
 clamp:(v,a,b)=>Math.max(a,Math.min(b,v)),readerImageEntrySource:()=>'',readerImageEntryCaption:()=>'',readerImageEntriesFor:()=>[],
 readerNextPageOffset(){throw new Error('cached page must not repaginate');},formatReaderText(text){formats++;return `html:${text}`;},readerLineBoxesForText(){boxes++;return [];}};
const readerStore={bodyText:'abcdefghijklmnopqrstuvwx',footnotes:[],loadStrokesForPage(){inkLoads++;}};
const context=vm.createContext({root,readerStore,readerInkCanvas:{clearLive(){clears++;}}});
for(const name of ['readerInkStoreAllowed','loadReaderInkForCurrentPage','prepareReaderInkPageChange','beginStylusStroke','appendStylusStroke','endStylusStroke','refreshReaderPageCache']){vm.runInContext(method(name),context);root[name]=context[name];}
assert.equal(root.readerInkStoreAllowed(),false);
root.beginStylusStroke(10,20,1);root.appendStylusStroke(11,21,1);root.endStylusStroke(12,22,1);
root.refreshReaderPageCache();
assert.equal(root.readerRenderedPageHtml,'html:klmnopqrst');
assert.equal(formats,1);assert.equal(inkLoads,0);assert.equal(clears,0);assert.equal(boxes,0);
readerStore.footnotes=[{textStart:12}];root.refreshReaderPageCache();
assert.equal(boxes,1,'footnote hit mapping must remain available with handwriting disabled');
assert.equal(inkLoads,0);
console.log('reader no-ink cached page behavior ok');
const baselineExpression=qml.slice(qml.indexOf('readonly property bool readerPerfBaseline:')+'readonly property bool readerPerfBaseline:'.length,qml.indexOf('readonly property bool readerInkEnabled:')).trim();
const baselineContext=vm.createContext({root:{selfTestMode:''},runtimeReaderPerfBaseline:true});
assert.equal(vm.runInContext(`(${baselineExpression})`,baselineContext),false,'diagnostic baseline flag must never enable ink in normal reading');
baselineContext.root.selfTestMode='exit-ui';assert.equal(vm.runInContext(`(${baselineExpression})`,baselineContext),false);
baselineContext.root.selfTestMode='reader-turn-perf';assert.equal(vm.runInContext(`(${baselineExpression})`,baselineContext),true);
