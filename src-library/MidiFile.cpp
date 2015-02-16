//
// Copyright 1999 by Craig Stuart Sapp, All Rights Reserved.
// Programmer:    Craig Stuart Sapp <craig@ccrma.stanford.edu>
// Creation Date: Fri Nov 26 14:12:01 PST 1999
// Last Modified: Fri Dec  2 13:26:29 PST 1999
// Last Modified: Wed Dec 13 10:33:30 PST 2000 Modified sorting routine
// Last Modified: Tue Jan 22 23:23:37 PST 2002 Allowed reading of meta events
// Last Modified: Tue Nov  4 23:09:19 PST 2003 Adjust noteoff in eventcompare
// Last Modified: Tue Jun 29 09:43:10 PDT 2004 Fixed end-of-track problem
// Last Modified: Sat Dec 17 23:11:57 PST 2005 Added millisecond ticks
// Last Modified: Thu Sep 14 20:07:45 PDT 2006 Added SMPTE ASCII printing
// Last Modified: Tue Apr  7 09:23:48 PDT 2009 Added addMetaEvent.
// Last Modified: Fri Jun 12 22:58:34 PDT 2009 Renamed SigCollection class.
// Last Modified: Mon Jul 26 13:38:23 PDT 2010 Added timing in seconds.
// Last Modified: Tue Feb 22 13:26:40 PST 2011 Added write(ostream).
// Last Modified: Mon Nov 18 13:10:37 PST 2013 Added .printHex function.
// Last Modified: Mon Feb  9 12:22:18 PST 2015 Remove dep. on FileIO class.
// Last Modified: Sat Feb 14 23:40:17 PST 2015 Split out subclasses.
// Filename:      midifile/src/MidiFile.cpp
// Website:       http://midifile.sapp.org
// Syntax:        C++11
//
// Description:   A class which can read/write Standard MIDI files.
//                MIDI data is stored by track in an array.  This
//                class is used for example in the MidiPerform class.
//

#include "MidiFile.h"

#include <string.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace std;


//////////////////////////////
//
// MidiFile::MidiFile -- Constuctor.
//

MidiFile::MidiFile(void) {
   ticksPerQuarterNote = 120;             // time base of file
   trackCount = 1;                       // # of tracks in file
   theTrackState = TRACK_STATE_SPLIT;    // joined or split
   theTimeState = TIME_STATE_DELTA;      // absolute or delta
   events.resize(1);
   events[0] = new MidiEventList;
   readFileName.resize(1);
   readFileName[0] = '\0';
   timemap.clear();
   timemapvalid = 0;
}


MidiFile::MidiFile(const char* filename) {
   ticksPerQuarterNote = 120;             // time base of file
   trackCount = 1;                       // # of tracks in file
   theTrackState = TRACK_STATE_SPLIT;    // joined or split
   theTimeState = TIME_STATE_DELTA;      // absolute or delta
   events.resize(1);
   events[0] = new MidiEventList;
   readFileName.resize(1);
   readFileName[0] = '\0';
   read(filename);
   timemap.clear();
   timemapvalid = 0;
}


MidiFile::MidiFile(const string& filename) {
   ticksPerQuarterNote = 120;             // time base of file
   trackCount = 1;                       // # of tracks in file
   theTrackState = TRACK_STATE_SPLIT;    // joined or split
   theTimeState = TIME_STATE_DELTA;      // absolute or delta
   events.resize(1);
   events[0] = new MidiEventList;
   readFileName.resize(1);
   readFileName[0] = '\0';
   read(filename);
   timemap.clear();
   timemapvalid = 0;
}



//////////////////////////////
//
// MidiFile::~MidiFile -- Deconstructor.
//

MidiFile::~MidiFile() {
   readFileName.resize(1);
   readFileName[0] = '\0';

   clear();

   if (events[0] != NULL) {
      delete events[0];
      events[0] = NULL;
   }
   events.resize(0);

   timemap.clear();
   timemapvalid = 0;
}



//////////////////////////////
//
// MidiFile::setFilename -- sets the filename of the MIDI file.
//      Currently removed any directory path.
//

void MidiFile::setFilename(const char* aname) {
   const char* ptr = strrchr(aname, '/');
   int len;
   if (ptr != NULL) {
     len = strlen(ptr+1);
     readFileName.resize(len+1);
     strncpy(readFileName.data(), ptr+1, len);
   } else {
      len = strlen(aname);
      readFileName.resize(len+1);
      strncpy(readFileName.data(), aname, len);
   }
}


void MidiFile::setFilename(const string& aname) {
   MidiFile::setFilename(aname.data());
}



//////////////////////////////
//
// MidiFile::getFilename -- returns the name of the file read into the
//    structure (if the data was read from a file).
//

const char* MidiFile::getFilename(void) {
   return readFileName.data();
}



//////////////////////////////
//
// MidiFile::getTrack --
//

int MidiFile::getTrack(int track, int index) {
   return getEvent(track, index).track;
}



//////////////////////////////
//
// MidiFile::absoluteTime -- convert the time data to
//    absolute time, which means that the time field
//    in the MidiEvent struct represents the exact tick
//    time to play the event rather than the time since
//    the last event to wait untill playing the current
//    event.
//

void MidiFile::absoluteTime(void) {
   if (getTimeState() == TIME_STATE_ABSOLUTE) {
      return;
   }
   int i, j;
   int length = getNumTracks();
   int* timedata = new int[length];
   for (i=0; i<length; i++) {
      timedata[i] = 0;
      if (events[i]->size() > 0) {
         timedata[i] = (*events[i])[0].time;
      } else {
         continue;
      }
      for (j=1; j<(int)events[i]->size(); j++) {
         timedata[i] += (*events[i])[j].time;
         (*events[i])[j].time = timedata[i];
      }
   }
   theTimeState = TIME_STATE_ABSOLUTE;
   delete [] timedata;
}



//////////////////////////////
//
// MidiFile::addEvent --
//

int MidiFile::addEvent(int aTrack, int aTime, vector<uchar>& midiData) {
   timemapvalid = 0;
   MidiEvent anEvent;
   anEvent.time = aTime;
   anEvent.track = aTrack;
   anEvent.setMessage(midiData);

   events[aTrack]->push_back(anEvent);
   return events[aTrack]->size() - 1;
}



//////////////////////////////
//
// MidiFile::addEvent -- Some bug here when joinedTracks(), but track==1...
//

int MidiFile::addEvent(MidiEvent& mfevent) {
   if (getTrackState() == TRACK_STATE_JOINED) {
      events[0]->push_back(mfevent);
      return events[0]->size()-1;
   } else {
      events[mfevent.track]->push_back(mfevent);
      return events[mfevent.track]->size()-1;
   }
}



