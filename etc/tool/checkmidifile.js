#!/usr/bin/env node

const fs = require("fs");
const MidiFile = require("./MidiFile.js");

/* --tempo
 ***************************************************************/

function reportTempo(midiFile) {
  let phase = 0;
  let brokePhase = false;
  let tempo = 0;
  const phaseBuckets = [];
  for (let i=128; i-->0; ) phaseBuckets.push(0);
  for (const event of midiFile.iterateEvents()) {
    //console.log(`${event.reprLog()}`);
    if (event.isTimeMarker()) {
      phase = event.phase;
    } else if ((event.opcode === 0x90) && event.b) {
      const p = Math.floor(phase * phaseBuckets.length);
      if ((p < 0) || (p >= phaseBuckets.length)) {
        throw new Error(`oops, phase=${phase} buckets=${phaseBuckets.length} p=${p}, ${event.reprLog()}`);
      }
      phaseBuckets[p]++;
    } else if ((event.opcode === 0xff) && (event.a === 0x51)) {
      if (!event.v || (event.v.length !== 3)) throw new Error(`invalid Set Tempo event`);
      const ntempo = (event.v[0] << 16) | (event.v[1] << 8) | event.v[2];
      if (!tempo) {
        tempo=ntempo;
      } else if (tempo !== ntempo) {
        throw new Error(`Tempo change from ${tempo} to ${ntempo} us/qnote. We require a fixed tempo.`);
      }
    }
  }
  const topCount = Math.max(...phaseBuckets);
  const height = 40;
  const viewBuckets = phaseBuckets.map(c => (c*height)/topCount);
  for (let y=40; y-->0; ) {
    const line = viewBuckets.map(c => ((c >= y) ? "*" : " ")).join("");
    console.log(line);
  }
  const legend = phaseBuckets.map((c) => " ");
  for (let factor=9; factor-->2; ) {
    for (let numerator=0; numerator<factor; numerator++) {
      const p = Math.floor((numerator * legend.length) / factor);
      legend[p] = factor.toString();
    }
  }
  legend[0] = '1'; // just [0], don't show us all the multiples of 1 :)
  console.log(legend.join(""));
}

/* --programs
 ******************************************************************/
 
function reportPrograms(midiFile) {
  const usageByProgramId = [];
  for (let i=0; i<128; i++) usageByProgramId.push(false);
  for (const event of midiFile.iterateEvents()) {
    if (event.opcode === 0xc0) {
      usageByProgramId[event.a] = true;
    } else if (event.opcode === 0xb0) {
      if (event.a === 0x00) {
        console.log(`${midiFile.name}:WARNING: Ignoring Bank Select MSB =${event.b} on channel ${event.channel}`);
      } else if (event.a === 0x20) {
        console.log(`${midiFile.name}:WARNING: Ignoring Bank Select LSB =${event.b} on channel ${event.channel}`);
      }
    }
  }
  for (let pid=0; pid<128; pid++) {
    if (!usageByProgramId[pid]) continue;
    console.log(`  ${pid} ${MidiFile.GM_PROGRAM_NAMES[pid]}`);
  }
}

/* --text
 ******************************************************************/
 
function reportText(midiFile) {
  for (const event of midiFile.iterateEvents()) {
    if (event.opcode === 0xff) {
      if (event.a < 0x20) {
        console.log(`  ${event.a}: ${event.v.toString("ascii")}`);
      }
    }
  }
}

/* Main.
 ******************************************************************/
 
let checkTempo = false; // --tempo
let checkPrograms = false; // --programs
let checkText = false; // --text

function printHelp() {
  console.log(`
Usage: ${process.argv[1]} [TESTS] [FILES]

TESTS:
  --tempo        Draw a histogram of where Note On occurs relative to the qnote.
  --programs     Report all programs used by the song.
  --text         Dump all text Meta events (typically title, composer, etc).
  
Run with no tests to only decode the file for validation.
`);
}

function checkFile(path) {
  console.log(`${path}...`);
  let midiFile;
  try {
    const serial = fs.readFileSync(path);
    midiFile = new MidiFile(serial, path);
  } catch (e) {
    console.error(`${path}:ERROR: ${e.message}`);
    return;
  }
  if (checkTempo) reportTempo(midiFile);
  if (checkPrograms) reportPrograms(midiFile);
  if (checkText) reportText(midiFile);
}

for (const arg of process.argv.slice(2)) {
       if (arg === "--help") printHelp();
  else if (arg === "--tempo") checkTempo = true;
  else if (arg === "--programs") checkPrograms = true;
  else if (arg === "--text") checkText = true;
  else if (arg.startsWith("-")) throw new Error(`${process.argv[1]}: Unexpected argument '${arg}]`);
  else checkFile(arg);
}
