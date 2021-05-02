#!/usr/bin/env node

const fs = require("fs");
const MidiFile = require("./MidiFile.js");

/*
function rvlq2(src, srcp) { return ((src[srcp]&0x7f)<<7)|src[srcp+1]; }
function rvlq3(src, srcp) { return ((src[srcp]&0x7f)<<14)|((src[srcp+1]&0x7f)<<7)|src[srcp+2]; }
function rvlq4(src, srcp) { return ((src[srcp]&0x7f)<<21)|((src[srcp+1]&0x7f)<<14)|((src[srcp+2]&0x7f)<<7)|src[srcp+3]; }

// => [v, len], throws if malformed
function readVlq(src, srcp) {
  if ((srcp < 0) || (srcp >= src.length)) throw new Error(`invalid vlq position`);
  const available = src.length - srcp;
  if (!(src[srcp] & 0x80)) return [src[srcp], 1];
  if (available < 2) throw new Error(`vlq overrun`);
  if (!(src[srcp+1] & 0x80)) return [rvlq2(src, srcp), 2];
  if (available < 3) throw new Error(`vlq overrun`);
  if (!(src[srcp+2] & 0x80)) return [rvlq3(src, srcp), 3];
  if (available < 4) throw new Error(`vlq overrun`);
  if (!(src[srcp+3] & 0x80)) return [rvlq4(src, srcp), 4];
  throw new Error(`invalid vlq (4 leading high bits)`);
}

class Event {

  constructor(src, srcp, status) {
    this.position = srcp;
    this.length = 0; // Raw encoded length. We throw an exception if <1
    this.delay = 0; // Ticks
    this.status = status;
    this.opcode = 0; // 0xff=Meta, 0xf0=Sysex, 0xf7=Sysex, otherwise MIDI
    this.channel = 0; // 0xff=all
    this.a = 0; // First data byte, Meta type, or zero
    this.b = 0; // Second data byte or zero
    this.v = null; // Meta or Sysex payload
    this.read(src, srcp);
  }
  
  // Read an event including the delay.
  // Populates (this) on success, throws on error.
  read(src) {
    
    const [delay, delayLength] = readVlq(src, this.position);
    this.delay = delay;
    this.length = delayLength;
    
    if (this.position >= src.length) throw new Error(`end of input`);
    let status = src[this.position + this.length];
    if (status & 0x80) { // fresh status from input
      this.status = status;
      this.length++;
    } else if (this.status & 0x80) { // use running status
      status = this.status;
    } else { // running status required, or invalid data
      throw new Error(`invalid leading byte 0x${status.toString(16).padStart(2,'0')}`);
    }
    
    // Meta, Sysex, etc
    if ((status & 0xf0) === 0xf0) {
      this.status = 0;
      this.opcode = status;
      this.channel = 0xff;
      if (status === 0xff) { // Meta
        if (this.position + this.length >= src.length) throw new Error(`Meta overruns input`);
        this.a = src[this.position + this.length++]; // type
        const [len, lenlen] = readVlq(src, this.position + this.length);
        this.length += lenlen;
        if (this.position + this.length > src.length - len) throw new Error(`Meta overruns input`);
        this.v = src.slice(this.position + this.length, this.position + this.length + len);
        this.length += len;
        
      } else if ((status === 0xf0) || (status === 0xf7)) { // Sysex
        const [len, lenlen] = readVlq(src, this.position + this.length);
        this.length += lenlen;
        if (this.position + this.length > src.length - len) throw new Error(`Sysex overruns input`);
        this.v = src.slice(this.position + this.length, this.position + this.length + len);
        this.length += len;
        
      } else {
        throw new Error(`Unexpected leading byte 0x${status.toString(16).padStart(2,'0')}`);
      }
      
    // MIDI events
    } else {
      this.opcode = status & 0xf0;
      this.channel = status & 0x0f;
      let dataCount = 0;
      switch (this.opcode) {
        case 0x80: dataCount = 2; break;
        case 0x90: dataCount = 2; break;
        case 0xa0: dataCount = 2; break;
        case 0xb0: dataCount = 2; break;
        case 0xc0: dataCount = 1; break;
        case 0xd0: dataCount = 1; break;
        case 0xe0: dataCount = 2; break;
        default: throw new Error(`Unexpected opcode ${this.opcode}`);
      }
      if (this.position > src.length - dataCount) {
        throw new Error(`Event 0x${status.toString(16).padStart(2,'0')} overruns input`);
      }
      if (dataCount >= 1) this.a = src[this.position + this.length++];
      if (dataCount >= 2) this.b = src[this.position + this.length++];
    }
  }
  
}

class MidiFile {

  constructor(path) {
    this.path = path;
    
    // MThd:
    this.format = 0;
    this.trackCount = 0;
    this.division = 0; // signals presence of MThd; zero is illegal
    
    this.mtrk = []; // Buffer
    this.unknown = []; // {id,body}
  }
  
  warn(msg) {
    console.log(`${this.path}:WARNING: ${msg}`);
  }

  processMThd(src) {
    if (this.division) throw new Error(`multiple MThd`);
    if (src.length < 6) throw new Error(`illegal MThd length ${src.length}`);
    if (src.length > 6) this.warn(`MThd length ${src.length} > 6, ignoring the tail`);
    
    this.format = (src[0]<<8)|src[1];
    this.trackCount = (src[2]<<8)|src[3];
    this.division = (src[4]<<8)|src[5];
    
    if (this.format !== 1) this.warn(`Format ${this.format}, expected 1`);
    if (!this.division) throw new Error(`MThd.division == 0`);
    if (this.division & 0x8000) this.warn(`SMPTE timing (${this.division.toString(16)})`);
  }

  // => [id, body, rlen], throws if invalid
  readChunk(src, srcp) {
    if (srcp > src.length - 8) throw new Error(`short chunk header at ${srcp}/${src.length}`);
    const id = src.slice(srcp, srcp + 4).toString();
    const len = (src[srcp+4]<<24)|(src[srcp+5]<<16)|(src[srcp+6]<<8)|src[srcp+7];
    if (len < 0) throw new Error(`invalid chunk length`);
    srcp += 8;
    if (srcp > src.length - len) throw new Error(`${id} chunk at ${srcp-8}/${src.length} overruns input`);
    return [id, src.slice(srcp, srcp + len), 8 + len];
  }
  
  read() {
    const src = fs.readFileSync(this.path);
    let srcp = 0;
    while (srcp < src.length) {
      const [id, body, rlen] = this.readChunk(src, srcp);
      switch (id) {
        case "MThd": this.processMThd(body); break;
        case "MTrk": this.mtrk.push(body); break;
        default: this.unknown.push({id, body}); break;
      }
      srcp += rlen;
    }
  }
  
  validate() {
    if (!this.division) throw new Error(`No MThd`);
    if (this.trackCount !== this.mtrk.length) {
      // I'm not at all clear on the meaning of MThd.trackCount.
      this.warn(`MThd.trackCount ${this.trackCount}, but found ${this.mtrk.length} tracks`);
    }
    if (!this.mtrk.length) throw new Error(`No MTrk`);
    
    if (this.unknown.length) {
      this.warn(`${this.unknown.length} unknown chunks:`);
      for (const [id,body] of this.unknown) {
        this.warn(`  ${id}, ${body.length} bytes`);
      }
    }
  }
  
  *iterateMTrk(src) {
    let srcp = 0;
    let status = 0;
    while (srcp < src.length) {
      let event;
      try {
        event = new Event(src, srcp, status);
      } catch (e) {
        yield {
          opcode: "EXCEPTION",
          exception: e,
        };
        return;
      }
      yield event;
      status = event.status;
      srcp += event.length;
    }
  }
  
  /* Look for tempo changes and calculate the exact time of each Note On.
   * Reports distribution of Note On timings relative to the preceding qnote interval.
   *
  reportTempo() {
    let changeCount = 0, index = 0;
    for (const mtrk of this.mtrk) {
      let time = 0;
      let sperqnote = 0.250000;
      for (const event of this.iterateMTrk(mtrk)) {
        const logpfx = () => `MTrk ${index}/${this.mtrk.length}:byte ${event.position}/${mtrk.length}:time ${time}`;
        time += event.delay * sperqnote;
        //console.log(`${this.path}: MTrk ${index}/${this.mtrk.length}: ${event.opcode.toString(16)} ${event.a.toString(16).padStart(2,'0')}`);
        if (event.opcode == 0xff) {
          switch (event.a) {
            case 0x51: { // Set Tempo
                if (event.v.length !== 3) {
                  throw new Error(`${logpfx()}: Meta Set Tempo length ${event.v.length}, expected 3`);
                }
                sperqnote = ((event.v[0]<<16)|(event.v[1]<<8)|event.v[2])/1000000.0;
                changeCount++;
                console.log(`${logpfx()}: Set Tempo ${sperqnote} s/qnote`);
              } break;
            case 0x58: { // Time Signature
              } break;
          }
        } else if (event.opcode == 0x90) { // Note On
        }
      }
      console.log(`MTrk ${index}/${this.mtrk.length} duration ${time}`);
      index++;
    }
    if (!changeCount) this.warn(`No tempo declarations`);
  }
}
/**/

function reportTempo(midiFile) {
  let phase = 0;
  let brokePhase = false;
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
    } else if (event.opcode >= 0xf0) {
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
  reportTempo(midiFile);
}

for (const path of process.argv.slice(2)) {
  checkFile(path);
}
