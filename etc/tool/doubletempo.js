#!/usr/bin/env node
/* doubletempo.js
 * Double reported tempo of a song without changing its actual playback.
 * In other words, a qnote will report half as long as it used to.
 */

const fs = require("fs");

if (process.argv.length !== 4) {
  throw new Error(`Usage: ${process.argv[0]} ${process.argv[1]} INPUT OUTPUT`);
}
const srcpath = process.argv[2];
const dstpath = process.argv[3];

console.log(`Read from ${srcpath}, write to ${dstpath}...`);

/* ------ BEGIN CONFIGURATION ----------- */

function adjustMThdDivision(input) {
  console.log(`division ${input}`);
  return input >> 1;
}

function adjustMetaTempo(input) {
  console.log(`tempo ${input}`);
  return input >> 1;
}

function adjustDelay(input) {
  return input;
}
 
/* ----- END CONFIGURATION ------------ */

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

// Must have at least 4 bytes available, returns output length.
function writeVlq(dst, dstp, n) {
  if (n < 0) throw new Error(`Integer ${n} not encodable as VLQ`);
  if (n < 0x80) {
    dst[dstp] = n;
    return 1;
  }
  if (n < 0x4000) {
    dst[dstp++] = 0x80|(n>>7);
    dst[dstp++] = n&0x7f;
    return 2;
  }
  if (n < 0x200000) {
    dst[dstp++] = 0x80|(n>>14);
    dst[dstp++] = 0x80|((n>>7)&0x7f);
    dst[dstp++] = n&0x7f;
    return 3;
  }
  if (n < 0x10000000) {
    dst[dstp++] = 0x80|(n>>21);
    dst[dstp++] = 0x80|((n>>14)&0x7f);
    dst[dstp++] = 0x80|((n>>7)&0x7f);
    dst[dstp++] = n&0x7f;
    return 4;
  }
  throw new Error(`Integer ${n} not encodable as VLQ`);
}

class Output {
  constructor() {
    this.v = Buffer.alloc(1024);
    this.a = this.v.length;
    this.c = 0;
  }
  finish() {
    const final = Buffer.alloc(this.c);
    this.v.copy(final);
    return final;
  }
  require(addc) {
    if (this.c <= this.a - addc) return;
    const na = (this.c + addc + 1024) & ~1023;
    const nv = Buffer.alloc(na);
    this.v.copy(nv);
    this.v = nv;
    this.a = na;
  }
  appendBuffer(src, srcp, srcc) {
    if (!srcp) srcp = 0;
    if (srcc === undefined) srcc = src.length - srcp;
    if (srcc < 1) return;
    this.require(srcc);
    src.copy(this.v, this.c, srcp, srcp + srcc);
    this.c += srcc;
  }
  appendUInt32BE(src) {
    this.require(4);
    this.v.writeUInt32BE(src, this.c);
    this.c += 4;
  }
  appendUInt16BE(src) {
    this.require(2);
    this.v.writeUInt16BE(src, this.c);
    this.c += 2;
  }
  appendUInt8(src) {
    this.require(1);
    this.v.writeUInt8(src, this.c);
    this.c += 1;
  }
  appendVlq(src) {
    this.require(4);
    this.c += writeVlq(this.v, this.c, src);
  }
}

const CHUNKID_MThd = 0x4d546864; 
const CHUNKID_MTrk = 0x4d54726b;

function processMThd(output, src, srcp, srcc) {
  if (srcc !== 6) throw new Error(`Unexpected length ${srcc} for MThd`);
  
  const division = src.readUInt16BE(srcp + 4);
  if ((division&0x8000) || !division) {
    throw new Error(`Unsupported division ${division} in MThd`);
  }
  const outDivision = adjustMThdDivision(division);
  if ((outDivision < 1) || (outDivision > 0x7fff)) {
    throw new Error(`Our adjustment produced illegal division ${outDivision} from ${division}`);
  }
  
  output.appendUInt32BE(CHUNKID_MThd);
  output.appendUInt32BE(6);
  output.appendBuffer(src, srcp, 4);
  output.appendUInt16BE(outDivision);
}