///////////////////////////////
//
// MidiFile::addMetaEvent --
//

int MidiFile::addMetaEvent(int aTrack, int aTime, int aType,
      vector<uchar>& metaData) {
   timemapvalid = 0;
   int i;
   int length = metaData.size();
   vector<uchar> fulldata;
   uchar size[23] = {0};
   int lengthsize =  makeVLV(size, length);

   fulldata.resize(2+lengthsize+length);
   fulldata[0] = 0xff;
   fulldata[1] = aType & 0x7F;
   for (i=0; i<lengthsize; i++) {
      fulldata[2+i] = size[i];
   }
   for (i=0; i<length; i++) {
      fulldata[2+lengthsize+i] = metaData[i];
   }

   return addEvent(aTrack, aTime, fulldata);
}


int MidiFile::addMetaEvent(int aTrack, int aTime, int aType,
      const char* metaData) {

   int length = strlen(metaData);
   vector<uchar> buffer;
   buffer.resize(length);
   int i;
   for (i=0; i<length; i++) {
      buffer[i] = (uchar)metaData[i];
   }
   return addMetaEvent(aTrack, aTime, aType, buffer);
}



//////////////////////////////
//
// MidiFile::makeVLV --
//

int MidiFile::makeVLV(uchar *buffer, int number) {

   unsigned long value = (unsigned long)number;

   if (value >= (1 << 28)) {
      cout << "Error: number too large to handle" << endl;
      exit(1);
   }

   buffer[0] = (value >> 21) & 0x7f;
   buffer[1] = (value >> 14) & 0x7f;
   buffer[2] = (value >>  7) & 0x7f;
   buffer[3] = (value >>  0) & 0x7f;

   int i;
   int flag = 0;
   int length = -1;
   for (i=0; i<3; i++) {
      if (buffer[i] != 0) {
         flag = 1;
      }
      if (flag) {
         buffer[i] |= 0x80;
      }
      if (length == -1 && buffer[i] >= 0x80) {
         length = 4-i;
      }
   }

   if (length == -1) {
      length = 1;
   }

   if (length < 4) {
      for (i=0; i<length; i++) {
         buffer[i] = buffer[4-length+i];
      }
   }

   return length;
}





//////////////////////////////
//
// MidiFile::addPitchBend -- convert  number in the range from -1 to +1
//     into two 7-bit numbers (smallest piece first)
//
//   -1.0 maps to 0 (0x0000)
//    0.0 maps to 8192 (0x2000 --> 0x40 0x00)
//   +1.0 maps to 16383 (0x3FFF --> 0x7F 0x7F)
//

int MidiFile::addPitchBend(int aTrack, int aTime, int aChannel, double amount) {
   timemapvalid = 0;
   amount += 1.0;
   int value = int(amount * 8192 + 0.5);

   // prevent any wrap-around in case of round-off errors
   if (value > 0x3fff) {
      value = 0x3fff;
   }
   if (value < 0) {
      value = 0;
   }

   int lsbint = 0x7f & value;
   int msbint = 0x7f & (value  >> 7);

   vector<uchar> mididata;
   mididata.resize(3);
   if (aChannel < 0) {
      aChannel = 0;
   } else if (aChannel > 15) {
      aChannel = 15;
   }
   mididata[0] = uchar(0xe0 | aChannel);
   mididata[1] = uchar(lsbint);
   mididata[2] = uchar(msbint);

   return addEvent(aTrack, aTime, mididata);
}



//////////////////////////////
//
// MidiFile::addTrack -- adds a blank track at end of the
//    track list.  Returns the track number of the added
//    track.
//

int MidiFile::addTrack(void) {
   int length = getNumTracks();
   events.resize(length+1);
   events[length] = new MidiEventList;
   events[length]->reserve(10000);
   events[length]->clear();
   return length;
}

int MidiFile::addTrack(int count) {
   int length = getNumTracks();
   events.resize(length+count);
   int i;
   for (i=0; i<count; i++) {
      events[length + i] = new MidiEventList;
      events[length + i]->reserve(10000);
      events[length + i]->clear();
   }
   return length + count - 1;
}



//////////////////////////////
//
// MidiFile::allocateEvents --
//

void MidiFile::allocateEvents(int track, int aSize) {
   int oldsize = events[track]->size();
   if (oldsize < aSize) {
      events[track]->reserve(aSize);
   }
}



//////////////////////////////
//
// MidiFile::deleteTrack -- remove a track from the MidiFile.
//   Tracks are numbered starting at track 0.
//

void MidiFile::deleteTrack(int aTrack) {
   int length = getNumTracks();
   if (aTrack < 0 || aTrack >= length) {
      return;
   }
   if (length == 1) {
      return;
   }
   delete events[aTrack];
   for (int i=aTrack; i<length-1; i++) {
      events[i] = events[i+1];
   }

   events[length] = NULL;
   events.resize(length-1);
}



//////////////////////////////
//
// MidiFile::deltaTime -- convert the time data to
//     delta time, which means that the time field
//     in the MidiEvent struct represents the time
//     since the last event was played. When a MIDI file
//     is read from a file, this is the default setting.
//

void MidiFile::deltaTime(void) {
   if (getTimeState() == TIME_STATE_DELTA) {
      return;
   }
   int i, j;
   int temp;
   int length = getNumTracks();
   int *timedata = new int[length];
   for (i=0; i<length; i++) {
      timedata[i] = 0;
      if (events[i]->size() > 0) {
         timedata[i] = (*events[i])[0].time;
      } else {
         continue;
      }
      for (j=1; j<(int)events[i]->size(); j++) {
         temp = (*events[i])[j].time;
         (*events[i])[j].time = temp - timedata[i];
         timedata[i] = temp;
      }
   }
   theTimeState = TIME_STATE_DELTA;
   delete [] timedata;
}



//////////////////////////////
//
// MidiFile::clear -- make the MIDI file empty with one
//     track with no data in it.
//

void MidiFile::clear(void) {
   int length = getNumTracks();
   for (int i=0; i<length; i++) {
      delete events[i];
      events[i] = NULL;
   }
   events.resize(1);
   events[0] = new MidiEventList;
   timemapvalid=0;
   timemap.clear();
}


void MidiFile::erase(void) {
   clear();
}



//////////////////////////////
//
// MidiFile::getEvent -- return the event at the given index in the
//    specified track.
//

MidiEvent& MidiFile::getEvent(int aTrack, int anIndex) {
   return (*events[aTrack])[anIndex];
}



//////////////////////////////
//
// MidiFile::operator[] -- return the event list for the specified track.
//

