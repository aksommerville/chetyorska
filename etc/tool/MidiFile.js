/* MidiFile.js
 * So I wanted a quick one-off script to validate MIDI file tempos, but finding I do need a general parser.
 * Aiming to keep this as portable as possible.
 */
 
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
  constructor() {
    this.length = 0; // Raw length in input, or zero for artificial events.
    this.status = 0; // Whole status byte, possibly echoing running status, or zero if artificial.
    this.opcode = 0; // 0xff=Meta, 0xf0=Sysex, 0xf7=Sysex, 0=timeMarker
    this.channel = -1; // 0..16 if applicable to just one channel
    this.a = 0; // First data byte
    this.b = 0; // Second data byte
    this.v = null; // Meta or Sysex payload
    // We don't set these; caller should if applicable:
    this.mtrkIndex = -1; // Which MTrk chunk?
    this.mtrkCount = -1; // How many?
    this.mtrkPosition = -1; // Where in that chunk?
    this.mtrkLength = -1; // Length of that chunk
  }
  
  /* The usual case: Read an event from a file, starting *after* the delay.
   * Event is not aware of delay.
   */
  static read(src, srcp, status) {
    const event = new Event();
    event._read(src, srcp, status);
    return event;
  }
  
  /* Generate an artificial event to indicate passage of time:
   *   (a): Absolute time in ticks, regardless of tempo.
   *   (b): Absolute time in seconds, tempo-aware.
   *   (phase): 0..1, where do we land relative to the reference beat. If (division) provided.
   */
  static timeMarker(ticks, seconds, division) {
    const event = new Event();
    event.opcode = 0x00;
    event.a = ticks;
    event.b = seconds;
    if (division) event.phase = (ticks % division) / division;
    return event;
  }
  
  isTimeMarker() {
    return (this.opcode === 0x00);
  }
  
  /* Human-readable representation for logging.
   */
  reprLog() {
    let location = "";
    if (this.mtrkIndex >= 0) {
      location = `${this.mtrkIndex}/${this.mtrkCount}:${this.mtrkPosition}/${this.mtrkLength}: `;
    }
    
    if (this.opcode === 0x00) {
      const millis = Math.floor((this.b * 1000) % 1000);
      const seconds = Math.floor(this.b) % 60;
      const minutes = Math.floor(this.b / 60);
      const friendlyTime = `${minutes}:${seconds.toString().padStart(2,'0')}.${millis.toString().padStart(3,'0')}`;
      return `${location}${friendlyTime} ${this.a} ticks, phase ${this.phase}`;
    }

    if (this.opcode === 0xff) {
      let payloadDescription = this.v ? `${this.v.length} bytes` : "INVALID";
      switch (this.a) {
        case 0x51: if (this.v && (this.v.length === 3)) {
            const usperqnote = (this.v[0]<<16)|(this.v[1]<<8)|this.v[2];
            payloadDescription = `Set Tempo ${usperqnote} us/qnote`;
          } break;
        default: if (this.v && (this.a < 0x20)) { // Text events.
            payloadDescription = JSON.stringify(this.v.toString());
          } break;
      }
      return `${location}Meta ${this.a.toString(16).padStart(2,'0')}: ${payloadDescription}`;
    }
    
    if ((this.opcode === 0xf0) || (this.opcode === 0xf7)) {
      return `${location}Sysex ${this.v?this.v.length:0} bytes`;
    }
    
    return `${location}${this.status.toString(16)} ${this.a.toString(16).padStart(2,'0')} ${this.b.toString(16).padStart(2,'0')}`;
  }
    
  _read(src, srcp, runningStatus) {
    if (srcp >= src.length) throw new Error(`end of input`);
    let status = src[srcp + this.length];
    if (status & 0x80) { // fresh status from input
      this.status = status;
      this.length++;
    } else if (runningStatus & 0x80) { // use running status
      status = runningStatus;
    } else { // running status required, or invalid data
      throw new Error(`invalid leading byte 0x${status.toString(16).padStart(2,'0')}`);
    }
    
    // Meta, Sysex, etc
    if ((status & 0xf0) === 0xf0) {
      this.status = 0;
      this.opcode = status;
      this.channel = 0xff;
      if (status === 0xff) { // Meta
        if (srcp + this.length >= src.length) throw new Error(`Meta overruns input`);
        this.a = src[srcp + this.length++]; // type
        const [len, lenlen] = readVlq(src, srcp + this.length);
        this.length += lenlen;
        if (srcp + this.length > src.length - len) throw new Error(`Meta overruns input`);
        this.v = src.slice(srcp + this.length, srcp + this.length + len);
        this.length += len;
        
      } else if ((status === 0xf0) || (status === 0xf7)) { // Sysex
        const [len, lenlen] = readVlq(src, srcp + this.length);
        this.length += lenlen;
        if (srcp + this.length > src.length - len) throw new Error(`Sysex overruns input`);
        this.v = src.slice(srcp + this.length, srcp + this.length + len);
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
      if (srcp > src.length - dataCount) {
        throw new Error(`Event 0x${status.toString(16).padStart(2,'0')} overruns input`);
      }
      if (dataCount >= 1) this.a = src[srcp + this.length++];
      if (dataCount >= 2) this.b = src[srcp + this.length++];
    }
  }
}

