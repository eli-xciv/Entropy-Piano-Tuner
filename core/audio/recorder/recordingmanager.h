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
//                           Recording manager
//=============================================================================

#ifndef RECORDINGMANAGER_H
#define RECORDINGMANAGER_H

#include "../../messages/messagelistener.h"
#include "audiorecorderadapter.h"


class RecordingManager :  public MessageListener
{
public:
    RecordingManager (AudioRecorderAdapter *audioRecorder);

    void init () {};

private:
    virtual void handleMessage(MessagePtr m);

    AudioRecorderAdapter *mAudioRecorder;   ///< Pointer to the audio device
    const Piano* mPiano;                    ///< Poitner to the actual piano
    OperationMode mOperationMode;           ///< Current operation mode
    const Key* mSelectedKey;                ///< Currently selected key
    int mNumberOfKeys;                      ///< Total number of keys
    int mNumberOfSelectedKey;               ///< Number of actually selected key

    const double FPS_FAST = 30;             ///< Stroboscopic fps during recording
    const double FPS_SLOW = 10;             ///< Stroboscopic fps during non-recording

    void updateStroboscopicFrequencies();
};

#endif // RECORDINGMANAGER_H