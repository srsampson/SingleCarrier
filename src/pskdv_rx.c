/*---------------------------------------------------------------------------*\

  FILE........: pskdv_rx.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A 1600 baud QPSK Digital Voice modem library

\*---------------------------------------------------------------------------*/
/*
  Copyright (C) 2020 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "pskdv_internal.h"
#include "fir.h"

// Externals

extern struct PSK *e_psk;
extern complex float e_pilots[];
extern complex float e_fcenter;
extern complex e_phaseRx;
extern float e_alpha50_root[];

// Defines

#define PSK_NFILTER  (6 * PSK_M)
#define NT           5
#define NSW          4
#define P            4

// Prototypes

static void constellationToBits(int []);
static void demodulate(int [], bool *, float []);
static void downconvert(complex float [], complex float [], int);
static void receiveFilter(complex float [], complex float [], int);
static void frequencyShift(complex float [], complex float [], int, int);
static void receiveProcessor(complex float [], complex float [], int, bool);
static void pilotCorrelation(float *, float *, int, float);
static void frameSyncFineFreqEstimate(complex float [], int, bool, bool *);
static void updateCtSymbolBuffer(complex float [], int);
static void syncStateMachine(bool, bool *);
static float rxEstimatedTiming(complex float [], complex float [], int);
static void linearRegression(complex float *, complex float *, complex float []);

// Locals

/*
 * Linear Regression pilot sample point now, and future
 */
const int m_samplingPoints[] = {
    0, PSK_FRAME
};

static complex float m_rx_filter[NTAPS];
static complex float m_ctSymbBuf[(PSK_FRAME * PSK_CYCLES) * 2];
static complex float m_ctFrameBuf[4];
static complex float m_chFrameBuf[(PSK_FRAME * PSK_CYCLES) * 2];
static complex float m_prevRxSymbols;
static complex float m_rxFilterMemory[PSK_NFILTER];
static complex float m_rxFilterMemTiming[NT * P];
static float m_pskPhase[PSK_SYMBOLS];
static float m_freqOffsetFiltered;
static float m_rxTiming;
static float m_ratio;
static int m_sampleCenter;
static int m_syncTimer;

// Functions

/*
 * Convert to baseband from 1100 Hz center frequency window
 */
static void downconvert(complex float baseband[], complex float signal[],
        int lnin) {
    for (int i = 0; i < lnin; i++) {
        e_phaseRx *= e_fcenter;
        baseband[i] = signal[i] * conjf(e_phaseRx);
    }

    // Normalize

    e_phaseRx /= cabsf(e_phaseRx);
}

static void receiveFilter(complex float filtered[], complex float baseband[], int lnin) {
    int n = PSK_M / P;

    for (int i = 0, j = 0; i < lnin; i += n, j++) {
        /*
         * Move the new samples in from the right.
         */
        for (int k = PSK_NFILTER - n, l = i; k < PSK_NFILTER; k++, l++) {
            m_rxFilterMemory[k] = baseband[l];
        }

        filtered[j] = 0.0f;

        for (int k = 0; k < PSK_NFILTER; k++) {
            filtered[j] += (m_rxFilterMemory[k] * e_alpha50_root[k]);
        }

        for (int k = 0, l = n; k < (PSK_NFILTER - n); k++, l++) {
            m_rxFilterMemory[k] = m_rxFilterMemory[l];
        }
    }
}

/*
 * Shift the center frequency to new estimate
 */
static void frequencyShift(complex float waveform[], complex float signal[],
        int index, int lnin) {
    complex float rxPhase = cmplx(TAU * -e_psk->freqEstimate / PSK_FS);

    for (int i = 0; i < lnin; i++) {
        e_phaseRx *= rxPhase;
        waveform[i] = signal[index + i] * e_phaseRx;
    }

    e_phaseRx /= cabsf(e_phaseRx);
}

static void pilotCorrelation(float *corr_out, float *mag_out, int t, float f_fine) {
    complex float acorr = 0.0f;
    float mag = 0.0f;

    for (int i = 0; i < SAMPLING_POINTS; i++) {
        complex float freqFinePhase = cmplx(TAU * f_fine * (m_samplingPoints[i] + 1.0f) / PSK_RS);
        complex float freqCorr = m_ctSymbBuf[t + m_samplingPoints[i]] * freqFinePhase;

        acorr += (freqCorr * e_pilots[i]);
        mag += cabsf(freqCorr);
    }

    *corr_out = cabsf(acorr);
    *mag_out = mag;
}

