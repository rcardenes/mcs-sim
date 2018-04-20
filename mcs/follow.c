#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "follow.h"

#define TIME_INT        0.005  /* 5 msec                             */
#define TRIGGER_LATENCY 0.1    /* Seconds before Bancomm trigger     */
#define JUMP            0.1    /* Degrees change considered a slew   */
#define AZ_JUMP         0.1    /* Degrees change considered a slew   */
#define EL_JUMP         0.1    /* Degrees change considered a slew   */
#define NUM_EXTRAP	20	/* number of points to extrapolate    */
#define	DOUBLE_BUFF	(NUM_EXTRAP>10)

#define MIN(x,y)	((x<y)?x:y)
#define MIN3(a,b,c)	((a<b) ? ((a<c) ? a : c) : ((b<c) ? b : c))



/* fillBuffer - Extrapolate demands
 */
long mcs_sim_fillBuffer (double *AA,  double *BB,  double *CC, 
                 double *pos, double *vel, double offset, 
                 long axis,   double *lastPMACDemand, double jump,
                 double maxVel, double maxAcc, double currentPos,
                 double currentVel, long trajectoryMode, int recent,
		 mcs_parameters *internal_params)
{
    int    i, imax;
    long   error;
    /*long   adjustment = 0; */
    double lastPos = 0, lastVel = 0, acceleration = 0;
    double A;
    double B;
    double C;
    double posA = 0;
    double posB = 0;
    double posC = 0;
    double timeA = 0;
    double timeB = 0;
    double timeC = 0;
    double tt;


    /* If the times in the three demands coming from the TCS are all zero
     * then the TCS has not connected yet.
     */
    if ((AA[0] == 0.0) && (BB[0] == 0.0) && (CC[0] == 0.0))
    {
	printf ("TCS has not connected!\n");
	return (1);
    }

    /* Fit a parabolla line to the last two demands.
     */
    error = calc_quadratic(jump, AA[0], AA[1], BB[0], BB[1], 
	                   CC[0], CC[1], &C, &B, &A);

    /* Use the previous coefficients if the fit fails.
     */
    if (error)
    {
	printf ("Time's Equal!!\n");
	if (axis == 1)
	{
	    A = internal_params->azA;
	    B = internal_params->azB;
	    C = internal_params->azC;
	}
	else
	{
	    A = internal_params->elA;
	    B = internal_params->elB;
	    C = internal_params->elC;
	}
    }

    /* Extrapolate data. Data points are extrapolated from the starting
     * time offset + TIME_INT (0.005) to time offset + NUM_EXTRAP * TIME_INT.
     */
    imax = NUM_EXTRAP + 1;
    for (i=1; i<imax; i++)
    {
        tt       = offset + i*TIME_INT;
	pos[i-1] = (A*tt + B)*tt + C;
	vel[i-1] = 2.0*A*tt + B;
    }

    /* Save coefficients for next call in case the fit fails.
     */
    if (axis == 1)
    {
	internal_params->azA = A;
	internal_params->azB = B;
	internal_params->azC = C;
    }
    else
    {
	internal_params->elA = A;
	internal_params->elB = B;
	internal_params->elC = C;
    }

    /* Put the last PMAC position demand in a separate parameter.
     */
    *lastPMACDemand = pos[NUM_EXTRAP-1];
    if (axis == 1)
	internal_params->lastAzVelocity = vel[NUM_EXTRAP-1];
    else
        internal_params->lastElVelocity = vel[NUM_EXTRAP-1];

    return (0);
}


/* calc_coeffs - Not used anymore.
 */
long mcs_sim_calc_coeffs (double *aa, double *bb, double *cc, double *A,
		  double *B, double *C)
{
    double denom;
    long   ret;

    denom = (aa[0]-bb[0]) * (aa[0]-cc[0]) * (bb[0]-cc[0]);
    if (denom == 0)
        ret = 1;
    else
    {
	*A  = ((aa[1]-bb[1])*(aa[0]-cc[0]) - (aa[1]-cc[1])*(aa[0]-bb[0])) /
	denom;
	*B  = (aa[1]-cc[1])/(aa[0]-cc[0]) - (*A)*(aa[0]+cc[0]);
	*C  = aa[1] - (*A)*aa[0]*aa[0] - (*B)*aa[0];
	ret = 0;

    }
    return (ret);
}

