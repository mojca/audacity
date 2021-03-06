/**********************************************************************

  Audacity: A Digital Audio Editor

  WaveClip.h

  ?? Dominic Mazzoni
  ?? Markus Meyer

*******************************************************************/

#ifndef __AUDACITY_WAVECLIP__
#define __AUDACITY_WAVECLIP__

#include "Audacity.h"
#include "SampleFormat.h"
#include "widgets/ProgressDialog.h"
#include "ondemand/ODTaskThread.h"
#include "xml/XMLTagHandler.h"

#include "Experimental.h"
#ifdef EXPERIMENTAL_USE_REALFFTF
#include "RealFFTf.h"
#endif

#include <wx/gdicmn.h>
#include <wx/longlong.h>
#include <wx/list.h>
#include <wx/msgdlg.h>

#include <vector>

class BlockArray;
class DirManager;
class Envelope;
class Sequence;
class SpectrogramSettings;
class WaveCache;
class WaveTrackCache;

class SpecCache {
public:

   // Make invalid cache
   SpecCache()
      : len(-1)
      , algorithm(-1)
      , pps(-1.0)
      , start(-1.0)
      , windowType(-1)
      , windowSize(-1)
      , zeroPaddingFactor(-1)
      , frequencyGain(-1)
#if 0
      , freq(NULL)
      , where(NULL)
#endif
      , dirty(-1)
   {
   }

   // Make valid cache, to be filled in
   SpecCache(int cacheLen, int algorithm_,
      double pps_, double start_, int windowType_, int windowSize_,
      int zeroPaddingFactor_, int frequencyGain_)
      : len(cacheLen)
      , algorithm(algorithm_)
      , pps(pps_)
      , start(start_)
      , windowType(windowType_)
      , windowSize(windowSize_)
      , zeroPaddingFactor(zeroPaddingFactor_)
      , frequencyGain(frequencyGain_)

      // len columns, and so many rows, column-major.
      // Don't take column literally -- this isn't pixel data yet, it's the
      // raw data to be mapped onto the display.
      , freq(len * ((windowSize * zeroPaddingFactor) / 2))

      // Sample counts corresponding to the columns, and to one past the end.
      , where(len + 1)

      , dirty(-1)
   {
      where[0] = 0;
   }

   ~SpecCache()
   {
   }

   bool Matches(int dirty_, double pixelsPerSecond,
      const SpectrogramSettings &settings, double rate) const;

   bool CalculateOneSpectrum
      (const SpectrogramSettings &settings,
       WaveTrackCache &waveTrackCache,
       int xx, sampleCount numSamples,
       double offset, double rate, double pixelsPerSecond,
       int lowerBoundX, int upperBoundX,
       const std::vector<float> &gainFactors,
       float *scratch);

   void Populate
      (const SpectrogramSettings &settings, WaveTrackCache &waveTrackCache,
       int copyBegin, int copyEnd, int numPixels,
       sampleCount numSamples,
       double offset, double rate, double pixelsPerSecond);

   const int          len; // counts pixels, not samples
   const int          algorithm;
   const double       pps;
   const double       start;
   const int          windowType;
   const int          windowSize;
   const int          zeroPaddingFactor;
   const int          frequencyGain;
   std::vector<float> freq;
   std::vector<sampleCount> where;

   int          dirty;
};

class SpecPxCache {
public:
   SpecPxCache(int cacheLen)
   {
      len = cacheLen;
      values = new float[len];
      valid = false;
      scaleType = 0;
      range = gain = -1;
      minFreq = maxFreq = -1;
   }

   ~SpecPxCache()
   {
      delete[] values;
   }

   sampleCount  len;
   float       *values;
   bool         valid;

   int scaleType;
   int range;
   int gain;
   int minFreq;
   int maxFreq;
};

class WaveClip;

WX_DECLARE_USER_EXPORTED_LIST(WaveClip, WaveClipList, AUDACITY_DLL_API);
WX_DEFINE_USER_EXPORTED_ARRAY_PTR(WaveClip*, WaveClipArray, class AUDACITY_DLL_API);

// A bundle of arrays needed for drawing waveforms.  The object may or may not
// own the storage for those arrays.  If it does, it destroys them.
class WaveDisplay
{
public:
   int width;
   sampleCount *where;
   float *min, *max, *rms;
   int* bl;