static void syncStateMachine(bool sync, bool *nextSync) {
    if (sync == true) {
        float corr;
        float mag;

        pilotCorrelation(&corr, &mag, m_sampleCenter, e_psk->freqFineEstimate);

        m_ratio = fabsf(corr) / mag;

        if (m_ratio < 0.8f) {
            m_syncTimer++;
        } else {
            m_syncTimer = 0;
        }

        if (m_syncTimer == 10) {
            *nextSync = false;
        }
    }
}

/*
 * This function works on a per pilot range,
 * so only one row of data symbols is processed.
 */
static void constellationToBits(int bitPairs[]) {
    complex float rxSymbolLinear[PSK_SYMBOLS];
    complex float slope;
    complex float intercept;
    complex float y[2];

    /*
     * Interpolate phase
     */
    for (int i = 0; i < PSK_SYMBOLS; i++) {
        for (int j = 0; j < 2; j++) {
            y[j] = m_ctFrameBuf[m_samplingPoints[j]] * e_pilots[i];
        }

        linearRegression(&slope, &intercept, y);

        m_pskPhase[i] = cargf((slope * (float)(i)) + intercept);

        /* Adjust the phase of data symbols */

        rxSymbolLinear[i] = m_ctFrameBuf[i] * conjf(cmplx(m_pskPhase[i]));

        complex float rotate = rxSymbolLinear[i] * cmplx(ROT45);

        bitPairs[2 * i + 1] = crealf(rotate) < 0.0f;
        bitPairs[2 * i] = cimagf(rotate) < 0.0f;

    float mag = 0.0f;

    mag += cabsf(rxSymbolLinear[i]);

    e_psk->signalRMS = mag / PSK_SYMBOLS;
    }
    
    float sum_x = 0.0f;
    float sum_xx = 0.0f;
    int n = 0;

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        complex float s = rxSymbolLinear[i];

        if (fabsf(crealf(s)) > e_psk->signalRMS) {
            sum_x += cimagf(s);
            sum_xx += (cimagf(s) * cimagf(s));
            n++;
        }
    }

    if (n > 1) {
        e_psk->noiseRMS = sqrtf((n * sum_xx - sum_x * sum_x) / (n * (n - 1)));
    } else {
        e_psk->noiseRMS = 0.0f;
    }

    float new_snr_est = 20.0f * log10f((e_psk->signalRMS + 1E-6f)
            / (e_psk->noiseRMS + 1E-6f)) - 10.0f * log10f(3000.0f / 2400.0f);

    e_psk->snrEstimate = 0.9f * e_psk->snrEstimate + 0.1f * new_snr_est;
}

/*
 * Function to receive bits from 1-Channel Real 8 kHz sample rate signal
 * 
 * @param 1 an unsigned byte array of the decoded bits
 * @param 2 sync boolean to show sync state
 * @param 3 a real 1-Channel waveform 8 kHz sample rate signal
 */
void psk_receive(uint8_t packed_codec_bits[], bool *sync, float signal[]) {
    int bitPairs[PSK_DATA_BITS_PER_FRAME];

    /*
     * Drop the real user-level amplitude
     */
    for (int i = 0; i < (PSK_FRAME * PSK_CYCLES); i++) {
        signal[i] /= SCALE;
    }

    demodulate(bitPairs, sync, signal);

    if (*sync == true) {
        for (int j = 0, k = 0; j < PSK_DATA_BITS_PER_FRAME; j += (PSK_DATA_BITS_PER_FRAME / 2), k += 4) {
            int bit = 7;
            int nbyte = 0;

            for (int i = 0; i < 4; i++) {
                packed_codec_bits[k + i] = 0;
            }

            for (int i = 0; i < (PSK_DATA_BITS_PER_FRAME / 2); i++) {
                packed_codec_bits[nbyte + k] |= ((bitPairs[j + i] & 0x01) << bit);
                bit--;
                if (bit < 0) {
                    bit = 7;
                    nbyte++;
                }
            }
        }
    }
}

/*
 * Method to demodulate a frame to baseband, and show sync state
 *
 * @param rx_bits a unsigned byte array of the demodulated bits
 * @param sync a pointer showing sync state
 * @param signal an 8 kHz complex array of the modulated signal
 */
