/*
 * This source code is a product of Sun Microsystems, Inc. and is provided
 * for unrestricted use.  Users may copy or modify this source code without
 * charge.
 *
 * SUN SOURCE CODE IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING
 * THE WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun source code is provided with no support and without any obligation on
 * the part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY THIS SOFTWARE
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * g72x.h
 *
 * Header file for CCITT conversion routines.
 *
 */
#ifndef _G72X_H
#define _G72X_H

/*
 * The following is the definition of the state structure
 * used by the G.721/G.723 encoder and decoder to preserve their internal
 * state between successive calls.  The meanings of the majority
 * of the state structure fields are explained in detail in the
 * CCITT Recommendation G.721.  The field names are essentially identical
 * to variable names in the bit level description of the coding algorithm
 * included in this Recommendation.
 */
struct g72x_state {
    long yl;   /* Locked or steady state step size multiplier. */
    short yu;  /* Unlocked or non-steady state step size multiplier. */
    short dms; /* Short term energy estimate. */
    short dml; /* Long term energy estimate. */
    short ap;  /* Linear weighting coefficient of 'yl' and 'yu'. */

    short a[2];  /* Coefficients of pole portion of prediction filter. */
    short b[6];  /* Coefficients of zero portion of prediction filter. */
    short pk[2]; /*
                  * Signs of previous two samples of a partially
                  * reconstructed signal.
                  */
    short dq[6]; /*
                  * Previous 6 samples of the quantized difference
                  * signal represented in an internal floating point
                  * format.
                  */
    short sr[2]; /*
                  * Previous 2 samples of the quantized difference
                  * signal represented in an internal floating point
                  * format.
                  */
    char td;     /* delayed tone detect, new in 1988 version */
};

/* External function definitions. */

void g72x_init_state(struct g72x_state*);
int g721_encoder(int sample, struct g72x_state* state_ptr);
int g721_decoder(int code, struct g72x_state* state_ptr);


int quantize(int d, int y, short* table, int size);
int reconstruct(int, int, int);
void

    update(int code_size,
                  int y,
                  int wi,
                  int fi,
                  int dq,
                  int sr,
                  int dqsez,
                  struct g72x_state* state_ptr);

int predictor_zero(struct g72x_state* state_ptr);

int predictor_pole(struct g72x_state* state_ptr);
int step_size(struct g72x_state* state_ptr);
#endif /* !_G72X_H */

