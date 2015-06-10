/*****************************************************************************
 * Copyright 2015 Haye Hinrichsen, Christoph Wick
 *
 * This file is part of Entropy Piano Tuner.
 *
 * Entropy Piano Tuner is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * Entropy Piano Tuner is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Entropy Piano Tuner. If not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

//=============================================================================
//                       A simple sine wave synthesizer
//=============================================================================

#include "synthesizer.h"

#include <algorithm>

#include "../system/log.h"
#include "../math/mathtools.h"
#include "../system/eptexception.h"
#include "../system/timer.h"


Sound::Sound() :
    mChannels(0),
    mSampleRate(0),
    mSineWave(),
    mSpectrum(),
    mStereo(0.5),
    mTime(0),
    mWaveForm(),
    mReady(false)
{}

void Sound::set (const int channels, const int samplerate, const WaveForm &sinewave,
                 const Spectrum &spectrum, const double stereo, const double time)
{
    mChannels = channels;
    mSampleRate = samplerate;
    mSineWave = sinewave;
    mSpectrum = spectrum;
    mStereo = stereo;
    mTime = time;
    mWaveForm.clear();
    mReady = false;
    start();
}


void Sound::workerFunction()
{
    auto round = [] (double x) { return static_cast<int64_t>(x+0.5); };
    const int64_t SampleLength = round(mSampleRate * mTime);
    const int64_t BufferSize = SampleLength * mChannels;
    const double leftvol  = sqrt(0.7-0.4*mStereo);
    const double rightvol = sqrt(0.3+0.4*mStereo);
    mWaveForm.resize(BufferSize);
    mWaveForm.assign(BufferSize,0);

    double sum=0;
    for (auto &mode : mSpectrum) sum+=mode.second;
    if (sum<=0) return;

    const int64_t SineLength = mSineWave.size();
    for (auto &mode : mSpectrum)
    {
        const double f = mode.first;
        const double volume = pow(mode.second / sum,0.4);

        if (f>24 and f<10000 and volume>0.001)
        {
            const int64_t periods = round((SampleLength * f) / mSampleRate);
            if (mChannels==1)
            {
                const int64_t phase = rand();
                for (int64_t i=0; i<SampleLength; ++i)
                    mWaveForm[i] += volume *
                        mSineWave[((i*periods*SineLength)/SampleLength+phase)%SineLength];
            }
            else if (mChannels==2)
            {
                const int64_t phasediff = round(periods * mSampleRate *
                                                (0.5-mStereo) / 500);
                const int64_t leftphase  = rand();
                const int64_t rightphase = leftphase + phasediff;
                for (int64_t i=0; i<SampleLength; ++i)
                {
                    mWaveForm[2*i] += volume * leftvol *
                        mSineWave[((i*periods*SineLength)/SampleLength+leftphase)%SineLength];
                    mWaveForm[2*i+1] += volume * rightvol *
                        mSineWave[((i*periods*SineLength)/SampleLength+rightphase)%SineLength];
                }
            }
        }
    }
    mReady = true;
    LogI ("Created waveform");

//    if (id !=0) return;
//    std::ofstream os("0000-waveform.dat");
//    for (int64_t i=0; i<SampleLength; ++i)  os << mWaveForms[0][2*i] << std::endl;
//    os << "&" << std::endl;
//    for (int64_t i=0; i<SampleLength; ++i)  os << mWaveForms[0][2*i+1] << std::endl;
//    os.close();


}

//-----------------------------------------------------------------------------
//	                             Constructor
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Constructor, intitializes the member variables.
///
/// \param audioadapter : Pointer to the implementation of the AudioPlayer
///////////////////////////////////////////////////////////////////////////////

Synthesizer::Synthesizer (AudioPlayerAdapter *audioadapter) :
    mSineWave(SineLength),
    mChord(),
    mChordMutex(),
    mAudioPlayer(audioadapter)
{
}


//-----------------------------------------------------------------------------
//	                      Initialize and start thread
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Initialize and start the synthesizer.
///
/// This function initializes the synthesizer in that it pre-calculates a sine
/// function and starts the main loop of the synthesizer in an indpendent thread.
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::init ()
{
    if (mAudioPlayer)
    {
        // Pre-calculate a sine wave for speedup
        mSineWave.resize(SineLength);
        for (int i=0; i<SineLength; ++i)
            mSineWave[i]=(float)(sin(MathTools::TWO_PI * i / SineLength));

    }
    else LogW("Could not start synthesizer: AudioPlayer not connected.");
}


//-----------------------------------------------------------------------------
//	                             Shut down
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Stop the synthesizer, request its execution thread to terminate.
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::exit ()
{
    stop();
}


//-----------------------------------------------------------------------------
//	                    Main Loop (worker function)
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Main loop of the synthesizer running in an independent thread.
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::workerFunction (void)
{
    setThreadName("Synthesizer");
    while (not cancelThread())
    {
        mChordMutex.lock();
        bool active = (mAudioPlayer and not mChord.empty());
        mChordMutex.unlock();
        if (active)
        {
            // first remove all sounds with an amplitude below the cutoff:
            mChordMutex.lock();
            for (auto it = mChord.begin(); it != mChord.end(); )
                if (it->second.stage>=2 and it->second.amplitude<CutoffVolume)
                { mChord.erase(it++); }
                else ++it;
            mChordMutex.unlock();

            generateWaveform();
        }
        else
        {
            createWaveforms();
            msleep(10);
        }
    }
}


//-----------------------------------------------------------------------------
//	                     Create a new sound
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Create a new sound (note).
///
/// This function creates or new (or recreates an existing) sound.
/// \param id : Identifier of the sound (usually the piano key number)
/// \param volume : Overall volume of the sound (intensity of keypress)
/// with typical values between 0 and 1.
/// \param stereo : Stereo position of the sound, ranging from 0 (left) to 1 (right).
/// \param attack : Rate of initial volume increase in units of 1/sec.
/// \param decayrate : Rate of the subsequent volume decrease in units of 1/sec.
/// If this rate is zero the decay phase is omitted and the volume
/// increases directly towards the sustain level controlled by the attack rate.
/// \param sustain : Level at which the volume saturates after decay in (0..1).
/// \param release : Rate at which the sound disappears after release in units of 1/sec.
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::createSound (int id, double volume, double stereo,
        double attack, double decayrate, double sustain, double release)
{
    std::cout << "create sound *********************************** " << (id%2 ? "old" : "new") << std::endl;
    mChordMutex.lock();
    mChord[id].amplitude=0;
    mChord[id].clock=0;
    mChord[id].fouriermodes.clear();
    mChord[id].stage=0;
    mChord[id].volume=volume;
    mChord[id].stereo=stereo;
    mChord[id].attack=attack;
    mChord[id].decayrate=decayrate;
    mChord[id].sustain=sustain;
    mChord[id].release=release;
    mChordMutex.unlock();
}

//-----------------------------------------------------------------------------
//	                        Generate the waveform
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Generate waveform.
///
/// This is the heart of the synthesizer. It fills the circular buffer
/// until it reaches the maximum size. It consists of two parts.
/// First the envelope is computed, rendering the actual amplitude of the
/// sound. Then a loop over all Fourier modes is carried out and a sine
/// wave with the corresponding frequency is added to the buffer.
/// \param snd : Reference of the sound to be converted.
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::generateWaveform ()
{
    // If there is nothing to play return
    if (mChord.size()==0) return;  /// ACHTUNG MUTEX *****************

    int SampleRate = mAudioPlayer->getSamplingRate();
    int channels = mAudioPlayer->getChannelCount();
    if (channels<=0 or channels>2) return;

    int64_t c = static_cast<int64_t>(100*SampleRate);
    int64_t d = static_cast<int64_t>(SineLength);

    int writtenSamples = 0;

    while (mAudioPlayer->getFreeSize()>=2 and writtenSamples < 10)
    {
        ++writtenSamples;
        mChordMutex.lock();
        double left=0, right=0, mono=0;
        for (auto &ch : mChord)
        {
            int id = ch.first;
            Tone &snd = ch.second;         // get sound of the key
            double y = snd.amplitude;       // get last amplitude
            switch (snd.stage)          // Manage ADSR
            {
                case 1: // ATTACK
                        y += snd.attack*snd.volume/SampleRate;
                        if (snd.decayrate>0)
                        {
                            if (y >= snd.volume) snd.stage++;
                        }
                        else
                        {
                            if (y >= snd.sustain*snd.volume) snd.stage+=2;
                        }
                        break;
                case 2: // DECAY
                        y *= (1-snd.decayrate/SampleRate); // DECAY
                        if (y <= snd.sustain*snd.volume) snd.stage++;
                        break;
                case 3: // SUSTAIN
                        y += (snd.sustain-y) * snd.release/SampleRate;
                        break;
                case 4: // RELEASE
                        y *= (1-snd.release/SampleRate);
                        break;
            }
            snd.amplitude = y;
            snd.clock ++;

            if (id%2 or id>=88 or id<0)
            {
                // compute stereo amplitude factors
                double leftvol=sqrt(0.7-0.4*snd.stereo);
                double rightvol=sqrt(0.3+0.4*snd.stereo);
                int64_t phase = static_cast<int64_t>((snd.stereo-0.5)*SampleRate)/800;

                // time-critical loop
                for (auto &mode : snd.fouriermodes)
                {
                    int64_t a = static_cast<int64_t>(100.0*mode.first*d);
                    int64_t b = static_cast<int64_t>(100.0*(mode.first*snd.clock+100)*d) + 100*d*c;
                    int64_t p = b + a*phase;
                    if (channels==1) mono += mode.second*y*mSineWave[(b/c)%d];
                    else // if stereo
                    {
                        left  += mode.second*leftvol *y*mSineWave[(b/c)%d];
                        right += mode.second*rightvol*y*mSineWave[(p/c)%d];
                    }
                }
            }
            else
            {
                int size = mWaveForms[id].size();
                if (size!=0)
                {
                    left  = y*mWaveForms[id][(2*snd.clock)%size];
                    right = y*mWaveForms[id][(2*snd.clock+1)%size];
                }
            }
        }
        mChordMutex.unlock();
        if (channels==1) mAudioPlayer->pushSingleSample(static_cast<AudioBase::PacketDataType>(mono));
        else // if stereo
        {
            mAudioPlayer->pushSingleSample(static_cast<AudioBase::PacketDataType>(left));
            mAudioPlayer->pushSingleSample(static_cast<AudioBase::PacketDataType>(right));
        }
    }
}


//-----------------------------------------------------------------------------
// 	                      Get sound (private)
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Get a pointer to the sound addressed by a given ID.
///
/// Note that this function has to be mutexed.
///
/// \param id : Identifier of the sound
///////////////////////////////////////////////////////////////////////////////

Synthesizer::Tone* Synthesizer::getSoundPtr (int id)
{
    auto snd = mChord.find(id);
    if (snd!=mChord.end()) return &(snd->second);
    else return nullptr;
}


//-----------------------------------------------------------------------------
// 	                Add a Fourier component to a sound
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Add a Fourier component to a sound.
///
/// This function adds a Fourier component (sine wave) to an existing sound
/// identified by an integer 'id'. The function is only carried out if the
/// sound with the identifier 'id' has already been created (see Create Sound)
/// or if the sound is still active, otherwise the function call is ignored.
///
/// \param id : identity tag of the sound (number of key).
/// \param f : Frequency of the spectral line in Hz.
/// \param amplitude : Amplitude of the spectral line.
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::addFourierComponent (int id, double f, double amplitude)
{
    mChordMutex.lock();
    auto snd = getSoundPtr(id);
    if (snd) snd->fouriermodes[f]=amplitude;
    else LogW("id does not exist");
    mChordMutex.unlock();
}



//-----------------------------------------------------------------------------
// 	                         Play a sound
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Play a sound.
///
/// This function marks the sound as ready for being played in the main loop.
///
/// \param id : identity tag of the sound (number of key).
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::playSound (int id)
{
    mChordMutex.lock();
    auto snd = getSoundPtr(id);
    if (snd) snd->stage = 1;
    else LogW("id does not exist");
    mChordMutex.unlock();
}


//-----------------------------------------------------------------------------
// 	                         Terminate a sound
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Terminate a sound.
///
/// This function forces the sound to fade out, entering the release phase.
///
/// \param id : identity tag of the sound (number of key).
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::releaseSound (int id)
{
    mChordMutex.lock();
    auto snd = getSoundPtr(id);
    if (snd) snd->stage=4;
    else LogW("id does not exist");
    mChordMutex.unlock();
}


//-----------------------------------------------------------------------------
// 	             Check whether a certain sound is still active
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Check whether a sound with given id is still playing.
///
/// \param id : Identifier of the sound
/// \return Boolean telling whether the sound is still playing.
///////////////////////////////////////////////////////////////////////////////

bool Synthesizer::isPlaying (int id)
{
    mChordMutex.lock();
    bool isplaying = (mChord.find(id) != mChord.end());
    mChordMutex.unlock();
    return isplaying;
}


//-----------------------------------------------------------------------------
// 	                       Change the sustain level
//-----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
/// \brief Change the level (sustain level) of a constantly playing sound
///
/// This function changes the sustain volume of a constantly playing sound
/// The synthesizer will adjust to the new volume adiabatically with the
/// respective decay rate.
///
/// \param id : Identity tag of the sound (number of key).
/// \param level : New sustain level in (0...1).
///////////////////////////////////////////////////////////////////////////////

void Synthesizer::ModifySustainLevel (int id, double level)
{
    mChordMutex.lock();
    auto snd = getSoundPtr(id);
    if (snd) snd->sustain = level;
    else LogW ("id does not exist");
    mChordMutex.unlock();
}



////-----------------------------------------------------------------------------

void Synthesizer::registerSoundForCreation  (const int id,
                                             const Spectrum &spectrum,
                                             const double stereo,
                                             const double time)
{
    (mAudioPlayer->getChannelCount(),
                                      mAudioPlayer->getSamplingRate(),
                                      &mSineWave, spectrum, stereo, time);mRegistrationQueueMutex.lock();
    mRegistrationQueueMutex.unlock();
    mRegistrationQueue[id]=snd
    mRegistrationQueueMutex.unlock();
    LogI ("*************** register ******************");
    std::cout << mRegistrationQueue.size() << std::endl;
}


////-----------------------------------------------------------------------------


//void Synthesizer::createWaveforms ()
//{
//    int64_t SampleRate = mAudioPlayer->getSamplingRate();
//    int channels = mAudioPlayer->getChannelCount();
//    if (channels<=0 or channels>2) return;

//    mRegistrationQueueMutex.lock();
//    if (mRegistrationQueue.size()==0) {  mRegistrationQueueMutex.unlock(); return; }
//    const int id = mRegistrationQueue.begin()->first;
//    const Sound tone = mRegistrationQueue.begin()->second;
//    mRegistrationQueue.erase(mRegistrationQueue.begin()); // erase
//    mRegistrationQueueMutex.unlock();

//    auto round = [] (double x) { return static_cast<int64_t>(x+0.5); };
//    const int64_t SampleLength = round(SampleRate * tone.time);
//    const int64_t BufferSize = SampleLength * channels;
//    const double leftvol  = sqrt(0.7-0.4*tone.stereo);
//    const double rightvol = sqrt(0.3+0.4*tone.stereo);
//    WaveForm buffer (BufferSize,0);

//    double sum=0;
//    for (auto &mode : tone.spectrum) sum+=mode.second;
//    if (sum<=0) return;


//    for (auto &mode : tone.spectrum)
//    {
//        const double f = mode.first;
//        const double volume = pow(mode.second / sum,0.4);

//        if (f>24 and f<10000 and volume>0.001)
//        {
//            const int64_t periods = round((SampleLength * f) / SampleRate);
//            if (channels==1)
//            {
//                const int64_t phase = rand();
//                for (int64_t i=0; i<SampleLength; ++i)
//                    buffer[i] += volume *
//                        mSineWave[((i*periods*SineLength)/SampleLength+phase)%SineLength];
//            }
//            else if (channels==2)
//            {
//                const int64_t phasediff = round(periods * SampleRate *
//                                                (0.5-tone.stereo) / 500);
//                const int64_t leftphase  = rand();
//                const int64_t rightphase = leftphase + phasediff;
//                for (int64_t i=0; i<SampleLength; ++i)
//                {
//                    buffer[2*i] += volume * leftvol *
//                        mSineWave[((i*periods*SineLength)/SampleLength+leftphase)%SineLength];
//                    buffer[2*i+1] += volume * rightvol *
//                        mSineWave[((i*periods*SineLength)/SampleLength+rightphase)%SineLength];
//                }
//            }
//        }
//    }

//    mWaveForms[id] = buffer;
//    LogI ("Created waveform %d", id);

////    if (id !=0) return;
////    std::ofstream os("0000-waveform.dat");
////    for (int64_t i=0; i<SampleLength; ++i)  os << mWaveForms[0][2*i] << std::endl;
////    os << "&" << std::endl;
////    for (int64_t i=0; i<SampleLength; ++i)  os << mWaveForms[0][2*i+1] << std::endl;
////    os.close();

//}