   std::vector<sampleCount> ownWhere;
   std::vector<float> ownMin, ownMax, ownRms;
   std::vector<int> ownBl;

public:
   WaveDisplay(int w)
      : width(w), where(0), min(0), max(0), rms(0), bl(0)
   {
   }

   // Create "own" arrays.
   void Allocate()
   {
      ownWhere.resize(width + 1);
      ownMin.resize(width);
      ownMax.resize(width);
      ownRms.resize(width);
      ownBl.resize(width);

      where = &ownWhere[0];
      if (width > 0) {
         min = &ownMin[0];
         max = &ownMax[0];
         rms = &ownRms[0];
         bl = &ownBl[0];
      }
      else {
         min = max = rms = 0;
         bl = 0;
      }
   }

   ~WaveDisplay()
   {
   }
};

class AUDACITY_DLL_API WaveClip final : public XMLTagHandler
{
private:
   // It is an error to copy a WaveClip without specifying the DirManager.
   // We define these break-inducing single-arg methods so that
   // if some developer makes the mistake of calling a single-arg copy
   // constructor rather than the one below (that requires a DirManager*),
   // rather than it going to C++-generated default copy constructor,
   // it goes here and the error is made clear to that developer.
   WaveClip(const WaveClip&)
   {
      wxFAIL_MSG(wxT("It is an error to copy a WaveClip without specifying the DirManager."));
   }
   WaveClip& operator=(const WaveClip& orig)
   {
      WaveClip bogus(orig);
      return *this;
   }

public:
   // typical constructor
   WaveClip(DirManager *projDirManager, sampleFormat format, int rate);

   // essentially a copy constructor - but you must pass in the
   // current project's DirManager, because we might be copying
   // from one project to another
   WaveClip(const WaveClip& orig, DirManager *projDirManager);

   virtual ~WaveClip();

   void ConvertToSampleFormat(sampleFormat format);

   void TimeToSamplesClip(double t0, sampleCount *s0) const;
   int GetRate() const { return mRate; }

   // Set rate without resampling. This will change the length of the clip
   void SetRate(int rate);

   // Resample clip. This also will set the rate, but without changing
   // the length of the clip
   bool Resample(int rate, ProgressDialog *progress = NULL);

   void SetOffset(double offset);
   double GetOffset() const { return mOffset; }
   void Offset(double delta) { SetOffset(GetOffset() + delta); }
   double GetStartTime() const;
   double GetEndTime() const;
   sampleCount GetStartSample() const;
   sampleCount GetEndSample() const;
   sampleCount GetNumSamples() const;

   // One and only one of the following is true for a given t (unless the clip
   // has zero length -- then BeforeClip() and AfterClip() can both be true).
   // Within() is true if the time is substantially within the clip
   bool WithinClip(double t) const;
   bool BeforeClip(double t) const;
   bool AfterClip(double t) const;

   bool GetSamples(samplePtr buffer, sampleFormat format,
                   sampleCount start, sampleCount len) const;
   bool SetSamples(samplePtr buffer, sampleFormat format,
                   sampleCount start, sampleCount len);

   Envelope* GetEnvelope() { return mEnvelope; }
   BlockArray* GetSequenceBlockArray();

   // Get low-level access to the sequence. Whenever possible, don't use this,
   // but use more high-level functions inside WaveClip (or add them if you
   // think they are useful for general use)
   Sequence* GetSequence() { return mSequence; }

   /** WaveTrack calls this whenever data in the wave clip changes. It is
    * called automatically when WaveClip has a chance to know that something
    * has changed, like when member functions SetSamples() etc. are called. */
   void MarkChanged() { mDirty++; }

   /// Create clip from copy, discarding previous information in the clip
   bool CreateFromCopy(double t0, double t1, WaveClip* other);

   /** Getting high-level data from the for screen display and clipping
    * calculations and Contrast */
   bool GetWaveDisplay(WaveDisplay &display,
                       double t0, double pixelsPerSecond, bool &isLoadingOD);
   bool GetSpectrogram(WaveTrackCache &cache,
                       const float *& spectrogram, const sampleCount *& where,
                       int numPixels,
                       double t0, double pixelsPerSecond);
   bool GetMinMax(float *min, float *max, double t0, double t1);
   bool GetRMS(float *rms, double t0, double t1);