static void demodulate(int bitPairs[], bool *sync, float signal[]) {
    complex float ch_symb[PSK_FRAME * PSK_CYCLES];
    bool anextSync;

    bool lsync = e_psk->sync;
    bool nextSync = lsync;

    /*
     * Add the new signal 8 kHz samples to the Sync Window Buffer
     *
     * First move the old samples to the left. Then add in new symbol
     * as a complex value.
     */
    for (int i = 0; i < (PSK_FRAME * PSK_CYCLES); i++) {
        m_chFrameBuf[i] = m_chFrameBuf[(PSK_FRAME * PSK_CYCLES) + i];
        m_chFrameBuf[(PSK_FRAME * PSK_CYCLES) + i] = (signal[i] + 0.0f * I);
    }

    if (lsync == false) {
        /*
         * OK, we are not in sync, so we will
         * move the center frequency around and see if we
         * can find it. We only have +/- 40 Hz to do this.
         * Beyond that, the user will have to turn the dial.
         */
        float maxRatio = 0.0f;
        float freqEstimate = 0.0f;

        for (int i = -40; i <= 40; i += 40) {
            e_psk->freqEstimate = PSK_CENTER + (float) i;
            receiveProcessor(ch_symb, m_chFrameBuf, PSK_CYCLES, false);

            for (int j = 0; j < NSW - 1; j++) {
                updateCtSymbolBuffer(ch_symb, (PSK_FRAME * j));
            }

            frameSyncFineFreqEstimate(ch_symb, PSK_FRAME, lsync, &anextSync);

            if (anextSync == true) {
                if (m_ratio > maxRatio) {
                    maxRatio = m_ratio;
                    freqEstimate = (e_psk->freqEstimate - e_psk->freqFineEstimate);
                    nextSync = anextSync;
                }
            }
        }

        if (nextSync == true) {
            e_psk->freqEstimate = freqEstimate;
            receiveProcessor(ch_symb, m_chFrameBuf, PSK_CYCLES, false);

            for (int i = 0; i < NSW - 1; i++) {
                updateCtSymbolBuffer(ch_symb, (i * PSK_FRAME));
            }

            frameSyncFineFreqEstimate(ch_symb, ((NSW - 1) * PSK_FRAME), lsync, &nextSync);

            if (fabsf(e_psk->freqFineEstimate) > 2.0f) {
                nextSync = 0;
            }
        }

        if (nextSync == true) {
            /*
             * We are in sync finally
             */
            for (int r = 0; r < 4; r++) {
                m_ctFrameBuf[r] = m_ctSymbBuf[m_sampleCenter + r];
            }
        }
    } else if (lsync == true) {
        /*
         * Good deal, we were already in sync
         * so we can skip searching for the center frequency
         * Much less CPU now.
         */
        receiveProcessor(ch_symb, signal, e_psk->nin, true);
        frameSyncFineFreqEstimate(ch_symb, 0, lsync, &nextSync);

        for (int r = 0; r < 2; r++) {
            m_ctFrameBuf[r] = m_ctFrameBuf[PSK_FRAME + r];
        }

        for (int r = 2; r < 4; r++) {
            m_ctFrameBuf[r] = m_ctSymbBuf[m_sampleCenter + r];
        }
    }

    *sync = false;

    if ((nextSync == true) || (lsync == true)) {
        constellationToBits(bitPairs);
        *sync = true;
    }

    syncStateMachine(lsync, &nextSync);

    e_psk->sync = lsync = nextSync;

    /*
     * Work out how many samples we need for the next
     * call to adapt to differences between distant
     * transmitter and this receivers clock.
     */
    int lnin = PSK_M;

    if (nextSync == true) {
        if (m_rxTiming > PSK_M / P) {
            lnin = PSK_M + PSK_M / P;
        } else if (m_rxTiming < -PSK_M / P) {
            lnin = PSK_M - PSK_M / P;
        }
    }

    e_psk->nin = lnin;

    *nin_frame = (PSK_FRAME - 1) * PSK_CYCLES + lnin;
}

/*
 * RX Process on 8 kHz sample rate
 */
static void receiveProcessor(complex float symbols[], complex float signal[],
        int lnin, bool freqTrack) {
    complex float offsetSignal[PSK_M + PSK_M / P];
    complex float baseband[PSK_M + PSK_M / P];
    complex float rxFiltered[P];
    complex float rxOneFrame;
    complex float adiff;
    complex float modStrip;

    int index = 0;
    float adjustedRxTiming = 0.0f;

    for (int i = 0; i < (PSK_FRAME * PSK_CYCLES); i++) {
        frequencyShift(offsetSignal, signal, index, lnin);
        index += lnin;

        downconvert(baseband, offsetSignal, lnin);
        receiveFilter(rxFiltered, baseband, lnin);

        adjustedRxTiming = rxEstimatedTiming(&rxOneFrame, rxFiltered, lnin);

        symbols[i] = rxOneFrame;

        if (freqTrack == true) {
            modStrip = 0.0f;

            adiff = rxOneFrame * conjf(m_prevRxSymbols);
            m_prevRxSymbols = rxOneFrame;

            adiff = cpowf(adiff, 4.0f);
            modStrip += cabsf(adiff);

            m_freqOffsetFiltered = (1.0f - 0.005f) * m_freqOffsetFiltered +
                    0.005f * cargf(modStrip);

            e_psk->freqEstimate += (0.2f * m_freqOffsetFiltered);
        }

        if (lnin != PSK_M) {
            lnin = PSK_M;
        }
    }

    m_rxTiming = adjustedRxTiming;
}

