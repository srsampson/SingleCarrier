/*---------------------------------------------------------------------------*\

  FILE........: pskrx.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A 1600 baud QPSK voice modem library

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

#include "psk_internal.h"
#include "fir.h"

// Externals

extern struct PSK *psk;
extern complex float pilots[];
extern complex float fcenter;
extern complex phaseRx;
extern float alpha50_root[];

// Defines

#define PSK_NFILTER  (6 * PSK_M)
#define NT           5
#define NSW          4
#define P            4
#define SCALE        8192.0f

// Prototypes

static void constellationToBits(int []);
static void demodulate(int [], int *, complex float [], int *);
static void downconvert(complex float [], complex float [], int);
static void receiveFilter(complex float [], complex float [], int);
static void frequencyShift(complex float [], complex float [], int, int);
static void receiveProcessor(complex float [], complex float [], int, int, int);
static void pilotCorrelation(float *, float *, int, float);
static void frameSyncFineFreqEstimate(complex float [], int, int, int *);
static void updateCtSymbolBuffer(complex float [], int);
static void syncStateMachine(int, int *);
static float rxEstimatedTiming(complex float [], complex float [], int);
static void linearRegression(complex float *, complex float *, float [], complex float []);

// Locals

static complex float rx_filter[NTAPS];
static complex float rxSymb[PSK_SYMBOLS];
static complex float ctSymbBuf[PSK_SYMBOL_BUF];
static complex float ctFrameBuf[PSK_SYMBOL_BUF];
static complex float chFrameBuf[PSK_FRAME * PSK_CYCLES];
static complex float prevRxSymbols;
static complex float rxFilterMemory[PSK_NFILTER];
static complex float rxFilterMemTiming[NT * P];
static float pskPhase[PSK_SYMBOLS];
static float freqOffsetFiltered;
static float rxTiming;
static float ratio;
static int sampleCenter;
static int syncTimer;

/*
 * Linear Regression X point values.
 * 0,1 for start, and 29,30 for end.
 * Algorithm will fit the rest.
 */
const int samplingPoints[] = {
    0, 1, 29, 30
};

// Functions

/*
 * Given a signal of complex samples,
 * return data bits packed into 8 bytes
 * and a sync indication
 */
int receive(uint8_t packed_codec_bits[], complex float signal[]) {
    int bitPairs[PSK_DATA_BITS_PER_FRAME];
    int nin = PSK_DATA_SYMBOLS_PER_FRAME;
    int sync;

    /*
     * Drop the user-level amplitude
     */
    for (int i = 0; i < nin; i++) {
        signal[i] /= SCALE;
    }

    demodulate(bitPairs, &sync, signal, &nin);

    if (sync) {
        for (int j = 0, k = 0; j < PSK_DATA_BITS_PER_FRAME; j += (PSK_DATA_BITS_PER_FRAME / 2), k += 4) { /* 0, 28 */
            int bit = 7;
            int nbyte = 0;

            for (int i = 0; i < 4; i++) {
                packed_codec_bits[k + i] = 0;
            }

            for (int i = 0; i < (PSK_DATA_BITS_PER_FRAME / 2); i++) { /* 0..28 */
                packed_codec_bits[nbyte + k] |= ((bitPairs[j + i] & 0x01) << bit);
                bit--;
                if (bit < 0) {
                    bit = 7;
                    nbyte++;
                }
            }
        }
    }

    return sync;
}

/*
 * Method to demodulate a frame to baseband, and show sync state
 *
 * @param rx_bits a unsigned byte array of the demodulated bits
 * @param sync_good a pointer showing sync state
 * @param signal a complex array of the modulated signal
 * @param nin_frame a pointer to an integer containing the new frame size
 */
