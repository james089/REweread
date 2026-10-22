import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const qml=fs.readFileSync('apps/weread-qt/Main.qml','utf8');
function method(name){let a=qml.indexOf(`function ${name}(`);assert(a>=0);let b=qml.indexOf('{',a)+1,d=1;while(d){if(qml[b]==='{')d++;if(qml[b]==='}')d--;b++;}return qml.slice(a,b);}
let now=10000,saves=[],reads=0,requests=0;
const root={screenName:'reader',currentBookId:'A',readerPaginationDirty:false,readerInteractionUntilMs:0,showReaderSettings:false,showReaderCatalog:false,showReaderJump:true,readerIndexFailed:false,readerFullPageCount:0,readerIndexRevision:0,readerFullIndex:{key:'',starts:[],images:[],cursor:0},readerJumpPercent:70,readerTextTopMargin:100,clamp:(x,a,b)=>Math.max(a,Math.min(b,x)),readerPaginationBuildSignature:()=> 'size42',readerPageEntryAt(offset){return {start:offset,end:Math.min(1000,offset+100),image:{}};},readerAppendTailImagePage(){},formatReaderText:(s,a,b)=>`${a}:${b}:${s}`,readerImageEntrySource:()=>'',readerChapterForOffset:x=>({title:x>=500?'Chapter2':'Chapter1'}),closeReaderSettings(){this.showReaderSettings=false;},closeReaderCatalog(){this.showReaderCatalog=false;},pauseReaderBackgroundWork(){this.readerInteractionUntilMs=now+750;}};
const store={bodyText:'a'.repeat(1000),title:'Synthetic',loadPageIndex(){reads++;return{};},savePageIndex(...args){saves.push(args);}};
const ctx=vm.createContext({root,readerStore:store,progressJumpInput:{pressed:false},readerJumpPreviewTimer:{running:false,start(){requests++;this.running=true;},stop(){this.running=false;}},readerProgressJumpTimer:{restart(){requests++;}},Date:{now:()=>now}});
for(const n of ['readerIndexKey','readerIndexPageForOffset','readerGlobalPageLabel','buildReaderFullIndexStep','previewReaderJumpAt','updateReaderJumpPreview','closeReaderJump','commitReaderJump']){vm.runInContext(method(n),ctx);root[n]=ctx[n];}
for(let i=0;i<10;i++)root.buildReaderFullIndexStep();
assert.equal(root.readerFullPageCount,10);assert.equal(reads,1);assert.equal(saves.length,1);assert.equal(saves[0][2].complete,true);
root.currentReaderTextStart=500;assert.equal(root.readerGlobalPageLabel(),'6/10');
root.updateReaderJumpPreview();assert.equal(root.readerJumpTarget.start,600);assert.equal(root.readerJumpTarget.page,6);assert(root.readerJumpHtml.startsWith('600:700:'));
const before=root.currentReaderTextStart;root.previewReaderJumpAt(100);assert.equal(root.currentReaderTextStart,before,'preview must not navigate');root.commitReaderJump();assert.equal(ctx.readerProgressJumpTimer.targetOffset,900);assert.equal(root.showReaderJump,false);
root.currentBookId='B';assert.equal(root.readerGlobalPageLabel(),'页码计算中','never show previous book total');
root.readerInteractionUntilMs=0;root.buildReaderFullIndexStep();assert.equal(root.readerFullPageCount,0);assert.equal(root.readerFullIndex.starts.length,1);
ctx.progressJumpInput.pressed=true;root.buildReaderFullIndexStep();assert.equal(root.readerFullIndex.starts.length,1,'no index measurement during drag');
ctx.progressJumpInput.pressed=false;root.readerInteractionUntilMs=now+50;root.buildReaderFullIndexStep();assert.equal(root.readerFullIndex.starts.length,1,'input wins over indexing');
now+=100;root.readerPaginationBuildSignature=()=> 'size46';root.buildReaderFullIndexStep();assert.equal(root.readerFullIndex.key,'B|size46');assert.equal(root.readerFullIndex.starts.length,1);
root.showReaderJump=true;root.readerJumpPercent=50;root.updateReaderJumpPreview();assert.equal(root.readerJumpTarget.start,500);assert.equal(root.readerJumpTarget.page,-1,'unindexed preview must not invent page number');
const settings=qml.slice(qml.indexOf('id: readerSettingsPanel'),qml.indexOf('id: catalogCloseGestureArea'));
assert(!settings.includes('id: progressJumpSlider'));assert(qml.includes('id: readerFooterChapter'));assert(qml.includes('id: readerFooterPages'));
console.log('reader full index, footer and page preview behavior ok');
// The independent index must use the same image boundary rules as the reader.
let image={source:'synthetic.png',textStart:50};
const imageRoot={clamp:root.clamp,readerTextTopMargin:100,readerNextPageOffset:(start,top)=>start+(top===100?100:60),readerImageForPageRange:()=>image,readerImageEntrySource:x=>x?.source||'',readerImageEntryTextStart:x=>x.textStart,readerImageTextTopFor:()=>500};
const imageContext=vm.createContext({root:imageRoot,readerStore:{bodyText:'a'.repeat(1000)}});
vm.runInContext(method('readerPageEntryAt'),imageContext);
assert.equal(imageContext.readerPageEntryAt(0).end,50,'image anchor must split the preceding page');
image={source:'synthetic.png',textStart:0};assert.equal(imageContext.readerPageEntryAt(0).end,60,'image height must reduce body budget');
image={};assert.equal(imageContext.readerPageEntryAt(0).end,100,'text-only page must retain its full budget');

root.showReaderJump=false;root.readerSuppressPageTurnUntilMs=0;root.closeReaderJump();
assert.equal(root.readerSuppressPageTurnUntilMs,0,'closing an already closed panel must not block fresh reader input');
root.showReaderJump=true;root.closeReaderJump();assert.equal(root.readerSuppressPageTurnUntilMs,now+600,'closing a visible panel must suppress click-through');
root.currentBookId='Cached';root.readerInteractionUntilMs=0;
store.loadPageIndex=()=>({complete:true,starts:[0,500],images:[{},{}]});
root.readerPageEntryAt=()=>{throw Error('cached full index must not paginate');};
root.buildReaderFullIndexStep();assert.equal(root.readerFullPageCount,2);assert.equal(root.readerIndexPercent,100);
root.buildReaderFullIndexStep();assert.equal(root.readerFullPageCount,2);
