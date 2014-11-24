/* Multicomponent data registration analysis. */
/*
  Copyright (C) 2004 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <rsf.h>


static float *coord, ***out, *num, *den, g0, dg, o1, d1, o2, d2, dout, doth;
static int n2g, ntr, n1, n2, ng, order;
static sf_bands spl;

void warpscan_init(int m1     /* input trace length */, 
		           float o11  /* input origin */,
		           float d11  /* float increment */,
		           int m2     /* output trace length */,
		           float o21  /* output origin */,
		           float d21  /* output increment */,
		           int ng1    /* number of scanned "gamma" values */,
		           float g01  /* first gamma */,
		           float dg1  /* gamma increment */,
		           int ntr1   /* number of traces */, 
		           int order1 /* interpolation accuracy */, 
		           int dim    /* dimensionality */, 
		           int *m     /* data dimensions [dim] */, 
		           int *rect  /* smoothing radius [dim] */, 
		           int niter  /* number of iterations */,
		           bool verb  /* verbosity */)
/*< initialize >*/
{
    n1 = m1;
    o1 = o11;
    d1 = d11;
    o2 = o21;
    d2 = d21;
    n2 = m2;
    ntr = ntr1;
    ng = ng1;
    g0 = g01;
    dg = dg1;
    n2g = n2*ng*ntr;
    order = order1;

    coord = sf_floatalloc (n2); 
    out =   sf_floatalloc3 (n2,ng,ntr);

    num = sf_floatalloc (n2g);
    den = sf_floatalloc (n2g);

    spl = sf_spline_init (order, n1);     
    sf_divn_init(dim, n2g, m, rect, niter, verb);
}

void interpolate_inp(float **inp, 
                     float **oth)
/*< interpolate input data >*/
{
    float g; 
    int i1,i2, ig;

    doth = 0.;
    dout = 0.;
    for (i2=0; i2 < ntr; i2++) {
	    sf_banded_solve (spl, inp[i2]);

	    for (i1=0; i1 < n2; i1++) {
	        doth += oth[i2][i1]*oth[i2][i1];
	    }
	
	    for (ig=0; ig < ng; ig++) {
	        g = g0 + ig*dg;

	        for (i1=0; i1 < n2; i1++) {
		        coord[i1] = (o2+i1*d2)*g;
	        }

	        sf_int1_init (coord, o1, d1, n1, sf_spline_int, order, n2);

	        sf_int1_lop (false,false,n1,n2,inp[i2],out[i2][ig]);

	        for (i1=0; i1 < n2; i1++) {
		        dout += out[i2][ig][i1]*out[i2][ig][i1];
	        }
	    }
    }
    doth = sqrtf(ntr*n2/doth);
    dout = sqrtf(n2g/dout);
}

void warpscan_partone(float** oth /* target data [ntr][n2] */,
	                  float* rat1)
/*< scan >*/
{
    int i1, i2, ig, i;

    for (i2=0; i2 < ntr; i2++) {
	for (ig=0; ig < ng; ig++) {
	    for (i1=0; i1 < n2; i1++) {
		i = (i2*ng + ig)*n2+i1;
		num[i] = oth[i2][i1]*dout;
		den[i] = out[i2][ig][i1]*dout;
	    }
	}
    }
    sf_divn(num,den,rat1);
}

void warpscan_partone_deriv(float* rat1_deriv)
/*< scan >*/
{
    int i1, i2, ig, i;

    for (i2=0; i2 < ntr; i2++) {
	for (ig=0; ig < ng; ig++) {
	    for (i1=0; i1 < n2; i1++) {
		i = (i2*ng + ig)*n2+i1;
		num[i] = 1.0*dout;
		den[i] = out[i2][ig][i1]*dout;
	    }
	}
    }
    sf_divn(num,den,rat1_deriv);
}

void warpscan_parttwo(float** oth /* target data [ntr][n2] */,
	                  float* rat2)
/*< scan >*/
{
    int i1, i2, ig, i;

    for (i2=0; i2 < ntr; i2++) {
	for (ig=0; ig < ng; ig++) {
	    for (i1=0; i1 < n2; i1++) {
		i = (i2*ng+ig)*n2+i1;
		num[i] = out[i2][ig][i1]*doth;
		den[i] = oth[i2][i1]*doth;
	    }
	}
    }
    sf_divn(num,den,rat2);
}

void warpscan_parttwo_deriv(float** oth /* target data [ntr][n2] */,
                            float*  rat2,
	                        float*  rat2_deriv)
/*< scan >*/
{
    int i1, i2, ig, i;

    for (i2=0; i2 < ntr; i2++) {
	for (ig=0; ig < ng; ig++) {
	    for (i1=0; i1 < n2; i1++) {
		i = (i2*ng+ig)*n2+i1;
		num[i] = -2.0*rat2[i];
        if ( fabsf(oth[i2][i1]) > 1.e-8 ) { 
            num[i] += out[i2][ig][i1]/oth[i2][i1];
        }
        num[i] = num[i] * doth; 
		den[i] = oth[i2][i1]*doth; 
	    }
	}
    }
    sf_divn(num,den,rat2_deriv);
}

void warpscan_combine(const float *one,
                      const float *two,
                      float *prod)
/*< combine >*/
{
    int i;
    float p;
    for ( i = 0 ; i < n2g; i++ ) { 
	    p=one[i]*two[i];
        prod[i]=p;
    }
}

void warpscan_combine_square(const float *one,
                             const float *two,
                             float *prod)
/*< combine >*/
{
    int i;
    float p;
    for ( i = 0 ; i < n2g; i++ ) { 
	    p=one[i]*two[i];
        prod[i]=p*p;
    }
}

/* 	$Id: Mwarpscan.c 744 2004-08-17 18:46:07Z fomels $	 */