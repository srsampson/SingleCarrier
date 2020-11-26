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
#include <complex.h>
#include <stdint.h>

#include "psk_internal.h"

// Externals

extern struct PSK *psk;
extern const int samplingPoints[];
extern const float alpha50_root[];

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

// Functions

/*
 * Given a signal of complex samples,
 * return data bits packed into 8 bytes
 * and a sync indication
 */
int receive(uint8_t packed_codec_bits[], complex float signal[]) {
    int bitPairs[PSK_BITS_PER_FRAME];
    int nin = PSK_SYMBOLS_PER_FRAME;
    int sync;

    /*
     * Drop the user-level amplitude
     */
    for (int i = 0; i < nin; i++) {
        signal[i] /= MODEM_SCALE;
    }

    demodulate(bitPairs, &sync, signal, &nin);

    if (sync == 1) {
        for (int j = 0, k = 0; j < PSK_BITS_PER_FRAME; j += (PSK_BITS_PER_FRAME / 2), k += 4) { /* 0, 28 */
            int bit = 7;
            int nbyte = 0;

            for (int i = 0; i < 4; i++) {
                packed_codec_bits[k + i] = 0;
            }

            for (int i = 0; i < (PSK_BITS_PER_FRAME / 2); i++) { /* 0..28 */
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
    complex float ch_symb[NSW * PILOTDATA_SYMBOLS];
    int anextSync;

    int lsync = psk->m_sync; /* get a starting value */
    int nextSync = lsync;

    /*
     * Add the new signal samples to the Sync Window Buffer
     * which is 2400 complex time samples long.
     *
     * First move the old samples to the left.
     */
    for (int i = 0; i < (NSW * PILOTDATA_SYMBOLS * PSK_M - *nin_frame); i++) {
        psk->m_chFrameBuf[i] = psk->m_chFrameBuf[i + *nin_frame];
    }

    /*
     * Now add in the new samples.
     */
    for (int i = 0; i < (NSW * PILOTDATA_SYMBOLS * PSK_M); i++) {
        psk->m_chFrameBuf[i] = signal[i];
    }

    if (lsync == 0) {
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
            receiveProcessor(ch_symb, psk->m_chFrameBuf, (NSW * PILOTDATA_SYMBOLS), PSK_M, 0);

            for (int j = 0; j < NSW - 1; j++) {
                updateCtSymbolBuffer(ch_symb, (PILOTDATA_SYMBOLS * j));
            }

            frameSyncFineFreqEstimate(ch_symb, ((NSW - 1) * PILOTDATA_SYMBOLS), lsync, &anextSync);

            if (anextSync == 1) {
                if (psk->m_ratio > maxRatio) {
                    maxRatio = psk->m_ratio;
                    freqEstimate = (psk->m_freqEstimate - psk->m_freqFineEstimate);
                    nextSync = anextSync;
                }
            }
        }

        if (nextSync == 1) {
            psk->m_freqEstimate = freqEstimate;
            receiveProcessor(ch_symb, psk->m_chFrameBuf, (NSW * PILOTDATA_SYMBOLS), PSK_M, 0);

            for (int i = 0; i < NSW - 1; i++) {
                updateCtSymbolBuffer(ch_symb, (i * PILOTDATA_SYMBOLS));
            }

            frameSyncFineFreqEstimate(ch_symb, ((NSW - 1) * PILOTDATA_SYMBOLS), lsync, &nextSync);

            if (fabsf(psk->m_freqFineEstimate) > 2.0f) {
                nextSync = 0;
            }
        }

        if (nextSync == 1) {
            /*
             * We are in sync finally
             */
            for (int r = 0; r < NCT_SYMB_BUF; r++) {
                psk->m_ctFrameBuf[r] = psk->m_ctSymbBuf[psk->m_sampleCenter + r];
            }
        }
    } else if (lsync == 1) {
        /*
         * Good deal, we were already in sync
         * so we can skip searching for the center frequency
         * Much less CPU now.
         */
        receiveProcessor(ch_symb, signal, PILOTDATA_SYMBOLS, psk->m_nin, 1);
        frameSyncFineFreqEstimate(ch_symb, 0, lsync, &nextSync);

        for (int r = 0; r < 2; r++) {
            psk->m_ctFrameBuf[r] = psk->m_ctFrameBuf[PILOTDATA_SYMBOLS + r];
        }

        for (int r = 2; r < NCT_SYMB_BUF; r++) {
            psk->m_ctFrameBuf[r] = psk->m_ctSymbBuf[psk->m_sampleCenter + r];
        }
    }

    *sync_good = 0;

    if ((nextSync == 1) || (lsync == 1)) {
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

    if (nextSync == 1) {
        if (psk->m_rxTiming > PSK_M / P) {
            lnin = PSK_M + PSK_M / P;
        } else if (psk->m_rxTiming < -PSK_M / P) {
            lnin = PSK_M - PSK_M / P;
        }
    }

    psk->m_nin = lnin;

    *nin_frame = (PILOTDATA_SYMBOLS - 1) * PSK_M + lnin;
}

/*
 * There are two more pilot symbols than data
 */
static void constellationToBits(int bitPairs[]) {
    complex float y[MOD_SYMBOLS];
    complex float rxSymbolLinear[MOD_SYMBOLS];
    complex float slope;
    complex float intercept;
    float x[MOD_SYMBOLS];

    for (int i = 0; i < MOD_SYMBOLS; i++) {
        x[i] = samplingPoints[i]; // 0, 1, 31, 32
        y[i] = psk->m_ctFrameBuf[samplingPoints[i]] * psk->m_pilots[i];
    }

    linearRegression(&slope, &intercept, x, y);

    for (int i = 0; i < MOD_SYMBOLS; i++) { /* 4 */
        psk->m_pskPhase[i] = cargf((slope * (MOD_SYMBOLS + i)) + intercept);
    }

    /* Adjust the phase of symbols */

    for (int i = 0; i < MOD_SYMBOLS; i++) {
        complex float phi_rect = conjf(cmplx(psk->m_pskPhase[i]));

        psk->m_rxSymb[i] = psk->m_ctFrameBuf[MOD_SYMBOLS + i] * phi_rect;
        rxSymbolLinear[MOD_SYMBOLS + i] = psk->m_rxSymb[i];
    }

    /*
     * load the bits detected from the received symbols
     * these are the bit-pairs in the modem frame
     */

    for (int i = 0; i < MOD_SYMBOLS; i++) {
        complex float rotate = psk->m_rxSymb[i] * cmplx(ROT45);

        bitPairs[2 * i + 1] = crealf(rotate) < 0.0f;
        bitPairs[2 * i] = cimagf(rotate) < 0.0f;
    }

    float mag = 0.0f;

    for (int i = 0; i < MOD_SYMBOLS; i++) {
        mag += cabsf(rxSymbolLinear[i]);
    }

    psk->m_signalRMS = mag / MOD_SYMBOLS;

    float sum_x = 0.0f;
    float sum_xx = 0.0f;
    int n = 0;

    for (int i = 0; i < MOD_SYMBOLS; i++) {
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

static void downconvert(complex float baseband[], complex float offsetSignal[],
        int lnin) {
    for (int i = 0; i < lnin; i++) {
        psk->m_phaseRx *= psk->m_carrier;
        baseband[i] = offsetSignal[i] * conjf(psk->m_phaseRx);
    }

    /* Normalize */

    psk->m_phaseRx /= cabsf(psk->m_phaseRx);
}

static void receiveFilter(complex float filtered[], complex float baseband[],
        int lnin) {
    int n = PSK_M / P;

    for (int i = 0, j = 0; i < lnin; i += n, j++) {
        /*
         * Move the new samples in from the right.
         */
        for (int k = PSK_NFILTER - n, l = i; k < PSK_NFILTER; k++, l++) {
            psk->m_rxFilterMemory[k] = baseband[l];
        }

        filtered[j] = 0.0f;

        for (int k = 0; k < PSK_NFILTER; k++) {
            filtered[j] += (psk->m_rxFilterMemory[k] * gtAlpha5Root[k]);
        }

        for (int k = 0, l = n; k < (PSK_NFILTER - n); k++, l++) {
            psk->m_rxFilterMemory[k] = psk->m_rxFilterMemory[l];
        }
    }
}

/*
 * Shift the center frequency
 */
static void frequencyShift(complex float waveform[], complex float signal[], int lnin) {
    complex float rxPhase = cmplx(TAU * -psk->m_freqEstimate / PSK_FS);

    for (int i = 0; i < lnin; i++) {
        psk->m_phaseRx *= rxPhase;
        waveform[i] = signal[i] * psk->m_phaseRx;
    }

    psk->m_phaseRx /= cabsf(psk->m_phaseRx);
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

        if (freqTrack == 1) {
            modStrip = 0.0f;

            adiff = rxOneFrame[0] * conjf(psk->m_prevRxSymbols);
            psk->m_prevRxSymbols = rxOneFrame[0];

            adiff = cpowf(adiff, 4.0f);
            modStrip += cabsf(adiff);

            psk->m_freqOffsetFiltered = (1.0f - 0.005f) * psk->m_freqOffsetFiltered +
                    0.005f * cargf(modStrip);

            psk->m_freqEstimate += (0.2f * psk->m_freqOffsetFiltered);
        }

        if (lnin != PSK_M) {
            lnin = PSK_M;
        }
    }

    psk->m_rxTiming = adjustedRxTiming;
}

static void pilotCorrelation(float *corr_out, float *mag_out, int t, float f_fine) {
    complex float acorr = 0.0f;
    float mag = 0.0f;

    for (int i = 0; i < MOD_SYMBOLS; i++) {
        complex float freqFinePhase = cmplx(TAU * f_fine * (samplingPoints[i] + 1.0f) / PSK_RS);
        complex float freqCorr = psk->m_ctSymbBuf[t + samplingPoints[i]] * freqFinePhase;

        acorr += (freqCorr * psk->m_pilots[i]);
        mag += cabsf(freqCorr);
    }

    *corr_out = cabsf(acorr);
    *mag_out = mag;
}

static void frameSyncFineFreqEstimate(complex float ch_symb[], int offset,
        int sync, int *nextSync) {

    updateCtSymbolBuffer(ch_symb, offset);

    if (sync == 0) {
        float corr;
        float mag;
        float max_corr = 0.0f;
        float max_mag = 0.0f;

        for (int j = -2000; j <= 2000; j += 25) {
            float f_fine = (float) j / 100.0f;

            for (int i = 0; i < PILOTDATA_SYMBOLS; i++) {
                pilotCorrelation(&corr, &mag, i, f_fine);

                if (corr >= max_corr) {
                    max_corr = corr;
                    max_mag = mag;
                    psk->m_sampleCenter = i;
                    psk->m_freqFineEstimate = f_fine;
                }
            }
        }

        if (max_corr / max_mag > 0.9f) {
            psk->m_syncTimer = 0;
            *nextSync = 1;
        } else {
            *nextSync = 0;
        }

        psk->m_ratio = max_corr / max_mag;
    }
}

static void updateCtSymbolBuffer(complex float symbol[], int offset) {
    for (int i = 0; i < (NCT_SYMB_BUF - PILOTDATA_SYMBOLS); i++) {
        psk->m_ctSymbBuf[i] = psk->m_ctSymbBuf[PILOTDATA_SYMBOLS + i];
    }

    for (int i = NCT_SYMB_BUF - PILOTDATA_SYMBOLS, j = 0; i < NCT_SYMB_BUF; i++, j++) {
        psk->m_ctSymbBuf[i] = symbol[offset + j];
    }
}

static void syncStateMachine(int sync, int *nextSync) {
    if (sync == 1) {
        float corr;
        float mag;

        pilotCorrelation(&corr, &mag, psk->m_sampleCenter, psk->m_freqFineEstimate);

        psk->m_ratio = fabsf(corr) / mag;

        if (psk->m_ratio < 0.8f) {
            psk->m_syncTimer++;
        } else {
            psk->m_syncTimer = 0;
        }

        if (psk->m_syncTimer == 10) {
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
        psk->m_rxFilterMemTiming[i] = psk->m_rxFilterMemTiming[j];
    }

    for (int i = (NT - 1) * P + adjust, j = 0; i < (NT * P); i++, j++) {
        psk->m_rxFilterMemTiming[i] = rxFiltered[j];
    }

    for (int i = 0; i < (NT * P); i++) {
        env[i] = cabsf(psk->m_rxFilterMemTiming[i]);
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

    complex float t1 = psk->m_rxFilterMemTiming[low_sample - 1] * (1.0f - fract);
    complex float t2 = psk->m_rxFilterMemTiming[high_sample - 1] * fract;
    symbol[0] = t1 + t2;

    return adjustedRxTiming * PSK_M;
}

/*
 * For testing this algorithm you can use these values:
 *
 * x = [1 2 7 8] start slope at 1,2 end at 7,8
 *               solve for [3 4 5 6]
 *
 * y = [
 *      1.0f * (-0.70702 + 0.70708 * I)
 *     -1.0f * ( 0.77314 - 0.63442 * I)
 *      1.0f * (-0.98083 + 0.19511 * I)
 *     -1.0f * ( 0.99508 - 0.09799 * I)
 *     ]
 *
 * Where [1 -1  1 -1] simulates pilot symbol phases
 *
 * yfit = (slope * x) + intercept
 * where x = 1..8
 *
 * Answers printed should be:
 *
 * -0.71953 + 0.71420i
 * -0.76081 + 0.62690i
 * -0.80209 + 0.53960i
 * -0.84338 + 0.45230i
 * -0.88466 + 0.36500i
 * -0.92594 + 0.27770i
 * -0.96722 + 0.19040i
 * -1.00850 + 0.10310i
 */
static void linearRegression(complex float *slope, complex float *intercept,
        float x[], complex float y[]) {
    complex float sumxy = 0.0f;
    complex float sumy = 0.0f;
    float sumx = 0.0f;
    float sumx2 = 0.0f;

    for (int i = 0; i < MOD_SYMBOLS; i++) {
        sumx += x[i]; // x is a fixed set of values
        sumx2 += (x[i] * x[i]);
        sumxy += (y[i] * x[i]);
        sumy += y[i];
    }

    float den = (MOD_SYMBOLS * sumx2 - sumx * sumx);

    /*
     * fits y = mx + b to the (x,y) data
     * x is the sampling points
     */

    if (den != 0.0f) {
        *slope = ((sumxy * MOD_SYMBOLS) - (sumy * sumx)) / den;
        *intercept = ((sumy * sumx2) - (sumxy * sumx)) / den;
    } else { // no solution never happen with the data used in this modem
        *slope = 0.0f;
        *intercept = 0.0f;
    }
}