int mcs_sim_calc_linear (double dpmax,
                 double ta, double pa,
                 double tb, double pb,
                 double tc, double pc,
                 double *c0, double *c1, double *c2)
/*
**  - - - - - - - - - - - -
**   c a l c _ l i n e a r
**  - - - - - - - - - - - -
**
**  Fit a parabola to three positions and times, with discontinuity
**  handling.
**
**  !! Fudged to produce a straight line between latest two points. !!
**
**  Given:
**    dpmax     double       maximum acceptable position jump
**    ta        double       time
**    pa        double       position at time ta
**    tb        double       another time
**    pb        double       position at time tb
**    tc        double       another time
**    pc        double       position at time tc
**
**  Returned:
**    c0       double*      polynomial coefficient for 1
**    c1       double*      polynomial coefficient for t
**    c2       double*      polynomial coefficient for t^2
**
**  Status:
**            int       0 = OK
**                     -1 = singular case: no solution
**
**  Notes:
**
**  1)  The argument dpmax specifies the maximum position span
**      that will not result in special "discontinuity" handling.
**      This consists of returning a result that produces a
**      constant position, that for the most recent time.
**
**  2)  The polynomial predicts position, p, for time t:
**
**          p = c0 + c1*t + c2*t*t
**
**  3)  The error status is returned whenever two or more of the
**      three times are identical.
**
**  4)  To minimize rounding errors and avoid overflows, it is best
**      to reckon time with respect to a local zero point close to
**      the present.
**
**  P.T.Wallace   Gemini   11 December 1998
**
**  Copyright 1998 Gemini Project.  All rights reserved.
*/

/* sign(A,B) - magnitude of A with sign of B (double) */
#define sign(A,B) ((B)<0.0?-(A):(A))

{
   double tw, pw, d;
   double span, w;

/* Sort so that most recent two points are (tb,pb) and (tc,pc). */
   if ( ta > tb ) {
      tw = ta;  pw = pa;
      ta = tb;  pa = pb;
      tb = tw;  pb = pw;
   }
   if ( tb > tc ) {
      tw = tb;  pw = pb;
      tb = tc;  pb = pc;
      tc = tw;  pc = pw;
   }
   if ( ta > tb ) {
      tw = ta;  pw = pa;
      ta = tb;  pa = pb;
      tb = tw;  pb = pw;
   }

/* Find the biggest position span. */
   span = fabs ( pa - pb );
   if ( span < ( w = fabs ( pb - pc ) ) ) span = w;
   if ( span < ( w = fabs ( pa - pc ) ) ) span = w;
/* If it's too big, use the most recent position for all three points. */

/* Determinant (must be non-zero). */
   if ( ( d = tc - tb ) == 0.0 ) return -1;

/* Solution. */
   *c0 = ( pb*tc - pc*tb ) / d;
   *c1 = ( pc - pb ) / d;
   *c2 = 0.0;

/* Normal exit. */
   return 0;
}


int mcs_sim_calc_quadratic( double dpmax,
                    double ta, double pa,
                    double tb, double pb,
                    double tc, double pc,
                    double *c0, double *c1, double *c2 )
/*
**  - - - - - - - - - - - - - - -
**   c a l c _ q u a d r a t i c
**  - - - - - - - - - - - - - - -
**
**  Fit a parabola to three positions and times, with discontinuity
**  handling.
**
**  Given:
**    dpmax     double       maximum acceptable position jump
**    ta        double       time
**    pa        double       position at time ta
**    tb        double       another time
**    pb        double       position at time tb
**    tc        double       another time
**    pc        double       position at time tc
**
**  Returned:
**    c0       double*      polynomial coefficient for 1
**    c1       double*      polynomial coefficient for t
**    c2       double*      polynomial coefficient for t^2
**
**  Status:
**            int       0 = OK
**                     -1 = singular case: no solution
**
**  Notes:
**
**  1)  The argument dpmax specifies the maximum position span
**      that will not result in special "discontinuity" handling.
**      This consists of returning a result that produces a
**      constant position, that for the most recent time.
**
**  2)  The polynomial predicts position, p, for time t:
**
**          p = c0 + c1*t + c2*t*t
**
**  3)  The error status is returned whenever two or more of the
**      three times are identical.
**
**  4)  To minimize rounding errors and avoid overflows, it is best
**      to reckon time with respect to a local zero point close to
**      the present.
**
**  P.T.Wallace   Gemini   21 November 1998
**
**  Copyright 1998 Gemini Project.  All rights reserved.
*/

