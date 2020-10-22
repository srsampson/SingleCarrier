/*
  Copyright (C) 2018 James C. Ahlstrom

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

/*
 * Created with:
 * hs = gen_rn_coeffs(.31, 1.0/8000.0, 1600, 10, 5);
 */
float alpha31_root[] = {
    -0.00140721f,
    -0.00258347f,
    -0.00211782f,
    -0.00010823f,
    0.00224934f,
    0.00326078f,
    0.00179380f,
    -0.00179940f,
    -0.00552621f,
    -0.00663829f,
    -0.00324267f,
    0.00418549f,
    0.01233564f,
    0.01618214f,
    0.01144857f,
    -0.00262892f,
    -0.02165701f,
    -0.03666715f,
    -0.03709688f,
    -0.01518676f,
    0.03002121f,
    0.09095027f,
    0.15306638f,
    0.19943502f,
    // cfTune D = (filter->nTaps - 1.0) / 2.0;
    0.21659606f,    //  48 / 2 = 24
    //
    0.19943502f,
    0.15306638f,
    0.09095027f,
    0.03002121f,
    -0.01518676f,
    -0.03709688f,
    -0.03666715f,
    -0.02165701f,
    -0.00262892f,
    0.01144857f,
    0.01618214f,
    0.01233564f,
    0.00418549f,
    -0.00324267f,
    -0.00663829f,
    -0.00552621f,
    -0.00179940f,
    0.00179380f,
    0.00326078f,
    0.00224934f,
    -0.00010823f,
    -0.00211782f,
    -0.00258347f,
    -0.00140721f
};