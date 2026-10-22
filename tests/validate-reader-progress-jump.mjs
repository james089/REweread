import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const qml=fs.readFileSync('apps/weread-qt/Main.qml','utf8');
function method(name){const start=qml.indexOf(`function ${name}(`);assert(start>=0);let end=qml.indexOf('{',start)+1,depth=1;while(depth){if(qml[end]==='{')depth++;if(qml[end]==='}')depth--;end++;}return qml.slice(start,end);}
let scheduled=0,closed=0,builds=[],turns=[];
const timer={restart(){scheduled++;}};
const root={screenName:'reader',currentBookId:'synthetic',clamp:(v,a,b)=>Math.max(a,Math.min(b,v)),closeReaderJump(){},closeReaderSettings(){closed++;this.showReaderSettings=false;},buildReaderPaginationWindowFromOffset(offset,count){builds.push([offset,count]);},setReaderPage(p){turns.push(p);},forceReaderRefresh:0};
const context=vm.createContext({root,readerStore:{bodyText:'x'.repeat(100001)},readerProgressJumpTimer:timer,Date});
for(const name of ['requestReaderProgressJump','setReaderProgressPercent','goToReaderTextOffset']){vm.runInContext(method(name),context);root[name]=context[name];}
root.requestReaderProgressJump(75);
assert.equal(closed,1);assert.equal(scheduled,1);assert.equal(timer.percent,75);assert.equal(builds.length,0,'hide panel before doing pagination');
root.requestReaderProgressJump(30);assert.equal(timer.percent,30,'coalesce latest target');
root.setReaderProgressPercent(timer.percent);assert.deepEqual(builds,[[30000,2]],'jump work must depend on target window, not distance');assert.deepEqual(turns,[0]);
root.setReaderProgressPercent(1000);assert.equal(builds.at(-1)[0],100000);
root.screenName='shelf';root.requestReaderProgressJump(90);assert.equal(scheduled,2,'ignore requests outside reader');
const slider=qml.slice(qml.indexOf('id: progressJumpSlider'),qml.indexOf('id: readerSettingsBackdrop'));
assert(slider.includes('height: 88'),'finger target must be taller than the visual bar');
assert(!slider.includes('root.setReaderProgressPercent'),'drag updates must never synchronously paginate');
assert(slider.includes('root.commitReaderJump()'),'release must commit the preview');
console.log('reader progress jump behavior ok');

vm.runInContext(method('readerNearestSettingValue'),context);
assert.equal(context.readerNearestSettingValue(64,[48,72,104,136]),72,'legacy default margin must have a visible standard preset');
for(let value=0;value<=100;value++){
 const levels=[0,10,20,40,60,80,100];
 const chosen=context.readerNearestSettingValue(value,levels);
 assert.equal(levels.filter(x=>x===chosen).length,1,'brightness must select exactly one preset');
}

root.readerProgressTextLength=100001;
context.readerStore.chapters=[{textStart:0,title:'开篇'},{textStart:50000,title:'第二章'},{textStart:80000,title:'第三章'}];
vm.runInContext(method('readerProgressPreviewTitle'),context);
assert.equal(context.readerProgressPreviewTitle(0),'开篇');
assert.equal(context.readerProgressPreviewTitle(50),'第二章');
assert.equal(context.readerProgressPreviewTitle(79),'第二章');
assert.equal(context.readerProgressPreviewTitle(100),'第三章');
context.readerStore.chapters=[];assert.equal(context.readerProgressPreviewTitle(50),'暂无章节信息');
root.width=954;
root.readerSettingsScale=1280/1000;
for(const name of ['readerSettingsOptionWidth','readerSettingsFooterWidth','readerSettingsFooterX']){vm.runInContext(method(name),context);root[name]=context[name];}
vm.runInContext(method('readerSettingsY'),context);
vm.runInContext(method('readerSettingsH'),context);
assert(context.readerSettingsH(56)>56,'settings controls must be taller');
assert(context.readerSettingsY(788)-context.readerSettingsY(744)>=80,'network row must have breathing room');
const settings=qml.slice(qml.indexOf('id: readerSettingsPanel'),qml.indexOf('id: catalogCloseGestureArea'));
assert(!settings.includes('root.rebuildReaderPagination()'),'settings taps must not block on full pagination');
assert(!settings.includes('id: progressJumpSlider'),'jump slider must live outside settings');
assert(qml.includes('id: readerJumpPreviewCard'),'show actual target page preview');
context.readerSettingsPanel={x:0,y:416,width:954,height:1280};
context.readerFontPicker={visible:false};
context.Font={DemiBold:600,Bold:700};
Object.assign(root,{showReaderSettings:true,readerLineHeightSteps:[],readerParagraphSpacingSteps:[],applyReaderSettingChange(){}});
for(const name of ['readerPointInRect','handleReaderSettingsStylusTap']){vm.runInContext(method(name),context);root[name]=context[name];}
const penY=416+context.readerSettingsY(440)+context.readerSettingsH(56)/2;
assert.equal(root.handleReaderSettingsStylusTap(122+3*(root.readerSettingsOptionWidth(4,12)+12)+root.readerSettingsOptionWidth(4,12)/2,penY),true);
assert.equal(root.readerMargin,136,'pen hit mapping must follow enlarged margin controls');

const marginRow=settings.slice(settings.lastIndexOf('Row {',settings.indexOf('id: readerMarginOptions')),settings.indexOf('y: root.readerSettingsY(548)'));
assert(marginRow.includes('width: root.readerSettingsOptionWidth(4, 12)') && marginRow.includes('height: root.readerSettingsH(56)') && marginRow.includes('radius: height / 2') && marginRow.includes('spacing: 12'),'margin pills must match spacing rows');
assert(!settings.includes('text: "首行"'),'remove indent selector');
assert(qml.includes('readonly property int readerFirstLineIndentChars: 2') && !qml.includes('root.readerFirstLineIndentChars ='),'old preferences cannot override fixed two-character indentation');
const footerBottom=context.readerSettingsY(798)+context.readerSettingsH(42);
assert(footerBottom<=1280-context.readerSettingsH(20),'footer needs safe bottom padding');

for (const width of [720,954,1200]) {
 root.width=width;
 for (const [count,gap] of [[4,12],[2,12],[7,10],[3,0]])
  assert(Math.abs(122+root.readerSettingsOptionWidth(count,gap)*count+(count-1)*gap-(width-24))<0.01);
 assert(Math.abs(root.readerSettingsFooterX(3)+root.readerSettingsFooterWidth()-(width-24))<0.01);
}
root.width=954;
root.readerFontWeight=600;
const weightY=416+context.readerSettingsY(620)-context.readerSettingsH(64)+context.readerSettingsH(20);
root.handleReaderSettingsStylusTap(122+root.readerSettingsOptionWidth(2,12)+12+40,weightY);
assert.equal(root.readerFontWeight,700,'new full-width weight row must have matching pen hit routing');