static void demodulate(int bitPairs[], int *sync_good, complex float signal[], int *nin_frame) {
    complex float ch_symb[PSK_FRAME];
    int anextSync;

    int lsync = psk->m_sync; /* get a starting value */
    int nextSync = lsync;

    /*
     * Add the new signal samples to the Sync Window Buffer
     * which is 2400 complex time samples long.
     *
     * First move the old samples to the left.
     */
    for (int i = 0; i < (PSK_FRAME - *nin_frame); i++) {
        chFrameBuf[i] = chFrameBuf[i + *nin_frame];
    }

    /*
     * Now add in the new samples.
     */
    for (int i = 0; i < PSK_FRAME; i++) {
        chFrameBuf[i] = signal[i];
    }

    if (!lsync) {
        /*
         * OK, we are not in sync, so we will
         * move the center frequency around and see if we
         * can find it. We only have +/- 40 Hz to do this.
         * Beyond that, the user will have to turn the dial.
         */
        float maxRatio = 0.0f;
        float freqEstimate = 0.0f;

        for (int i = -40; i <= 40; i += 40) { // Don't use float in loops */
            psk->m_freqEstimate = PSK_CENTER + (float) i;
            receiveProcessor(ch_symb, chFrameBuf, PSK_FRAME, PSK_CYCLES, 0);

            for (int j = 0; j < NSW - 1; j++) {
                updateCtSymbolBuffer(ch_symb, (PSK_FRAME * j));
            }

            frameSyncFineFreqEstimate(ch_symb, PSK_FRAME, lsync, &anextSync);

            if (anextSync) {
                if (ratio > maxRatio) {
                    maxRatio = ratio;
                    freqEstimate = (psk->m_freqEstimate - psk->m_freqFineEstimate);
                    nextSync = anextSync;
                }
            }
        }

        if (nextSync) {
            psk->m_freqEstimate = freqEstimate;
            receiveProcessor(ch_symb, chFrameBuf, PSK_FRAME, PSK_CYCLES, 0);

            for (int i = 0; i < NSW - 1; i++) {
                updateCtSymbolBuffer(ch_symb, (i * PSK_FRAME));
            }

            frameSyncFineFreqEstimate(ch_symb, ((NSW - 1) * PSK_FRAME), lsync, &nextSync);

            if (fabsf(psk->m_freqFineEstimate) > 2.0f) {
                nextSync = 0;
            }
        }

        if (nextSync) {
            /*
             * We are in sync finally
             */
            for (int r = 0; r < PSK_SYMBOL_BUF; r++) {
                ctFrameBuf[r] = ctSymbBuf[sampleCenter + r];
            }
        }
    } else if (lsync) {
        /*
         * Good deal, we were already in sync
         * so we can skip searching for the center frequency
         * Much less CPU now.
         */
        receiveProcessor(ch_symb, signal, PSK_FRAME, psk->m_nin, 1);
        frameSyncFineFreqEstimate(ch_symb, 0, lsync, &nextSync);

        for (int r = 0; r < 2; r++) {
            ctFrameBuf[r] = ctFrameBuf[PSK_FRAME + r];
        }

        for (int r = 2; r < PSK_SYMBOL_BUF; r++) {
            ctFrameBuf[r] = ctSymbBuf[sampleCenter + r];
        }
    }

    *sync_good = 0;

    if (nextSync || lsync) {
        constellationToBits(bitPairs);
        *sync_good = 1;
    }

    syncStateMachine(lsync, &nextSync);

    psk->m_sync = lsync = nextSync; /* store the next sync value in the global */

    /*
     * Work out how many samples we need for the next
     * call to adapt to differences between distant
     * transmitter and this receivers clock.
     */
    int lnin = PSK_M;

    if (nextSync) {
        if (rxTiming > PSK_M / P) {
            lnin = PSK_M + PSK_M / P;
        } else if (rxTiming < -PSK_M / P) {
            lnin = PSK_M - PSK_M / P;
        }
    }

    psk->m_nin = lnin;

    *nin_frame = (PSK_FRAME - 1) * PSK_CYCLES + lnin;
}

/*
 * This function works on a per pilot range,
 * so only one row of data symbols is processed.
 */