MidiEventList& MidiFile::operator[](int aTrack) {
   return *events[aTrack];
}



//////////////////////////////
//
// MidiFile::getTicksPerQuarterNote -- returns the number of
//   time units that are supposed to occur during a quarternote.
//

int MidiFile::getTicksPerQuarterNote(void) {
   if (ticksPerQuarterNote == 0xE728) {
      // this is a special case which is the SMPTE time code
      // setting for 25 frames a second with 40 subframes
      // which means one tick per millisecond.  When SMPTE is
      // being used, there is no real concept of the quarter note,
      // so presume 60 bpm as a simiplification here.
      // return 1000;
   }
   return ticksPerQuarterNote;
}



//////////////////////////////
//
// MidiFile::getTrackCount -- return the number of tracks in
//   the Midi File.
//

int MidiFile::getTrackCount(void) {
   return events.size();
}


int MidiFile::getNumTracks(void) {
   return events.size();
}



//////////////////////////////
//
// MidiFile::getEventCount -- returns the number of events
//   in a given track.
//

int MidiFile::getEventCount(int aTrack) {
   return events[aTrack]->size();
}


int MidiFile::getNumEvents(int aTrack) {
   return events[aTrack]->size();
}



//////////////////////////////
//
// MidiFile::joinTracks -- merge the data from all tracks,
//   but keeping the identity of the tracks unique so that
//   the function splitTracks can be called to split the
//   tracks into separate units again.  The style of the
//   MidiFile when read from a file is with tracks split.
//

void MidiFile::joinTracks(void) {
   if (getTrackState() == TRACK_STATE_JOINED) {
      return;
   }
   if (getNumTracks() == 1) {
      return;
   }

   MidiEventList* joinedTrack;
   joinedTrack = new MidiEventList;
   joinedTrack->reserve(200000);
   joinedTrack->clear();
   int oldTimeState = getTimeState();
   if (oldTimeState == TIME_STATE_DELTA) {
      absoluteTime();
   }
   int i, j;
   int length = getNumTracks();
   for (i=0; i<length; i++) {
      for (j=0; j<(int)events[i]->size(); j++) {
         joinedTrack->push_back((*events[i])[j]);
      }
   }

   clear();

   delete events[0];
   events[0] = joinedTrack;
   sortTracks();
   if (oldTimeState == TIME_STATE_DELTA) {
      deltaTime();
   }
}



//////////////////////////////
//
// MidiFile::mergeTracks -- combine the data from two
//   tracks into one.  Placing the data in the first
//   track location listed, and Moving the other tracks
//   in the file around to fill in the spot where Track2
//   used to be.  The results of this function call cannot
//   be reversed.
//

void MidiFile::mergeTracks(int aTrack1, int aTrack2) {
   MidiEventList* mergedTrack;
   mergedTrack = new MidiEventList;
   int oldTimeState = getTimeState();
   if (oldTimeState == TIME_STATE_DELTA) {
      absoluteTime();
   }
   int i, j;
   int length = getNumTracks();
   for (i=0; i<(int)events[aTrack1]->size(); i++) {
      mergedTrack->push_back((*events[aTrack1])[i]);
   }
   for (j=0; j<(int)events[aTrack2]->size(); i++) {
      (*events[aTrack2])[i].track = aTrack1;
      mergedTrack->push_back((*events[aTrack2])[i]);
   }

   sortTrack(*mergedTrack);

   delete events[aTrack1];

   events[aTrack1] = mergedTrack;

   for (i=aTrack2; i<length-1; i++) {
      events[i] = events[i+1];
   }

   events[length] = NULL;
   events.resize(length-1);

   if (oldTimeState == TIME_STATE_DELTA) {
      deltaTime();
   }
}



//////////////////////////////
//
// MidiFile::read -- Parse a Standard MIDI File and store its contents
//      in the object.
//

int MidiFile::read(const char* filename) {
   timemapvalid = 0;
   if (filename != NULL) {
      setFilename(filename);
   }

   fstream input;
   input.open(filename, ios::binary | ios::in);

   if (!input.is_open()) {
      return 0;
   }

   return MidiFile::read(input);
}


//
// string version of read().
//


int MidiFile::read(const string& filename) {
   timemapvalid = 0;
   setFilename(filename);

   fstream input;
   input.open(filename.data(), ios::binary | ios::in);

   if (!input.is_open()) {
      return 0;
   }

   return MidiFile::read(input);
}


//
// istream version of read().
//