/*BROWSER* 
export class MidiFile {
/**/
/*NODE*/
module.exports = class MidiFile {
/**/

  /* (serial) is a Buffer or Uint8Array.
   * (name) is a String for your purposes. Empty is fine, or an FS path, or whatever.
   * We dechunk and validate during construction, and throw if anything smells fishy.
   */
  constructor(serial, name) {
    this.serial = serial;
    this.name = name || "";
    this.format = 0;
    this.trackCount = 0;
    this.division = 0; // ticks/qnote; never zero after successful decode
    this.mtrk = []; // Buffer|Uint8Array
    this.unknown = []; // {id,body}

    this._decode();
    this._validate();
  }
  
  *iterateEvents() {
    const heads = this.mtrk.map((mtrk, mtrkIndex) => ({
      mtrk,
      mtrkIndex,
      position: 0,
      status: 0,
      delay: -1, // <0 if not yet read
      finished: false,
    }));
    let timet = 0; // absolute time in ticks
    let times = 0.0; // absolute time in seconds
    let sperqnote = 0.250; // tempo
    let spertick = sperqnote / this.division;
    for (;;) {
      let allFinished = true;
      let nextDelay = 0x10000000; // longer than a vlq can express
      
      // Read delays, check completion, and deliver any zero-delay events.
      for (const head of heads) {
        while (!head.finished) {
          if (head.delay < 0) {
            this._readDelay(head);
            if (head.finished) break;
          }
          if (head.delay) break;
          yield this._readEvent(head);
        }
        if (head.finished) continue;
        //console.log(`ran head ${head.mtrkIndex}, delay=${head.delay} nextDelay=${nextDelay}`);
        allFinished = false;
        if (head.delay < nextDelay) nextDelay = head.delay;
      }
      if (allFinished) break;
      
      // Consume some time.
      for (const head of heads) {
        if (!head.finished) {
          head.delay -= nextDelay;
        }
      }
      timet += nextDelay;
      times += nextDelay * spertick;
      //console.log(`${timet} (${timet%384})`);
      yield Event.timeMarker(timet, times, this.division);
    }
  }
  
// ----- private -----------------------------------------------------------------

  _readDelay(head) {
    if (head.position >= head.mtrk.length) {
      head.finished = true;
      return;
    }
    const [delay, len] = readVlq(head.mtrk, head.position);
    head.delay = delay;
    head.position += len;
    
    if (false&&delay) {
      console.log(`*** delay==${delay}, head ${head.mtrkIndex} at ${head.position-len}/${head.mtrk.length}`);
    }
  }
  
  // must be (delay>=0)
  _readEvent(head) {
    const event = Event.read(head.mtrk, head.position, head.status);
    event.mtrkIndex = head.mtrkIndex;
    event.mtrkCount = this.mtrk.length;
    event.mtrkPosition = head.position;
    event.mtrkLength = head.mtrk.length;
    head.position += event.length;
    head.status = event.status;
    if (head.position >= head.mtrk.length) {
      head.finished = true;
    } else {
      head.delay = -1;
    }
    return event;
  }
  
  _decode() {
    let position = 0;
    while (position < this.serial.length) {
      const [id, body, rlen] = this._decodeChunk(position);
      position += rlen;
      switch (id) {
        case "MThd": this._decodeMThd(body); break;
        case "MTrk": this.mtrk.push(body); break;
        default: this.unknown.push({id, body}); break;
      }
    }
  }

  // => [id, body, rlen], throws if invalid
  _decodeChunk(srcp) {
    if (srcp > this.serial.length - 8) throw new Error(`short chunk header at ${srcp}/${this.serial.length}`);
    const id = this.serial.slice(srcp, srcp + 4).toString();
    const len = (this.serial[srcp+4]<<24)|(this.serial[srcp+5]<<16)|(this.serial[srcp+6]<<8)|this.serial[srcp+7];
    if (len < 0) throw new Error(`invalid chunk length`);
    srcp += 8;
    if (srcp > this.serial.length - len) throw new Error(`${id} chunk at ${srcp-8}/${this.serial.length} overruns input`);
    return [id, this.serial.slice(srcp, srcp + len), 8 + len];
  }
  
  _decodeMThd(src) {
    if (this.division) throw new Error(`multiple MThd`);
    if (src.length < 6) throw new Error(`invalid MThd length ${src.length}`);
    if (src.length > 6) this.warn(`MThd length ${src.length} > 6, ignoring tail`);
    
    this.format = (src[0]<<8)|src[1];
    this.trackCount = (src[2]<<8)|src[3];
    this.division = (src[4]<<8)|src[5];
    
    if (this.format !== 1) this.warn(`Format ${this.format}, expected 1`);
    if (!this.division) throw new Error(`MThd.division == 0`);
    if (this.division & 0x8000) throw new Error(`SMPTE timing not supported (${this.division.toString(16)})`);
  }
  
  _validate() {
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
  
  warn(msg) {
    console.log(`${this.name}:WARNING: ${msg}`);
  }
}
