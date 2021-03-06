/*
 *  spectron_cct.cpp - Implementation colour functions operating on spectral
 *                     measurements from Hamamatsu sensors.
 *
 *  Copyright 2019 Alexey Danilchenko, Bruce Lindbloom
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version with ADDITION (see below).
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, 51 Franklin Street - Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */


#include "spectron_cct.h"

#include <math.h>
#include <float.h>

// ---------------------------------------------------------------------------
// CCT implementation is taken and adapted for this code from Bruce Lindbloom
// site:
//     http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_T.html
// ---------------------------------------------------------------------------

// LERP(a,b,c) = linear interpolation macro, is 'a' when c == 0.0 and 'b' when c == 1.0
#define LERP(a,b,c)     (((b) - (a)) * (c) + (a))
struct UVT {
        double  u;
        double  v;
        double  t;
};
double rt[31] = {       /* reciprocal temperature (K) */
         DBL_MIN,  10.0e-6,  20.0e-6,  30.0e-6,  40.0e-6,  50.0e-6,
         60.0e-6,  70.0e-6,  80.0e-6,  90.0e-6, 100.0e-6, 125.0e-6,
        150.0e-6, 175.0e-6, 200.0e-6, 225.0e-6, 250.0e-6, 275.0e-6,
        300.0e-6, 325.0e-6, 350.0e-6, 375.0e-6, 400.0e-6, 425.0e-6,
        450.0e-6, 475.0e-6, 500.0e-6, 525.0e-6, 550.0e-6, 575.0e-6,
        600.0e-6
};
UVT uvt[31] = {
        {0.18006, 0.26352, -0.24341},
        {0.18066, 0.26589, -0.25479},
        {0.18133, 0.26846, -0.26876},
        {0.18208, 0.27119, -0.28539},
        {0.18293, 0.27407, -0.30470},
        {0.18388, 0.27709, -0.32675},
        {0.18494, 0.28021, -0.35156},
        {0.18611, 0.28342, -0.37915},
        {0.18740, 0.28668, -0.40955},
        {0.18880, 0.28997, -0.44278},
        {0.19032, 0.29326, -0.47888},
        {0.19462, 0.30141, -0.58204},
        {0.19962, 0.30921, -0.70471},
        {0.20525, 0.31647, -0.84901},
        {0.21142, 0.32312, -1.0182},
        {0.21807, 0.32909, -1.2168},
        {0.22511, 0.33439, -1.4512},
        {0.23247, 0.33904, -1.7298},
        {0.24010, 0.34308, -2.0637},
        {0.24792, 0.34655, -2.4681},  /* Note: 0.24792 is a corrected value for the error found in W&S as 0.24702 */
        {0.25591, 0.34951, -2.9641},
        {0.26400, 0.35200, -3.5814},
        {0.27218, 0.35407, -4.3633},
        {0.28039, 0.35577, -5.3762},
        {0.28863, 0.35714, -6.7262},
        {0.29685, 0.35823, -8.5955},
        {0.30505, 0.35907, -11.324},
        {0.31320, 0.35968, -15.628},
        {0.32129, 0.36011, -23.325},
        {0.32931, 0.36038, -40.770},
        {0.33724, 0.36051, -116.45}
};
double XYZtoCorColorTemp(double *xyz)
{
    double us, vs, p, di, dm;
    int i;
    double temp = 0;
    if ((xyz[0] < 1.0e-20) && (xyz[1] < 1.0e-20) && (xyz[2] < 1.0e-20))
        // protect against possible divide-by-zero failure
        return 0;
    us = (4.0 * xyz[0]) / (xyz[0] + 15.0 * xyz[1] + 3.0 * xyz[2]);
    vs = (6.0 * xyz[1]) / (xyz[0] + 15.0 * xyz[1] + 3.0 * xyz[2]);
    dm = 0.0;
    for (i = 0; i < 31; i++)
    {
        di = (vs - uvt[i].v) - uvt[i].t * (us - uvt[i].u);
        if ((i > 0) && (((di < 0.0) && (dm >= 0.0)) || ((di >= 0.0) && (dm < 0.0))))
            break;  // found lines bounding (us, vs) : i-1 and i
        dm = di;
    }
    if (i == 31)
        // bad XYZ input, color temp would be less than minimum of 1666.7 degrees, or too far towards blue
        return 0;
    di = di / sqrt(1.0 + uvt[i    ].t * uvt[i    ].t);
    dm = dm / sqrt(1.0 + uvt[i - 1].t * uvt[i - 1].t);
    // p = interpolation parameter, 0.0 : i-1, 1.0 : i
    p = dm / (dm - di);
    p = 1.0 / (LERP(rt[i - 1], rt[i], p));
    return p;
}