int MidiFile::read(istream& input) {
   const char* filename = getFilename();

   int    character;
   // uchar  buffer[123456] = {0};
   ulong  longdata;
   ushort shortdata;


   // Read the MIDI header (4 bytes of ID, 4 byte data size,
   // anticipated 6 bytes of data.

   character = input.get();
   if (character == EOF) {
      cout << "In file " << filename << ": unexpected end of file." << endl;
      cout << "Expecting 'M' at first byte, but found nothing." << endl;
      exit(1);
   } else if (character != 'M') {
      cout << "File " << filename << " is not a MIDI file" << endl;
      cout << "Expecting 'M' at first byte but got '"
           << character << "'" << endl;
      return 0;
   }

   character = input.get();
   if (character == EOF) {
      cout << "In file " << filename << ": unexpected end of file." << endl;
      cout << "Expecting 'T' at first byte, but found nothing." << endl;
      exit(1);
   } else if (character != 'T') {
      cout << "File " << filename << " is not a MIDI file" << endl;
      cout << "Expecting 'T' at first byte but got '"
           << character << "'" << endl;
      return 0;
   }

   character = input.get();
   if (character == EOF) {
      cout << "In file " << filename << ": unexpected end of file." << endl;
      cout << "Expecting 'h' at first byte, but found nothing." << endl;
      exit(1);
   } else if (character != 'h') {
      cout << "File " << filename << " is not a MIDI file" << endl;
      cout << "Expecting 'h' at first byte but got '"
           << character << "'" << endl;
      return 0;
   }

   character = input.get();
   if (character == EOF) {
      cout << "In file " << filename << ": unexpected end of file." << endl;
      cout << "Expecting 'd' at first byte, but found nothing." << endl;
      exit(1);
   } else if (character != 'd') {
      cout << "File " << filename << " is not a MIDI file" << endl;
      cout << "Expecting 'd' at first byte but got '"
           << character << "'" << endl;
      return 0;
   }

   // read header size (allow larger header size?)
   longdata = MidiFile::readLittleEndian4Bytes(input);
   if (longdata != 6) {
      cout << "File " << filename
           << " is not a MIDI 1.0 Standard MIDI file." << endl;
      cout << "The header size is " << longdata << " bytes." << endl;
      return 0;
   }

   // Header parameter #1: format type
   int type;
   shortdata = MidiFile::readLittleEndian2Bytes(input);
   switch (shortdata) {
      case 0:
         type = 0;
         break;
      case 1:
         type = 1;
         break;
      case 2:    // Type-2 MIDI files should probably be allowed as well.
      default:
         cout << "Error: cannot handle a type-" << shortdata
              << " MIDI file" << endl;
         return 0;
   }

   // Header parameter #2: track count
   int tracks;
   shortdata = MidiFile::readLittleEndian2Bytes(input);
   if (type == 0 && shortdata != 1) {
      cout << "Error: Type 0 MIDI file can only contain one track" << endl;
      cout << "Instead track count is: " << shortdata << endl;
      return 0;
   } else {
      tracks = shortdata;
   }
   clear();
   if (events[0] != NULL) {
      delete events[0];
   }
   events.resize(tracks);
   for (int z=0; z<tracks; z++) {
      events[z] = new MidiEventList;
      events[z]->reserve(10000);   // Initialize with 10,000 event storage.
      events[z]->clear();
   }

   // Header parameter #3: Ticks per quarter note
   shortdata = MidiFile::readLittleEndian2Bytes(input);
   if (shortdata >= 0x8000) {
      int framespersecond = ((!(shortdata >> 8))+1) & 0x00ff;
      int resolution      = shortdata & 0x00ff;
      switch (framespersecond) {
         case 232:  framespersecond = 24; break;
         case 231:  framespersecond = 25; break;
         case 227:  framespersecond = 29; break;
         case 226:  framespersecond = 30; break;
         default:
               cerr << "Warning: unknown FPS: " << framespersecond << endl;
               framespersecond = 255 - framespersecond + 1;
               cerr << "Setting FPS to " << framespersecond << endl;
      }
      // actually ticks per second (except for frame=29 (drop frame)):
      ticksPerQuarterNote = shortdata;

      cout << "SMPTE ticks: " << ticksPerQuarterNote << " ticks/sec" << endl;
      cout << "SMPTE frames per second: " << framespersecond << endl;
      cout << "SMPTE frame resolution per frame: " << resolution << endl;
   }  else {
      ticksPerQuarterNote = shortdata;
   }


   //////////////////////////////////////////////////
   //
   // now read individual tracks:
   //

   uchar runningCommand;
   MidiEvent event;
   vector<uchar> bytes;
   int absticks;
   // int barline;

   for (int i=0; i<tracks; i++) {
      runningCommand = 0;

      // cout << "\nReading Track: " << i + 1 << flush;

      // read track header...

      character = input.get();
      if (character == EOF) {
         cout << "In file " << filename << ": unexpected end of file." << endl;
         cout << "Expecting 'M' at first byte in track, but found nothing."
              << endl;
         exit(1);
      } else if (character != 'M') {
         cout << "File " << filename << " is not a MIDI file" << endl;
         cout << "Expecting 'M' at first byte in track but got '"
              << character << "'" << endl;
         return 0;
      }

      character = input.get();
      if (character == EOF) {
         cout << "In file " << filename << ": unexpected end of file." << endl;
         cout << "Expecting 'T' at first byte in track, but found nothing."
              << endl;
         exit(1);
      } else if (character != 'T') {
         cout << "File " << filename << " is not a MIDI file" << endl;
         cout << "Expecting 'T' at first byte in track but got '"
              << character << "'" << endl;
         return 0;
      }

      character = input.get();
      if (character == EOF) {
         cout << "In file " << filename << ": unexpected end of file." << endl;
         cout << "Expecting 'r' at first byte in track, but found nothing."
              << endl;
         exit(1);
      } else if (character != 'r') {
         cout << "File " << filename << " is not a MIDI file" << endl;
         cout << "Expecting 'r' at first byte in track but got '"
              << character << "'" << endl;
         return 0;
      }

      character = input.get();
      if (character == EOF) {
         cout << "In file " << filename << ": unexpected end of file." << endl;
         cout << "Expecting 'k' at first byte in track, but found nothing."
              << endl;
         exit(1);
      } else if (character != 'k') {
         cout << "File " << filename << " is not a MIDI file" << endl;
         cout << "Expecting 'k' at first byte in track but got '"
              << character << "'" << endl;
         return 0;
      }

      // Now read track chunk size and throw it away because it is
      // not really necessary since the track MUST end with an
      // end of track meta event, and 50% of Midi files or so
      // do not correctly give the track size.
      longdata = MidiFile::readLittleEndian4Bytes(input);

      // set the size of the track allocation so that it might
      // approximately fit the data.
      events[i]->reserve(longdata/2);
      events[i]->clear();

      // process the track
      absticks = 0;
      // barline = 1;
      while (!input.eof()) {
         longdata = extractVlvTime(input);
         //cout << "ticks = " << longdata << endl;
         absticks += longdata;
         extractMidiData(input, bytes, runningCommand);
         event.setMessage(bytes);
         //cout << "command = " << hex << (int)event.data[0] << dec << endl;
         if (bytes[0] == 0xff && (bytes[1] == 1 ||
             bytes[1] == 2 || bytes[1] == 3 || bytes[1] == 4)) {
           // mididata.push_back('\0');
           // cout << '\t';
           // for (int m=0; m<event.data[2]; m++) {
           //    cout << event.data[m+3];
           // }
           // cout.flush();
         } else if (bytes[0] == 0xff && bytes[1] == 0x2f) {
            // end of track message
            // uncomment out the following three lines if you don't want
            // to see the end of track message (which is always required,
            // and added automatically when a MIDI is written.
            event.time = absticks;
            event.track = i;
            events[i]->push_back(event);

            break;
         }

         if (bytes[0] != 0xff && bytes[0] != 0xf0) {
            event.time = absticks;
            event.track = i;
            events[i]->push_back(event);
         } else {
            event.time = absticks;
            event.track = i;
            events[i]->push_back(event);
         }

      }

   }

   theTimeState = TIME_STATE_ABSOLUTE;
   return 1;
}



//////////////////////////////
//
// MidiFile::setTicksPerQuarterNote --
//

void MidiFile::setTicksPerQuarterNote(int ticks) {
   ticksPerQuarterNote = ticks;
}



//////////////////////////////
//
// MidiFile::setMilliseconds -- set the ticks per quarter note
//   value to milliseconds.  The format for this specification is
//   highest 8-bits: SMPTE Frame rate (as a negative 2's compliment value).
//   lowest 8-bits: divisions per frame (as a positive number).
//   for millisecond resolution, the SMPTE value is -25, and the
//   frame rate is 40 frame per division.  In hexadecimal, these
//   values are: -25 = 1110,0111 = 0xE7 and 40 = 0010,1000 = 0x28
//   So setting the ticks per quarter note value to 0xE728 will cause
//   delta times in the MIDI file to represent milliseconds.
//