{
   /*double span, w, tmax, ptmax, ab, bc, ac, d; */
   double ab, bc, ac, d; 


/* Find the biggest position span. */
/*   span = fabs ( pa - pb );
   if ( span < ( w = fabs ( pb - pc ) ) ) span = w;
   if ( span < ( w = fabs ( pa - pc ) ) ) span = w;
   */

/* If it's too big, use the most recent position for all three points. */
/*   if ( span > dpmax ) {
      tmax = ta;
      ptmax = pa;
      if ( tmax < tb ) {
         tmax = tb;
         ptmax = pb;
      }
      if ( tmax < tc ) ptmax = pc;
      pa = ptmax;
      pb = ptmax;
      pc = ptmax;
   }
   */

/* Time differences. */
   ab = ta - tb;
   bc = tb - tc;
   ac = ta - tc;

/* Determinant (must be non-zero). */
   if ( ( d = ab * bc * ac ) == 0.0 ) return -1;

/* Solution. */
   *c0 = ( pa*tb*tc*bc - pb*ta*tc*ac + pc*ta*tb*ab ) / d;
   *c1 = ( - pa*bc*(tb+tc) + pb*ac*(ta+tc) - pc*ab*(ta+tb) ) / d;
   *c2 = ( pa*bc - pb*ac + pc*ab ) / d;

/* Normal exit. */
   return 0;
}


int mcs_sim_fit_new_AZ_demand( double timeA, double *posA,
                       double timeB, double *posB,
                       double timeC, double *posC,
                       double maxVel, double maxAcc,
                       double currentPos, 
                       long trajectoryMode,
                       int recent,
		       mcs_parameters *internal_params)
/*
**  - - - - - - - - - - - -
**   fit_new_AZ_demand
**  - - - - - - - - - - - -
**
*/

/* sign(A,B) - magnitude of A with sign of B (double) */
#define sign(A,B) ((B)<0.0?-(A):(A))