// -------------------------------------------------------------------------
//  Below functions modelling standard 1931 observer are implemented from
//  the C.Wyman, P.Sloan, P.Shirley "Simple Analytic Approximations to the
//  CIE XYZ Color Matching Functions" paper
// -------------------------------------------------------------------------

double xFunc_1931(double wavelength)
{
    double t1 = (wavelength-442.0)*((wavelength<442.0) ? 0.0624 : 0.0374);
    double t2 = (wavelength-599.8)*((wavelength<599.8) ? 0.0264 : 0.0323);
    double t3 = (wavelength-501.1)*((wavelength<501.1) ? 0.0490 : 0.0382);

    return 0.362*exp(-0.5*t1*t1)+1.056*exp(-0.5*t2*t2)-0.065*exp(-0.5*t3*t3);
}

double yFunc_1931(double wavelength)
{
    double t1 = (wavelength-568.8)*((wavelength<568.8) ? 0.0213 : 0.0247);
    double t2 = (wavelength-530.9)*((wavelength<530.9) ? 0.0613 : 0.0322);

    return 0.821*exp(-0.5*t1*t1)+0.286*exp(-0.5*t2*t2);
}

double zFunc_1931(double wavelength)
{
    double t1 = (wavelength-437.0)*((wavelength<437.0) ? 0.0845 : 0.0278);
    double t2 = (wavelength-459.0)*((wavelength<459.0) ? 0.0385 : 0.0725);

    return 1.217*exp(-0.5*t1*t1)+0.681*exp(-0.5*t2*t2);
}

// -----------------------------------------------------------
//  Processing Spectron spectra and calculating CCT, x and y
// -----------------------------------------------------------
void calculateColourParam(SpectronDevice& spectron, double &CCT, double &x, double &y)
{
    if (!spectron.isConnected())
        return;

    double xyz[3] = { 0, 0, 0 };
    for (int i=0; i<spectron.totalPixels(); i++)
    {
        double dLamda=0;
        if (i==0)
            dLamda = (spectron.getWavelength(i+1)-spectron.getWavelength(i))/2;
        else if (i==spectron.totalPixels()-1)
            dLamda = (spectron.getWavelength(i)-spectron.getWavelength(i-1))/2;
        else
            dLamda = (spectron.getWavelength(i+1)-spectron.getWavelength(i-1))/2;
        double curWavelength = spectron.getWavelength(i);
        xyz[0] += spectron.getLastMeasurement(i)*xFunc_1931(curWavelength)*dLamda;
        xyz[1] += spectron.getLastMeasurement(i)*yFunc_1931(curWavelength)*dLamda;
        xyz[2] += spectron.getLastMeasurement(i)*zFunc_1931(curWavelength)*dLamda;
    }

    double sumXYZ = xyz[0]+xyz[1]+xyz[2];
    
    if (sumXYZ)
    {
        x=xyz[0]/sumXYZ;
        y=xyz[1]/sumXYZ;
    }
    else
        x = y = 0;

    CCT = XYZtoCorColorTemp(xyz);
}