void MidiFile::setMillisecondDelta(void) {
   ticksPerQuarterNote = 0xE728;
}



//////////////////////////////
//
// MidiFile::sortTrack --
//

void MidiFile::sortTrack(MidiEventList& trackData) {
   qsort(trackData.data(), trackData.size(),
      sizeof(MidiEvent*), eventcompare);
}



//////////////////////////////
//
// MidiFile::sortTracks -- sort all tracks in the MidiFile.
//

void MidiFile::sortTracks(void) {
   for (int i=0; i<getTrackCount(); i++) {
      sortTrack(*events[i]);
   }
}



//////////////////////////////
//
// MidiFile::splitTracks -- take the joined tracks and split them
//   back into their separate track identities.
//

void MidiFile::splitTracks(void) {
   if (getTrackState() == TRACK_STATE_SPLIT) {
      return;
   }

   int oldTimeState = getTimeState();
   if (oldTimeState == TIME_STATE_DELTA) {
      absoluteTime();
   }

   int maxTrack = 0;
   int i;
   int length = events[0]->size();
   for (i=0; i<length; i++) {
      if ((*events[0])[i].track > maxTrack) {
          maxTrack = (*events[0])[i].track;
      }
   }

   MidiEventList* olddata = events[0];
   events[0] = NULL;
   events.resize(maxTrack);
   for (i=0; i<maxTrack; i++) {
      events[i] = new MidiEventList;
      events[i]->clear();
   }

   int trackValue = 0;
   for (i=0; length; i++) {
      trackValue = (*olddata)[i].track;
      events[trackValue]->push_back((*olddata)[i]);
   }

   delete olddata;

   if (oldTimeState == TIME_STATE_DELTA) {
      deltaTime();
   }
}



//////////////////////////////
//
// MidiFile::getTrackCount --  Return the number of tracks in the
//    MIDI file.  Returns the size of the events if not in joined state.
//    If in joined state, reads track 0 to find the maximum track
//    value from the original unjoined tracks.
//

int MidiFile::getTrackCountAsType1(void) {
   if (getTrackState() == TRACK_STATE_JOINED) {
      int output = 0;
      int i;
      for (i=0; i<(int)events[0]->size(); i++) {
         if (getEvent(0,i).track > output) {
            output = getEvent(0,i).track;
         }
      }
      return output+1;  // I think the track values are 0 offset...
   } else {
      return events.size();
   }
}



//////////////////////////////
//
// MidiFile::timeState -- returns what type of time method is
//   being used: either TIME_STATE_ABSOLUTE or TIME_STATE_DELTA.
//

int MidiFile::getTimeState(void) {
   return theTimeState;
}



//////////////////////////////
//
// MidiFile::getTrackState -- returns what type of track method
//     is being used: either TRACK_STATE_JOINED or TRACK_STATE_SPLIT.
//

int MidiFile::getTrackState(void) {
   return theTrackState;
}



//////////////////////////////
//
// MidiFile::write -- write a standard MIDI file from data.
//

int MidiFile::write(const char* filename) {
   #ifndef OLDCPP
      #ifdef VISUAL
         fstream outputfile(filename, ios::out | ios::binary);
      #else
         fstream outputfile(filename, ios::out);
      #endif
   #else
      #ifdef VISUAL
         fstream outputfile(filename, ios::out | ios::noreplace | ios::binary);
      #else
         fstream outputfile(filename, ios::out | ios::noreplace);
      #endif
   #endif

   if (!outputfile.is_open()) {
      cout << "Error: could not write: " << filename << endl;
      exit(1);
   }
   int status = write(outputfile);
   outputfile.close();
   return status;
}


int MidiFile::write(const string& filename) {
   return MidiFile::write(filename.data());
}


//////////////////////////////
//
// MidiFile::printHex -- print as a list of ASCII Hex bytes,
//    formatted 25 to a line, and two digits each.
//

ostream& MidiFile::printHex(ostream& out) {
   stringstream tempstream;
   MidiFile::write(tempstream);
   int value = 0;
   int i;
   int len = tempstream.str().length();
   int wordcount = 1;
   int linewidth = 25;
   for (i=0; i<len; i++) {
      value = (unsigned char)tempstream.str()[i];
      printf("%02x", value);
      if (i < len - 1) {
         if (wordcount % linewidth) {
            out << ' ';
         } else {
            out << '\n';
         }
      }
      wordcount++;
   }
   out << '\n';
   return out;
}



//////////////////////////////
//
// MidiFile::write -- write a standard MIDI file to an output stream.
//