static void frameSyncFineFreqEstimate(complex float ch_symb[], int offset,
        bool sync, bool *nextSync) {

    updateCtSymbolBuffer(ch_symb, offset);

    if (sync == false) {
        float corr;
        float mag;
        float max_corr = 0.0f;
        float max_mag = 0.0f;

        for (int j = -2000; j <= 2000; j += 25) {
            float f_fine = (float) j / 100.0f;

            for (int i = 0; i < PSK_FRAME; i++) {
                pilotCorrelation(&corr, &mag, i, f_fine);

                if (corr >= max_corr) {
                    max_corr = corr;
                    max_mag = mag;
                    
                    m_sampleCenter = i;
                    e_psk->freqFineEstimate = f_fine;
                }
            }
        }

        if (max_corr / max_mag > 0.9f) {
            m_syncTimer = 0;
            *nextSync = true;
        } else {
            *nextSync = false;
        }

        m_ratio = max_corr / max_mag;
    }
}

static void updateCtSymbolBuffer(complex float symbol[], int offset) {
    // Move old contents left
    
    for (int i = 0; i < PSK_FRAME; i++) {
        m_ctSymbBuf[i] = m_ctSymbBuf[PSK_FRAME + i];
    }

    // Add in new symbols
    
    for (int i = 0; i < PSK_FRAME; i++) {
        m_ctSymbBuf[PSK_FRAME + i] = symbol[offset + i];
    }
}

/*
 * Estimate optimum timing offset
 * re-filter receive symbols at optimum timing estimate.
 */
static float rxEstimatedTiming(complex float *symbol, complex float rxFiltered[],
        int lnin) {
    int adjust = P - lnin * P / PSK_M;

    /* Make room for new data, slide left 16 samples */

    for (int i = 0, j = P - adjust; i < (NT - 1) * P + adjust; i++, j++) {
        m_rxFilterMemTiming[i] = m_rxFilterMemTiming[j];
    }

    for (int i = (NT - 1) * P + adjust, j = 0; i < (NT * P); i++, j++) {
        m_rxFilterMemTiming[i] = rxFiltered[j];
    }

    complex float x = 0.0f;
    complex float phase = cmplx(0.0f);
    complex float freq = cmplx(TAU / P);
    float env[NT * P];

    for (int i = 0; i < (NT * P); i++) {
        env[i] = cabsf(m_rxFilterMemTiming[i]);
        
        x += (phase * env[i]);
        phase *= freq;
    }

    float adjustedRxTiming = cargf(x) / TAU;
    float rx_timing = adjustedRxTiming * (float) P + 1.0f;

    if (rx_timing > (float) P) {
        rx_timing -= (float) P;
    } else if (rx_timing < (float) -P) {
        rx_timing += (float) P;
    }

    rx_timing += floorf(NT / 2.0f) * (float) P;
    int low_sample = floorf(rx_timing);

    float fract = rx_timing - low_sample;
    int high_sample = ceilf(rx_timing);

    complex float t1 = m_rxFilterMemTiming[low_sample - 1] * (1.0f - fract);
    complex float t2 = m_rxFilterMemTiming[high_sample - 1] * fract;
    
    *symbol = t1 + t2;

    return adjustedRxTiming * (float) PSK_M;
}

static void linearRegression(complex float *slope, complex float *intercept, complex float y[]) {
    complex float sumxy = 0.0f;
    complex float sumy = 0.0f;
    float sumx = 0.0f;
    float sumx2 = 0.0f;

    for (int i = 0; i < SAMPLING_POINTS; i++) {
        sumx += (float) m_samplingPoints[i];
        sumx2 += ((float) m_samplingPoints[i] * (float) m_samplingPoints[i]);
        
        sumxy += ((float) m_samplingPoints[i] * y[i]);
        sumy += y[i];
    }

    float denom = (((float) SAMPLING_POINTS * sumx2) - (sumx * sumx));

    /*
     * fits y = mx + b to the pilots
     * x is the pilot now, and future
     */

    if (denom != 0.0f) {
        *slope = ((sumxy * (float) SAMPLING_POINTS) - (sumx * sumy)) / denom;
        *intercept = ((sumx2 * sumy) - (sumx * sumxy)) / denom;
    } else {
        *slope = 0.0f;
        *intercept = 0.0f;
    }
}