static void constellationToBits(int bitPairs[]) {
    complex float y[PSK_SYMBOLS];
    complex float rxSymbolLinear[PSK_SYMBOLS];
    complex float slope;
    complex float intercept;
    float x[PSK_SYMBOLS];

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        x[i] = samplingPoints[i]; // 0, 1, 29, 30
        y[i] = ctFrameBuf[samplingPoints[i]] * pilots[i];
    }

    linearRegression(&slope, &intercept, x, y);

    for (int i = 0; i < PSK_SYMBOLS; i++) { /* 4 */
        pskPhase[i] = cargf((slope * (PSK_SYMBOLS + i)) + intercept);
    }

    /* Adjust the phase of data symbols */

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        complex float phi_rect = conjf(cmplx(pskPhase[i]));

        rxSymb[i] = ctFrameBuf[PSK_SYMBOLS + i] * phi_rect;
        rxSymbolLinear[PSK_SYMBOLS + i] = rxSymb[i];
    }

    /*
     * load the bits detected from the received symbols
     * these are the bit-pairs in the modem frame
     */

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        complex float rotate = rxSymb[i] * cmplx(ROT45);

        bitPairs[2 * i + 1] = crealf(rotate) < 0.0f;
        bitPairs[2 * i] = cimagf(rotate) < 0.0f;
    }

    float mag = 0.0f;

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        mag += cabsf(rxSymbolLinear[i]);
    }

    psk->m_signalRMS = mag / PSK_SYMBOLS;

    float sum_x = 0.0f;
    float sum_xx = 0.0f;
    int n = 0;

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        complex float s = rxSymbolLinear[i];

        if (fabsf(crealf(s)) > psk->m_signalRMS) {
            sum_x += cimagf(s);
            sum_xx += (cimagf(s) * cimagf(s));
            n++;
        }
    }

    if (n > 1) {
        psk->m_noiseRMS = sqrtf((n * sum_xx - sum_x * sum_x) / (n * (n - 1)));
    } else {
        psk->m_noiseRMS = 0.0f;
    }
}

static void downconvert(complex float baseband[], complex float offsetSignal[], int lnin) {
    for (int i = 0; i < lnin; i++) {
        phaseRx *= fcenter;
        baseband[i] = offsetSignal[i] * conjf(phaseRx);
    }

    // Normalize

    phaseRx /= cabsf(phaseRx);
}

static void receiveFilter(complex float filtered[], complex float baseband[],
        int lnin) {
    int n = PSK_M / P;

    for (int i = 0, j = 0; i < lnin; i += n, j++) {
        /*
         * Move the new samples in from the right.
         */
        for (int k = PSK_NFILTER - n, l = i; k < PSK_NFILTER; k++, l++) {
            rxFilterMemory[k] = baseband[l];
        }

        filtered[j] = 0.0f;

        for (int k = 0; k < PSK_NFILTER; k++) {
            filtered[j] += (rxFilterMemory[k] * alpha50_root[k]);
        }

        for (int k = 0, l = n; k < (PSK_NFILTER - n); k++, l++) {
            rxFilterMemory[k] = rxFilterMemory[l];
        }
    }
}

/*
 * Shift the center frequency
 */
static void frequencyShift(complex float waveform[], complex float signal[], int index, int lnin) {
    complex float rxPhase = cmplx(TAU * -psk->m_freqEstimate / PSK_FS);

    for (int i = 0; i < lnin; i++) {
        phaseRx *= rxPhase;
        waveform[i] = signal[index + i] * phaseRx;
    }

    phaseRx /= cabsf(phaseRx);
}

/*
 * RX Process
 */
static void receiveProcessor(complex float symbols[], complex float signal[],
        int nsymb, int lnin, int freqTrack) {
    complex float offsetSignal[PSK_M + PSK_M / P];
    complex float baseband[PSK_M + PSK_M / P];
    complex float rxFiltered[P];
    complex float rxOneFrame[1]; // lazy pointer
    complex float adiff;
    complex float modStrip;

    int index = 0;
    float adjustedRxTiming = 0.0f;

    for (int i = 0; i < nsymb; i++) {
        frequencyShift(offsetSignal, signal, index, lnin);
        index += lnin;

        downconvert(baseband, offsetSignal, lnin);
        receiveFilter(rxFiltered, baseband, lnin);

        adjustedRxTiming = rxEstimatedTiming(rxOneFrame, rxFiltered, lnin);

        symbols[i] = rxOneFrame[0];

        if (freqTrack) {
            modStrip = 0.0f;

            adiff = rxOneFrame[0] * conjf(prevRxSymbols);
            prevRxSymbols = rxOneFrame[0];

            adiff = cpowf(adiff, 4.0f);
            modStrip += cabsf(adiff);

            freqOffsetFiltered = (1.0f - 0.005f) * freqOffsetFiltered +
                    0.005f * cargf(modStrip);

            psk->m_freqEstimate += (0.2f * freqOffsetFiltered);
        }

        if (lnin != PSK_M) {
            lnin = PSK_M;
        }
    }

    rxTiming = adjustedRxTiming;
}

static void pilotCorrelation(float *corr_out, float *mag_out, int t, float f_fine) {
    complex float acorr = 0.0f;
    float mag = 0.0f;

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        complex float freqFinePhase = cmplx(TAU * f_fine * (samplingPoints[i] + 1.0f) / PSK_RS);
        complex float freqCorr = ctSymbBuf[t + samplingPoints[i]] * freqFinePhase;

        acorr += (freqCorr * pilots[i]);
        mag += cabsf(freqCorr);
    }

    *corr_out = cabsf(acorr);
    *mag_out = mag;
}