int MidiFile::write(ostream& outputfile) {
   int oldTimeState = getTimeState();
   if (oldTimeState == TIME_STATE_ABSOLUTE) {
      deltaTime();
   }

   // write the header of the Standard MIDI File

   char ch;
   // 1. The characters "MThd"
   ch = 'M';
   // outputfile.writeBigEndian(ch);
   outputfile << ch;
   ch = 'T';
   // outputfile.writeBigEndian(ch);
   outputfile << ch;
   ch = 'h';
   // outputfile.writeBigEndian(ch);
   outputfile << ch;
   ch = 'd';
   // outputfile.writeBigEndian(ch);
   outputfile << ch;

   // 2. write the size of the header (always a "6" stored in unsigned long
   //    (4 bytes).
   ulong longdata = 6;
   // outputfile.writeBigEndian(longdata);
   ch = (char)((longdata >> 24) & 0xff); outputfile << ch;
   ch = (char)((longdata >> 16) & 0xff); outputfile << ch;
   ch = (char)((longdata >>  8) & 0xff); outputfile << ch;
   ch = (char)( longdata        & 0xff); outputfile << ch;

   // 3. MIDI file format, type 0, 1, or 2
   ushort shortdata;
   if (getNumTracks() == 1) {
      shortdata = 0;
   } else {
      shortdata = 1;
   }
   // outputfile.writeBigEndian(shortdata);
   ch = (char)((shortdata >>  8) & 0xff); outputfile << ch;
   ch = (char)( shortdata        & 0xff); outputfile << ch;

   // 4. write out the number of tracks.
   shortdata = getNumTracks();
   // outputfile.writeBigEndian(shortdata);
   ch = (char)((shortdata >>  8) & 0xff); outputfile << ch;
   ch = (char)( shortdata        & 0xff); outputfile << ch;

   // 5. write out the number of ticks per quarternote. (avoiding SMTPE for now)
   shortdata = getTicksPerQuarterNote();
   // outputfile.writeBigEndian(shortdata);
   ch = (char)((shortdata >>  8) & 0xff); outputfile << ch;
   ch = (char)( shortdata        & 0xff); outputfile << ch;

   // now write each track.
   vector<uchar> trackdata;
   uchar endoftrack[4] = {0, 0xff, 0x2f, 0x00};
   int i, j, k;
   int size;
   for (i=0; i<getNumTracks(); i++) {
      trackdata.reserve(1000000);   // make the track data larger than
                                    // expected data input
      trackdata.clear();
      for (j=0; j<(int)events[i]->size(); j++) {
         writeVLValue((*events[i])[j].time, trackdata);
         for (k=0; k<(int)(*events[i])[j].size(); k++) {
            trackdata.push_back((*events[i])[j][k]);
         }
      }
      size = trackdata.size();
      if ((size < 3) || !((trackdata[size-3] == 0xff)
            && (trackdata[size-2] == 0x2f))) {
         trackdata.push_back(endoftrack[0]);
         trackdata.push_back(endoftrack[1]);
         trackdata.push_back(endoftrack[2]);
         trackdata.push_back(endoftrack[3]);
      }

      // now ready to write to MIDI file.

      // first write the track ID marker "MTrk":
      ch = 'M'; outputfile << ch;
      ch = 'T'; outputfile << ch;
      ch = 'r'; outputfile << ch;
      ch = 'k'; outputfile << ch;

      // A. write the size of the MIDI data to follow:
      longdata = trackdata.size();
      // outputfile.writeBigEndian(longdata);
      ch = (char)((longdata >> 24) & 0xff); outputfile << ch;
      ch = (char)((longdata >> 16) & 0xff); outputfile << ch;
      ch = (char)((longdata >>  8) & 0xff); outputfile << ch;
      ch = (char)( longdata        & 0xff); outputfile << ch;

      // B. write the actual data
      outputfile.write((char*)trackdata.data(), trackdata.size());
   }

   if (oldTimeState == TIME_STATE_ABSOLUTE) {
      absoluteTime();
   }

   // outputfile.close();

   return 1;
}



//////////////////////////////
//
// MidiFile::getTimeInSeconds -- return the time in seconds for
//     the current message.
//

double MidiFile::getTimeInSeconds(int aTrack, int anIndex) {
   return getTimeInSeconds(getEvent(aTrack, anIndex).time);
}



double MidiFile::getTimeInSeconds(int tickvalue) {
   if (timemapvalid == 0) {
      buildTimeMap();
      if (timemapvalid == 0) {
         return -1.0;    // something went wrong
      }
   }

   _TickTime key;
   key.tick    = tickvalue;
   key.seconds = -1;

   void* ptr = bsearch(&key, timemap.data(), timemap.size(),
         sizeof(_TickTime), ticksearch);

   if (ptr == NULL) {
      // The specific tick value was not found, so do a linear
      // search for the two tick values which occur before and
      // after the tick value, and do a linear interpolation of
      // the time in seconds values to figure out the final
      // time in seconds.
      // Since the code is not yet written, kill the program at this point:
      cerr << "ERROR: tick value " << tickvalue << " was not found " << endl;
      exit(1);
   } else {
      return ((_TickTime*)ptr)->seconds;
   }
}



//////////////////////////////
//
// MidiFile::getAbsoluteTickTime -- return the tick value represented
//    by the input time in seconds.  If there is not tick entry at
//    the given time in seconds, then interpolate between two values.
//

int MidiFile::getAbsoluteTickTime(double starttime) {
   if (timemapvalid == 0) {
      buildTimeMap();
      if (timemapvalid == 0) {
         if (timemapvalid == 0) {
            return -1.0;    // something went wrong
         }
      }
   }

   _TickTime key;
   key.tick    = -1;
   key.seconds = starttime;

   void* ptr = bsearch(&key, timemap.data(), timemap.size(),
         sizeof(_TickTime), secondsearch);

   if (ptr == NULL) {
      // The specific seconds value was not found, so do a linear
      // search for the two time values which occur before and
      // after the given time value, and do a linear interpolation of
      // the time in tick values to figure out the final time in ticks.
      return linearTickInterpolationAtSecond(starttime);
   } else {
      return ((_TickTime*)ptr)->tick;
   }

}



//////////////////////////////
//
// MidiFile::doTimeInSecondsAnalysis --
//

void MidiFile::doTimeInSecondsAnalysis(void) {
    buildTimeMap();
}



///////////////////////////////////////////////////////////////////////////
//
// private functions
//

//////////////////////////////
//
// MidiFile::linearTickInterpolationAtSecond -- return the tick value at the
//    given input time.
//

int MidiFile::linearTickInterpolationAtSecond(double seconds) {
   if (timemapvalid == 0) {
      buildTimeMap();
      if (timemapvalid == 0) {
         return -1.0;    // something went wrong
      }
   }

   int i;
   double lasttime = timemap[timemap.size()-1].seconds;
   // give an error value of -1 if time is out of range of data.
   if (seconds < 0.0) {
      return -1;
   }
   if (seconds > timemap[timemap.size()-1].seconds) {
      return -1;
   }

   // Guess which side of the list is closest to target:
   // Could do a more efficient algorithm since time values are sorted,
   // but good enough for now...
   int startindex = -1;
   if (seconds < lasttime / 2) {
      for (i=0; i<(int)timemap.size(); i++) {
         if (timemap[i].seconds > seconds) {
            startindex = i-1;
            break;
         } else if (timemap[i].seconds == seconds) {
            startindex = i;
            break;
         }
      }
   } else {
      for (i=(int)timemap.size()-1; i>0; i--) {
         if (timemap[i].seconds < seconds) {
            startindex = i+1;
            break;
         } else if (timemap[i].seconds == seconds) {
            startindex = i;
            break;
         }
      }
   }

   if (startindex < 0) {
      return -1;
   }
   if (startindex >= (int)timemap.size()-1) {
      return -1;
   }

   double x1 = timemap[startindex].seconds;
   double x2 = timemap[startindex+1].seconds;
   double y1 = timemap[startindex].tick;
   double y2 = timemap[startindex+1].tick;
   double xi = seconds;

   return (xi-x1) * ((y2-y1)/(x2-x1)) + y1;
}



//////////////////////////////
//
// MidiFile::buildTimeMap -- build an index of the absolute tick values
//      found in a MIDI file, and their corresponding time values in
//      seconds, taking into consideration tempo change messages.  If no
//      tempo messages are given (or until they are given, then the
//      tempo is set to 120 beats per minute).  If SMPTE time code is
//      used, then ticks are actually time values.  So don't build
//      a time map for SMPTE ticks, and just calculate the time in
//      seconds from the tick value (1000 ticks per second SMPTE
//      is the only mode tested (25 frames per second and 40 subframes
//      per frame).
//