function processMTrk(output, src, srcp, srcc) {
  output.appendUInt32BE(CHUNKID_MTrk);
  const chunkLengthPosition = output.c;
  output.appendUInt32BE(0);
  srcc += srcp; // Change to "how far srcp can go" from "how much content"
  let status = 0;
  while (srcp < srcc) {
  
    const [delay, delayLength] = readVlq(src, srcp);
    srcp += delayLength;
    let outDelay = adjustDelay(delay);
    output.appendVlq(outDelay);
    
    let lead = src[srcp];
    let running = false;
    if (lead >= 0xf0) {
      status = 0;
      srcp++;
    } else if (lead < 0x80) {
      if (!status) throw new Error(`Missing status byte at ${srcp}`);
      lead = status;
      running = true;
    } else {
      status = lead;
      srcp++;
    }
    
    if (!running) output.appendUInt8(lead);
    
    switch (lead&0xf0) {
      case 0x80:
      case 0x90:
      case 0xa0:
      case 0xb0:
      case 0xe0: output.appendBuffer(src, srcp, 2); srcp += 2; break;
      case 0xc0:
      case 0xd0: output.appendBuffer(src, srcp, 1); srcp += 1; break;
      default: switch (lead) {
          case 0xf0: case 0xf7: {
              const [paylen, lenlen] = readVlq(src, srcp);
              srcp += lenlen;
              if (srcp > srcc - paylen) throw new Error(`Sysex overflows Mtrk`);
              output.appendVlq(paylen);
              output.appendBuffer(src, srcp, paylen);
              srcp += paylen;
            } break;
          case 0xff: {
              const type = src[srcp++];
              const [paylen, lenlen] = readVlq(src, srcp);
              srcp += lenlen;
              if (srcp > srcc - paylen) throw new Error(`Meta overflows MTrk`);
              output.appendUInt8(type);
              output.appendVlq(paylen); // even if we modify the payload, we'll keep its length constant
              if ((type === 0x51) && (paylen === 3)) {
                const tempo = (src[srcp]<<16) | (src[srcp+1]<<8) | src[srcp+2];
                const outTempo = adjustMetaTempo(tempo);
                if ((outTempo < 1) || (outTempo > 0xffffff)) {
                  throw new Error(`Our adjustment produced invalid tempo ${outTempo}`);
                }
                output.appendUInt8(outTempo >> 16);
                output.appendUInt16BE(outTempo & 0xffff);
              } else {
                output.appendBuffer(src, srcp, paylen);
              }
              srcp += paylen;
            } break;
          default: throw new Error(`Unexpected system common 0x${lead.toString(16).padStart(2,'0')}`);
        }
    }
  }
  const chunklen = output.c - chunkLengthPosition - 4;
  output.v.writeUInt32BE(chunklen, chunkLengthPosition);
}

function processUnknown(output, chunkid, src, srcp, srcc) {
  console.log(`WARNING: Keeping unknown chunk 0x${chunkid.toString(16).padStart(8, '0')}, ${srcc} bytes, verbatim`);
  output.appendUInt32BE(chunkid);
  output.appendUInt32BE(srcc);
  output.appendBuffer(src, srcp, srcc);
}

function processChunk(output, chunkid, src, srcp, srcc) {
  switch (chunkid) {
    case CHUNKID_MThd: return processMThd(output, src, srcp, srcc);
    case CHUNKID_MTrk: return processMTrk(output, src, srcp, srcc);
    default: return processUnknown(output, chunkid, src, srcp, srcc);
  }
}

const output = new Output();
const src = fs.readFileSync(srcpath);
let srcp = 0;
while (srcp < src.length) {
  const chunkid = src.readUInt32BE(srcp);
  srcp += 4;
  const chunklen = src.readUInt32BE(srcp);
  srcp += 4;
  if (srcp > src.length - chunklen) {
    throw new Error(`${srcpath}:${srcp-8}/${src.length}: chunk length ${chunklen} overflows input`);
  }
  processChunk(output, chunkid, src, srcp, chunklen);
  srcp += chunklen;
}

fs.writeFileSync(dstpath, output.finish());