{
   double tw, pw, d;
   double vel, velPos, accel;
   double newpos = 0;
   double pa, pb, pc;
   double pospa, pospb, pospc;
   double targetPos, distanceLeft;
   double prevpa, prevpb, prevpc;
   double ta, tb, tc;
   double jump;
   int    flag  = 0;
   int    index = 0;

   pospa = *posA; 
   pospb = *posB; 
   pospc = *posC; 
   
   ta = timeA;
   tb = timeB;
   tc = timeC;

   /* in FIT_NEW */ 
   if (internal_params->firstAzFit)
   {
       internal_params->prevAzDemand[0] = pa = currentPos;
       internal_params->prevAzDemand[1] = pb = currentPos;
       internal_params->prevAzDemand[2] = pc = currentPos;
       internal_params->prevAzVel       = 0.0;
       internal_params->firstAzFit = 0;
       printf("firstAzFit = 0\n");
   } else {
       pa = internal_params->prevAzDemand[0];
       pb = internal_params->prevAzDemand[1];
       pc = internal_params->prevAzDemand[2];
   } 
   

   prevpa = pa;
   prevpb = pb;
   prevpc = pc;
   

   if ( (ta > tb) && (ta > tc) )
      {
         index = 0;
         newpos = pa = *posA;
      }
   else if ( (tb > ta) && (tb > tc) )
      {
         index = 1;
         newpos = pb = *posB;
      }
   else if ( (tc > ta) && (tc > tb) )
      {
         index = 2;
         newpos = pc = *posC;
      } 

/* Sort so that most recent two points are (tb,pb) and (tc,pc). */
   if ( ta > tb ) {
      tw = ta;  pw = pa;
      ta = tb;  pa = pb;
      tb = tw;  pb = pw;
   }
   if ( tb > tc ) {
      tw = tb;  pw = pb;
      tb = tc;  pb = pc;
      tc = tw;  pc = pw;
   }
   if ( ta > tb ) {
      tw = ta;  pw = pa;
      ta = tb;  pa = pb;
      tb = tw;  pb = pw;
   }
 
   jump = fabs(pc - pb);
 
   if ( jump >= 0.1)
      maxAcc = 2.0 * maxAcc;

   /*newpos    = pc; */
   targetPos = pc;

/* Determinant (must be non-zero). */
   if ( ( d = tc - tb ) == 0.0 ) return -1; 
   

/* Solution. */
   vel       = ( pc - pb ) / d; 

/* Apply Velocity Limit */
   if ( fabs(vel) > maxVel)
   {
      vel = sign( maxVel, vel);
      flag = 1;
   }

   accel = (vel - internal_params->prevAzVel)/d;

   /* Apply Acceleration Limit */
   if ( fabs(accel) > maxAcc)
   {
       accel = sign( maxAcc, accel);
       vel   = internal_params->prevAzVel + (double)d * accel; 
       flag = 1;
   }

/* Apply Velocity Limit (yes again!!!)*/
   if ( fabs(vel) > maxVel)
   {
      vel = sign( maxVel, vel);
      accel = (vel - internal_params->prevAzVel)/(double)d;
      flag = 1;
   }

/* override the velocity demand if close to final position */
/* which direction are we trying to head in? */
   if ( vel > 0.0 )
   {
      distanceLeft = fabs(targetPos - pb);

      if ( (targetPos - pb) < 0)
      {
          distanceLeft = 0.0;
      }

      velPos = sqrt((double) 2.0 * maxAcc * distanceLeft);
      if ( (vel > velPos) ) 
      {
          vel = velPos; 
          accel = (vel - internal_params->prevAzVel)/(double)d; 
          flag  = 1; 
      } 
   } 
   else
   { /* same case for negative velocity */

      distanceLeft = fabs(pb - targetPos);
      
      if ( (targetPos - pb) > 0)
      {
          distanceLeft = 0.0;
      }

      velPos = -sqrt((double) 2.0 * maxAcc * distanceLeft);
      if ( (vel < velPos) ) 
      {
          vel = velPos; 
          accel = (vel - internal_params->prevAzVel)/(double)d;
          flag = 1; 
      } 
   } 

/* Adjust new position */
 
   if (flag)
   {
      newpos =  internal_params->prevAzVel * d + (double)0.5*accel*d*d + pb;
   }

/* Check new position */
   if ( (flag) && (((vel > 0) && (newpos > targetPos)) 
               || ((vel < 0) && (newpos < targetPos))) )
       newpos = targetPos;

   switch (index)
      {
      case 0:
          *posA = pa = newpos;
          *posB = pb = prevpb;
          *posC = pc = prevpc;
          break;
      case 1:
          *posA = pa = prevpa;
          *posB = pb = newpos;
          *posC = pc = prevpc;
          break;
      case 2:
          *posA = pa = prevpa;
          *posB = pb = prevpb;
          *posC = pc = newpos;
          break;
      default:
	  printf ("Incorrect value of index - %i", index);
	  break;
      }


   internal_params->prevAzDemand[0] = pa;
   internal_params->prevAzDemand[1] = pb;
   internal_params->prevAzDemand[2] = pc;
   internal_params->prevAzVel       = vel;

/* Normal exit. */
   return 0;
}

/*
**  - - - - - - - - - - - -
**   fit_new_EL_demand
**  - - - - - - - - - - - -
**
*/
int mcs_sim_fit_new_EL_demand( double timeA, double *posA,
                       double timeB, double *posB,
                       double timeC, double *posC,
                       double maxVel, double maxAcc,
                       double currentPos, 
                       long trajectoryMode,
                       int recent,
		       mcs_parameters *internal_params)

/* sign(A,B) - magnitude of A with sign of B (double) */
#define sign(A,B) ((B)<0.0?-(A):(A))