void MidiFile::buildTimeMap(void) {

   // convert the MIDI file to absolute time representation
   // in single track mode (and undo if the MIDI file was not
   // in that state when this function was called.
   //
   int trackstate = getTrackState();
   int timestate  = getTimeState();

   absoluteTime();
   joinTracks();

   int allocsize = getNumEvents(0);
   timemap.reserve(allocsize+10);
   timemap.clear();

   _TickTime value;

   int lasttick = 0;
   int curtick;
   int tickinit = 0;

   int i;
   int tpq = getTicksPerQuarterNote();
   double defaultTempo = 120.0;
   double secondsPerTick = 60.0 / (defaultTempo * tpq);

   double lastsec = 0.0;
   double cursec;

   for (i=0; i<getNumEvents(0); i++) {
      curtick = getEvent(0, i).time;
      if ((curtick > lasttick) || !tickinit) {
         tickinit = 1;

         // calculate the current time in seconds:
         cursec = lastsec + (curtick - lasttick) * secondsPerTick;

         // store the new tick to second mapping
         value.tick = curtick;
         value.seconds = cursec;
         timemap.push_back(value);
         lasttick   = curtick;
         lastsec    = cursec;
      }

      // update the tempo if needed:
      if (getEvent(0,i).isTempo()) {
         secondsPerTick = getEvent(0,i).getTempoSPT(getTicksPerQuarterNote());
      }
   }

   // reset the states of the tracks or time values if necessary here:
   if (timestate == TIME_STATE_DELTA) {
      deltaTime();
   }
   if (trackstate == TRACK_STATE_SPLIT) {
      splitTracks();
   }

   timemapvalid = 1;

}



//////////////////////////////
//
// MidiFile::extractMidiData --
//

void MidiFile::extractMidiData(istream& input, vector<uchar>& array,
   uchar& runningCommand) {

   int character;
   uchar byte;
   array.clear();
   int runningQ;

   character = input.get();
   if (character == EOF) {
      cerr << "Error: unexpected end of file." << endl;
      exit(1);
   } else {
      byte = (uchar)character;
   }

   if (byte < 0x80) {
      runningQ = 1;
      if (runningCommand == 0) {
         cout << "Error: running command with no previous command" << endl;
         exit(1);
      }
   } else {
      runningCommand = byte;
      runningQ = 0;
   }

   array.push_back(runningCommand);
   if (runningQ) {
      array.push_back(byte);
   }

   uchar metai;
   switch (runningCommand & 0xf0) {
      case 0x80:        // note off (2 more bytes)
      case 0x90:        // note on (2 more bytes)
      case 0xA0:        // aftertouch (2 more bytes)
      case 0xB0:        // cont. controller (2 more bytes)
      case 0xE0:        // pitch wheel (2 more bytes)
         byte = MidiFile::readByte(input);
         array.push_back(byte);
         if (!runningQ) {
            byte = MidiFile::readByte(input);
            array.push_back(byte);
         }
         break;
      case 0xC0:        // patch change (1 more byte)
      case 0xD0:        // channel pressure (1 more byte)
         if (!runningQ) {
            byte = MidiFile::readByte(input);
            array.push_back(byte);
         }
         break;
      case 0xF0:
         switch (runningCommand) {
            case 0xff:                 // meta event
               {
               if (!runningQ) {
                  byte = MidiFile::readByte(input); // meta type
               array.push_back(byte);
               }
               metai = MidiFile::readByte(input); // meta type
               array.push_back(metai);
               for (uchar j=0; j<metai; j++) {
                  byte = MidiFile::readByte(input); // meta type
                  array.push_back(byte);
               }
               }
               break;
            case 0xf0:                // sysex
               // read until you find a 0xf7 character
               byte = 0;
               while (byte != 0xf7 && !input.eof()) {
                  byte = MidiFile::readByte(input); // meta data
               }
               break;
         }
         break;
      default:
         cout << "Error reading midifile" << endl;
         cout << "Command byte was " << (int)runningCommand << endl;
         exit(1);
   }
}



//////////////////////////////
//
// MidiFile::extractVlvTime --
//

ulong MidiFile::extractVlvTime(istream& input) {
   uchar b[5] = {0};

   for (int i=0; i<5; i++) {
      b[i] = MidiFile::readByte(input);
      if (b[i] < 0x80) {
         break;
      }
   }

   return unpackVLV(b[0], b[1], b[2], b[3], b[4]);
}



//////////////////////////////
//
// MidiFile::unpackVLV -- converts a VLV value to pure unsigned long value.
// default values: a = b = c = d = e = 0;
//

ulong MidiFile::unpackVLV(uchar a, uchar b, uchar c, uchar d, uchar e) {
   if (e > 0x7f) {
      cout << "Error: VLV value was too long" << endl;
      exit(1);
   }

   uchar bytes[5] = {a, b, c, d, e};
   int count = 0;
   while (bytes[count] > 0x7f && count < 5) {
      count++;
   }
   count++;

   ulong output = 0;
   for (int i=0; i<count; i++) {
      output = output << 7;
      output = output | (bytes[i] & 0x7f);
   }

   return output;
}



//////////////////////////////
//
// MidiFile::writeVLValue -- write a number to the midifile
//    as a variable length value which segments a file into 7-bit
//    values.  Maximum size of aValue is 0x7fffffff
//

void MidiFile::writeVLValue(long aValue, vector<uchar>& outdata) {
   uchar bytes[5] = {0};
   bytes[0] = (uchar)(((ulong)aValue >> 28) & 0x7f);  // most significant 5 bits
   bytes[1] = (uchar)(((ulong)aValue >> 21) & 0x7f);  // next largest 7 bits
   bytes[2] = (uchar)(((ulong)aValue >> 14) & 0x7f);
   bytes[3] = (uchar)(((ulong)aValue >> 7)  & 0x7f);
   bytes[4] = (uchar)(((ulong)aValue)       & 0x7f);  // least significant 7 bits

   int start = 0;
   while (start<5 && bytes[start] == 0)  start++;

   for (int i=start; i<4; i++) {
      bytes[i] = bytes[i] | 0x80;
      outdata.push_back(bytes[i]);
   }
   outdata.push_back(bytes[4]);
}



///////////////////////////////////////////////////////////////////////////
//
// external functions
//


//////////////////////////////
//
// eventcompare -- for sorting the tracks
//