   // Set/clear/get rectangle that this WaveClip fills on screen. This is
   // called by TrackArtist while actually drawing the tracks and clips.
   void ClearDisplayRect();
   void SetDisplayRect(const wxRect& r);
   void GetDisplayRect(wxRect* r);

   /** Whenever you do an operation to the sequence that will change the number
    * of samples (that is, the length of the clip), you will want to call this
    * function to tell the envelope about it. */
   void UpdateEnvelopeTrackLen();

   /// You must call Flush after the last Append
   bool Append(samplePtr buffer, sampleFormat format,
               sampleCount len, unsigned int stride=1,
               XMLWriter* blockFileLog = NULL);
   /// Flush must be called after last Append
   bool Flush();

   bool AppendAlias(const wxString &fName, sampleCount start,
                    sampleCount len, int channel,bool useOD);

   bool AppendCoded(const wxString &fName, sampleCount start,
                            sampleCount len, int channel, int decodeType);

   /// This name is consistent with WaveTrack::Clear. It performs a "Cut"
   /// operation (but without putting the cutted audio to the clipboard)
   bool Clear(double t0, double t1);

   /// Clear, and add cut line that starts at t0 and contains everything until t1.
   bool ClearAndAddCutLine(double t0, double t1);

   /// Paste data from other clip, resampling it if not equal rate
   bool Paste(double t0, const WaveClip* other);

   /** Insert silence - note that this is an efficient operation for large
    * amounts of silence */
   bool InsertSilence(double t, double len);

   /// Get access to cut lines list
   WaveClipList* GetCutLines() { return &mCutLines; }

   /** Find cut line at (approximately) this position. Returns true and fills
    * in cutLineStart and cutLineEnd (if specified) if a cut line at this
    * position could be found. Return false otherwise. */
   bool FindCutLine(double cutLinePosition,
                    double* cutLineStart = NULL,
                    double *cutLineEnd = NULL);

   /** Expand cut line (that is, re-insert audio, then DELETE audio saved in
    * cut line). Returns true if a cut line could be found and sucessfully
    * expanded, false otherwise */
   bool ExpandCutLine(double cutLinePosition);

   /// Remove cut line, without expanding the audio in it
   bool RemoveCutLine(double cutLinePosition);
   void RemoveAllCutLines();

   /// Offset cutlines right to time 't0' by time amount 'len'
   void OffsetCutLines(double t0, double len);

   /// Lock all blockfiles
   void Lock();
   void CloseLock(); //similar to Lock but should be called when the project closes.
   /// Unlock all blockfiles
   void Unlock();

   ///Delete the wave cache - force redraw.  Thread-safe
   void DeleteWaveCache();

   ///Adds an invalid region to the wavecache so it redraws that portion only.
   void AddInvalidRegion(long startSample, long endSample);

   //
   // XMLTagHandler callback methods for loading and saving
   //

   virtual bool HandleXMLTag(const wxChar *tag, const wxChar **attrs);
   virtual void HandleXMLEndTag(const wxChar *tag);
   virtual XMLTagHandler *HandleXMLChild(const wxChar *tag);
   virtual void WriteXML(XMLWriter &xmlFile);

   // Cache of values to colour pixels of Spectrogram - used by TrackArtist
   SpecPxCache    *mSpecPxCache;

   // AWD, Oct 2009: for pasting whitespace at the end of selection
   bool GetIsPlaceholder() const { return mIsPlaceholder; }
   void SetIsPlaceholder(bool val) { mIsPlaceholder = val; }

protected:
   wxRect mDisplayRect;

   double mOffset;
   int mRate;
   int mDirty;
   bool mIsCutLine;
   Sequence *mSequence;
   Envelope *mEnvelope;

   WaveCache    *mWaveCache;
   ODLock       mWaveCacheMutex;
   SpecCache    *mSpecCache;
   SampleBuffer  mAppendBuffer;
   sampleCount   mAppendBufferLen;

   // Cut Lines are nothing more than ordinary wave clips, with the
   // offset relative to the start of the clip.
   WaveClipList mCutLines;

   // AWD, Oct. 2009: for whitespace-at-end-of-selection pasting
   bool mIsPlaceholder;
};

#endif
