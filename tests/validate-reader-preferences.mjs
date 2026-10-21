import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const qml=fs.readFileSync('apps/weread-qt/Main.qml','utf8');
const start=qml.indexOf('function persistReaderPreferences(');let at=qml.indexOf('{',start),d=1,end=at+1;while(d){if(qml[end]==='{')d++;if(qml[end]==='}')d--;end++;}
const values={fontSize:50,fontChoice:'思源宋体',lineHeight:1.26,paragraphSpacing:12,fontWeight:600,firstLineIndentChars:2,margin:64};
const written=new Map();let synced=0;
const root={readerPreferencesReady:true,selfTestMode:'',captureReaderSettings:()=>values};
const context=vm.createContext({root,persistedReaderSettings:{setValue:(k,v)=>written.set(k,v),sync(){synced++;}}});
vm.runInContext(qml.slice(start,end),context);context.persistReaderPreferences();
assert.equal(written.get('fontChoice'),'思源宋体');assert.equal(written.get('fontSize'),50);assert.equal(synced,1);assert.equal(written.get('wenkaiDefaultMigrated'),true);
root.selfTestMode='reader-ink';context.persistReaderPreferences();assert.equal(synced,1,'tests must never persist user preferences');
root.selfTestMode='';root.readerPreferencesReady=false;context.persistReaderPreferences();assert.equal(synced,1,'initial property notifications must not overwrite loaded settings');
assert(qml.includes('location: runtimeSettingsUrl'),'settings must use an explicit stable location');
assert(qml.includes('root.persistReaderPreferences()'),'font changes must persist immediately');
const session=fs.readFileSync('scripts/weread-qt-session.sh','utf8');assert(session.includes('export HOME XDG_CONFIG_HOME'),'launcher must supply a stable writable configuration home');
console.log('reader preference persistence behavior ok');