{
   double tw, pw, d;
   double vel, velPos, accel;
   double newpos = 0;
   double pa, pb, pc;
   double pospa, pospb, pospc;
   double targetPos, distanceLeft;
   double prevpa, prevpb, prevpc;
   double ta, tb, tc;
   double jump;
   int    flag  = 0;
   int    index = 0;

   pospa = *posA; 
   pospb = *posB; 
   pospc = *posC; 
   
   ta = timeA;
   tb = timeB;
   tc = timeC;

   /* in FIT_NEW */ 
   if (internal_params->firstElFit)
   {
       internal_params->prevElDemand[0] = pa = currentPos;
       internal_params->prevElDemand[1] = pb = currentPos;
       internal_params->prevElDemand[2] = pc = currentPos;
       internal_params->prevElVel       = 0.0;
       internal_params->firstElFit = 0;
       printf("firstElFit = 0\n");
   } else {
       pa = internal_params->prevElDemand[0];
       pb = internal_params->prevElDemand[1];
       pc = internal_params->prevElDemand[2];
   } 
   

   prevpa = pa;
   prevpb = pb;
   prevpc = pc;
   

   if ( (ta > tb) && (ta > tc) )
      {
         index = 0;
         newpos = pa = *posA;
      }
   else if ( (tb > ta) && (tb > tc) )
      {
         index = 1;
         newpos = pb = *posB;
      }
   else if ( (tc > ta) && (tc > tb) )
      {
         index = 2;
         newpos = pc = *posC;
      } 

/* Sort so that most recent two points are (tb,pb) and (tc,pc). */
   if ( ta > tb ) {
      tw = ta;  pw = pa;
      ta = tb;  pa = pb;
      tb = tw;  pb = pw;
   }
   if ( tb > tc ) {
      tw = tb;  pw = pb;
      tb = tc;  pb = pc;
      tc = tw;  pc = pw;
   }
   if ( ta > tb ) {
      tw = ta;  pw = pa;
      ta = tb;  pa = pb;
      tb = tw;  pb = pw;
   }
 
   jump = fabs(pc - pb);
 
   if ( jump >= 0.1)
      maxAcc = 2.0 * maxAcc;

   /*newpos    = pc; */
   targetPos = pc;

/* Determinant (must be non-zero). */
   if ( ( d = tc - tb ) == 0.0 ) return -1; 
   

/* Solution. */
   vel       = ( pc - pb ) / d; 

/* Apply Velocity Limit */
   if ( fabs(vel) > maxVel)
   {
      vel = sign( maxVel, vel);
      flag = 1;
   }

   accel = (vel - internal_params->prevElVel)/d;

   /* Apply Acceleration Limit */
   if ( fabs(accel) > maxAcc)
   {
       accel = sign( maxAcc, accel);
       vel   = internal_params->prevElVel + (double)d * accel; 
       flag = 1;
   }

/* Apply Velocity Limit (yes again!!!)*/
   if ( fabs(vel) > maxVel)
   {
      vel = sign( maxVel, vel);
      accel = (vel - internal_params->prevElVel)/(double)d;
      flag = 1;
   }

/* override the velocity demand if close to final position */
/* which direction are we trying to head in? */
   if ( vel > 0.0 )
   {
      distanceLeft = fabs(targetPos - pb);

      if ( (targetPos - pb) < 0)
      {
          distanceLeft = 0.0;
      }

      velPos = sqrt((double) 2.0 * maxAcc * distanceLeft);
      if ( (vel > velPos) ) 
      {
          vel = velPos; 
          accel = (vel - internal_params->prevElVel)/(double)d; 
          flag  = 1; 
      } 
   } 
   else
   { /* same case for negative velocity */

      distanceLeft = fabs(pb - targetPos);
      
      if ( (targetPos - pb) > 0)
      {
          distanceLeft = 0.0;
      }

      velPos = -sqrt((double) 2.0 * maxAcc * distanceLeft);
      if ( (vel < velPos) ) 
      {
          vel = velPos; 
          accel = (vel - internal_params->prevElVel)/(double)d;
          flag = 1; 
      } 
   } 

/* Adjust new position */
 
   if (flag)
   {
      newpos =  internal_params->prevElVel * d + (double)0.5*accel*d*d + pb;
   }
   
/* Check new position */
   if ( (flag) && (((vel > 0) && (newpos > targetPos)) 
               || ((vel < 0) && (newpos < targetPos))) )
       newpos = targetPos;

   switch (index)
      {
      case 0:
          *posA = pa = newpos;
          *posB = pb = prevpb;
          *posC = pc = prevpc;
          break;
      case 1:
          *posA = pa = prevpa;
          *posB = pb = newpos;
          *posC = pc = prevpc;
          break;
      case 2:
          *posA = pa = prevpa;
          *posB = pb = prevpb;
          *posC = pc = newpos;
          break;
      default:
	  printf ("Incorrect value of index - %i", index);
	  break;
      }

   internal_params->prevElDemand[0] = pa;
   internal_params->prevElDemand[1] = pb;
   internal_params->prevElDemand[2] = pc;
   internal_params->prevElVel       = vel;

/* Normal exit. */
   return 0;
}