static void frameSyncFineFreqEstimate(complex float ch_symb[], int offset,
        int sync, int *nextSync) {

    updateCtSymbolBuffer(ch_symb, offset);

    if (!sync) {
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
                    sampleCenter = i;
                    psk->m_freqFineEstimate = f_fine;
                }
            }
        }

        if (max_corr / max_mag > 0.9f) {
            syncTimer = 0;
            *nextSync = 1;
        } else {
            *nextSync = 0;
        }

        ratio = max_corr / max_mag;
    }
}

static void updateCtSymbolBuffer(complex float symbol[], int offset) {
    for (int i = 0; i < (PSK_SYMBOL_BUF - PSK_FRAME); i++) {
        ctSymbBuf[i] = ctSymbBuf[PSK_FRAME + i];
    }

    for (int i = PSK_SYMBOL_BUF - PSK_FRAME, j = 0; i < PSK_SYMBOL_BUF; i++, j++) {
        ctSymbBuf[i] = symbol[offset + j];
    }
}

static void syncStateMachine(int sync, int *nextSync) {
    if (sync) {
        float corr;
        float mag;

        pilotCorrelation(&corr, &mag, sampleCenter, psk->m_freqFineEstimate);

        ratio = fabsf(corr) / mag;

        if (ratio < 0.8f) {
            syncTimer++;
        } else {
            syncTimer = 0;
        }

        if (syncTimer == 10) {
            *nextSync = 0;
        }
    }
}

/*
 * Estimate optimum timing offset
 * re-filter receive symbols at optimum timing estimate.
 */
static float rxEstimatedTiming(complex float symbol[], complex float rxFiltered[], int lnin) {
    float env[NT * P];

    int adjust = P - lnin * P / PSK_M;

    /* Make room for new data, slide left 16 samples */

    for (int i = 0, j = P - adjust; i < (NT - 1) * P + adjust; i++, j++) {
        rxFilterMemTiming[i] = rxFilterMemTiming[j];
    }

    for (int i = (NT - 1) * P + adjust, j = 0; i < (NT * P); i++, j++) {
        rxFilterMemTiming[i] = rxFiltered[j];
    }

    for (int i = 0; i < (NT * P); i++) {
        env[i] = cabsf(rxFilterMemTiming[i]);
    }

    complex float x = 0.0f;
    complex float phase = cmplx(0.0f);
    complex float freq = cmplx(TAU / P);

    for (int i = 0; i < (NT * P); i++) {
        x += (phase * env[i]);
        phase *= freq;
    }

    float adjustedRxTiming = cargf(x) / TAU;
    float rx_timing = adjustedRxTiming * P + 1;

    if (rx_timing > (float) P) {
        rx_timing -= P;
    } else if (rx_timing < (float) -P) {
        rx_timing += P;
    }

    rx_timing += floorf(NT / 2.0f) * P;
    int low_sample = floorf(rx_timing);

    float fract = rx_timing - low_sample;
    int high_sample = ceilf(rx_timing);

    complex float t1 = rxFilterMemTiming[low_sample - 1] * (1.0f - fract);
    complex float t2 = rxFilterMemTiming[high_sample - 1] * fract;
    symbol[0] = t1 + t2;

    return adjustedRxTiming * PSK_M;
}

static void linearRegression(complex float *slope, complex float *intercept,
        float x[], complex float y[]) {
    complex float sumxy = 0.0f;
    complex float sumy = 0.0f;
    float sumx = 0.0f;
    float sumx2 = 0.0f;

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        sumx += x[i]; // x is a fixed set of values
        sumx2 += (x[i] * x[i]);
        sumxy += (y[i] * x[i]);
        sumy += y[i];
    }

    float den = (PSK_SYMBOLS * sumx2 - sumx * sumx);

    /*
     * fits y = mx + b to the (x,y) data
     * x is the sampling points
     */

    if (den != 0.0f) {
        *slope = ((sumxy * PSK_SYMBOLS) - (sumy * sumx)) / den;
        *intercept = ((sumy * sumx2) - (sumxy * sumx)) / den;
    } else {
        *slope = 0.0f;
        *intercept = 0.0f;
    }
}