int eventcompare(const void* a, const void* b) {
   MidiEvent& aevent = **((MidiEvent**)a);
   MidiEvent& bevent = **((MidiEvent**)b);

   if (aevent.time > bevent.time) {
      return 1;
   } else if (aevent.time < bevent.time) {
      return -1;
   } else if (aevent[0] == 0xff && bevent[0] != 0xff) {
      return 1;
   } else if (bevent[0] == 0xff && aevent[0] != 0xff) {
      return -1;
   } else if (bevent[0] == 0xff && bevent[1] == 0x2f) {
      return -1;
   } else if (aevent[0] == 0xff && aevent[1] == 0x2f) {
      return 1;
   } else if (((aevent[0] & 0xf0) == 0xe0) &&
              ((bevent[0] & 0xf0) == 0x90)) {
      // pitch bend placed before note on messages
      return -1;
   } else if (((aevent[0] & 0xf0) == 0x90) &&
              ((bevent[0] & 0xf0) == 0xe0)) {
      // pitch bend placed before note on messages
      return +1;
   } else if (((aevent[0] & 0xf0) == 0x90) &&
              ((bevent[0] & 0xf0) == 0x80)) {
      return 1;
   } else if (((aevent[0] & 0xf0) == 0x80) &&
              ((bevent[0] & 0xf0) == 0x90)) {
      return -1;
   } else {
      return 0;
   }
}



//////////////////////////////
//
// operator<< -- for printing an ASCII version of the MIDI file
//

ostream& operator<<(ostream& out, MidiFile& aMidiFile) {
   int i, j, k;
   out << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
   out << "Number of Tracks: " << aMidiFile.getTrackCount() << "\n";
   out << "Time method: " << aMidiFile.getTimeState();
   if (aMidiFile.getTimeState() == TIME_STATE_DELTA) {
      out << " (Delta timing)";
   } else if (aMidiFile.getTimeState() == TIME_STATE_ABSOLUTE) {
      out << " (Absolute timing)";
   } else {
      out << " (unknown method)";
   }
   out << "\n";

   out << "Divisions per Quarter Note: " << dec
       << aMidiFile.getTicksPerQuarterNote() << "\n";
   for (i=0; i<aMidiFile.getNumTracks(); i++) {
      out << "\nTrack " << i
          << "   +++++++++++++++++++++++++++++++++++++++++++++++++++\n\n";
      for (j=0; j<aMidiFile.getNumEvents(i); j++) {
         out << dec << aMidiFile.getEvent(i, j).time << "\t"
             << "0x" << hex << (int)aMidiFile.getEvent(i, j)[0] << " ";
         if (aMidiFile.getEvent(i, j)[0] == 0xff) {

            if (aMidiFile.getEvent(i, j)[1] == 0x01) {
               out << "TEXT [";
               for (k=3; k<(int)aMidiFile.getEvent(i, j).size(); k++) {
                  out << (char)aMidiFile.getEvent(i, j)[k];
               }
               out << "]";

            } else if (aMidiFile.getEvent(i, j)[1] == 0x02) {
               out << "COPY [";
               for (k=3; k<(int)aMidiFile.getEvent(i, j).size(); k++) {
                  out << (char)aMidiFile.getEvent(i, j)[k];
               }
               out << "]";

            } else if (aMidiFile.getEvent(i, j)[1] == 0x03) {
               out << "TRACK [";
               for (k=3; k<(int)aMidiFile.getEvent(i, j).size(); k++) {
                  out << (char)aMidiFile.getEvent(i, j)[k];
               }
               out << "]";

            } else if (aMidiFile.getEvent(i, j)[1] == 0x04) {
               out << "INSTR [";
               for (k=3; k<(int)aMidiFile.getEvent(i, j).size(); k++) {
                  out << (char)aMidiFile.getEvent(i, j)[k];
               }
               out << "]";

            } else if (aMidiFile.getEvent(i, j)[1] == 0x05) {
               out << "LYRIC [";
               for (k=3; k<(int)aMidiFile.getEvent(i, j).size(); k++) {
                  out << (char)aMidiFile.getEvent(i, j)[k];
               }
               out << "]";

            } else {
               for (k=1; k<(int)aMidiFile.getEvent(i, j).size(); k++) {
                  out << dec << (int)aMidiFile.getEvent(i, j)[k] << " ";
               }
            }

         } else {
            for (k=1; k<(int)aMidiFile.getEvent(i, j).size(); k++) {
               out << dec << (int)aMidiFile.getEvent(i, j)[k] << " ";
            }
         }
         out << "\n";
      }
   }
   out << "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n";
   return out;
}



//////////////////////////////
//
// MidiFile::ticksearch -- for finding a tick entry in the time map.
//

int MidiFile::ticksearch(const void* A, const void* B) {
   _TickTime& a = *((_TickTime*)A);
   _TickTime& b = *((_TickTime*)B);

   if (a.tick < b.tick) {
      return -1;
   } else if (a.tick > b.tick) {
      return 1;
   }
   return 0;
}



//////////////////////////////
//
// MidiFile::secondsearch -- for finding a second entry in the time map.
//

int MidiFile::secondsearch(const void* A, const void* B) {
   _TickTime& a = *((_TickTime*)A);
   _TickTime& b = *((_TickTime*)B);

   if (a.seconds < b.seconds) {
      return -1;
   } else if (a.seconds > b.seconds) {
      return 1;
   }
   return 0;
}


///////////////////////////////////////////////////////////////////////////
//
// Static functions:
//


//////////////////////////////
//
// MidiFile::readLittleEndian4Bytes -- Read four bytes which are in
//      little-endian order (smallest byte is first).  Then flip
//      the order of the bytes to create the return value.
//

ulong MidiFile::readLittleEndian4Bytes(istream& input) {
   uchar buffer[4] = {0};
   input.read((char*)buffer, 4);
   if (input.eof()) {
      cerr << "Error: unexpected end of file." << endl;
      exit(1);
   }
   return buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);
}



//////////////////////////////
//
// MidiFile::readLittleEndian2Bytes -- Read two bytes which are in
//       little-endian order (smallest byte is first).  Then flip
//       the order of the bytes to create the return value.
//

ushort MidiFile::readLittleEndian2Bytes(istream& input) {
   uchar buffer[2] = {0};
   input.read((char*)buffer, 2);
   if (input.eof()) {
      cerr << "Error: unexpected end of file." << endl;
      exit(1);
   }
   return buffer[1] | (buffer[0] << 8);
}



//////////////////////////////
//
// MidiFile::readByte -- Read one byte from input stream.  Exit if there
//     was an error.
//

uchar MidiFile::readByte(istream& input) {
   uchar buffer[1] = {0};
   input.read((char*)buffer, 1);
   if (input.eof()) {
      cerr << "Error: unexpected end of file." << endl;
      exit(1);
   }
   return buffer[0];
}


