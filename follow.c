#include <epicsStdlib.h>
#include <epicsStdioRedirect.h>
#include <epicsExport.h>
#include <registryFunction.h>
#include <string.h>
#include <iocsh.h> 
#include <math.h>
#include <time.h>
#include <epicsPrint.h>

#include <dbEvent.h>
#include <dbDefs.h>
#include <dbCommon.h>
#include <recSup.h>
#include <subRecord.h>
#include <genSubRecord.h>
#include <mcssir.h>
#include <mcscad.h>
#include <mcs.h>
#include <cadRecord.h>
#include <cad.h>
#include <menuCarstates.h>
#include <timeLib.h>

#define TIME_INT        0.005  /* 5 msec                             */
#define TRIGGER_LATENCY 0.1    /* Seconds before Bancomm trigger     */
#define JUMP            0.1    /* Degrees change considered a slew   */
#define AZ_JUMP         0.1    /* Degrees change considered a slew   */
#define EL_JUMP         0.1    /* Degrees change considered a slew   */
#define LOG_LIMIT       72000  /* One hour of TCS logging capability */
#define NUM_EXTRAP	20	/* number of points to extrapolate    */
#define	DOUBLE_BUFF	(NUM_EXTRAP>10)
#define	LOGGING_ON	TRUE

#define MIN(x,y)	((x<y)?x:y)
#define MIN3(a,b,c)	((a<b) ? ((a<c) ? a : c) : ((b<c) ? b : c))

long fillBuffer		(double *, double *, double *, double *, double *,
			 double, long, double *, double, double, double,
                         double, double, long, int);
long calc_coeffs	(double *, double *, double *, double *, double *,
			 double *);
int calc_linear		(double, double, double, double, double, double,
			 double, double *, double *, double *);
int calc_quadratic	(double, double, double, double, double, double,
			 double, double *, double *, double * );
int fit_new_AZ_demand   (double, double *, double, double *, double,
                         double *, double, double, double, long, int);
int fit_new_EL_demand   (double, double *, double, double *, double,
                         double *, double, double, double, long, int);


#if LOGGING_ON
/* -----------------------------------------------------------------*/

#define	LOG_NONE	0
#define	LOG_AZIMUTH	1
#define	LOG_ELEVATION	2

#define	LOG_FIT         3
#define	LOG_AZ_FIT      5
#define	LOG_EL_FIT      6
#define	LOG_PMAC        4
#define	LOG_AZ_PMAC     7
#define	LOG_EL_PMAC     8
#define	LOG_AZ_CALC     9
#define	LOG_EL_CALC     10

#define	LOG_TCS_MAX	1000
#define	LOG_AZEL_MAX	1000
#define	LOG_EXTRAP_MAX	6

int	logSingleFlag = TRUE;
int	logEnableFlag = LOG_NONE;
int	logFitFlag    = LOG_NONE;
int	logPmacFlag   = LOG_NONE;
int	logCalcFlag   = LOG_NONE;
int     loggingFlag   = LOG_NONE;

/* Time interrupt log structure.
 */
struct logTimeStruct {
    int		hour;
    int		min;
    int		sec;
    int		frac;
} logTimeData;

/* TCS demands log structure.
 */
struct logTcsStruct {
    double	t;
    double	az;
    double	el;
};

/* trajectory calculation log structure.
 */
struct logTrajStruct {
    double	prevVel;
    double	vel;
    double	velPos;
    double	accel;
    double	prevpa;
    double	prevpb;
    double	prevpc;
    double	pa;
    double	pb;
    double	pc;
    double	currentPos;
    double	newpos;
    double	targetPos;
};

/* Pmac demand and velocity log structure.
 */
struct logPmacStruct {
    double	pos[NUM_EXTRAP];
    double	vel[NUM_EXTRAP];
    double      lastPos;
    double      lastVel;
    double      acceleration;
};

/* Tracking log structure.
 */
struct logTrackStruct {
    int		word;
    double	toff;
    double      startT;
    double      least;
    int         ticks;
    double	t1, t2, t3;
    double	v1, v2, v3;
    double	pos[LOG_EXTRAP_MAX];
    double	vel[LOG_EXTRAP_MAX];
};

/* calc_linear log structure.
 */
struct logCalcStruct {
    double	toff;
    double	ta;
    double	tb;
    double	tc;
    double	pa;
    double	pb;
    double	pc;
    double	A;
    double	B;
    double	C;
    double	pos;
    double	vel;
};

/* TCS log array and counter.
 */
struct logTcsStruct	logTcsData[LOG_TCS_MAX];
int			logTcsCounter = 0;

/* Tracking log array and counter.
 */
struct logTrackStruct	logTrackData[LOG_AZEL_MAX];
int			logTrackCounter = 0;

/* Pmac log array and counter.
 */
struct logPmacStruct	logPmacData[LOG_AZEL_MAX];
int			logPmacCounter = 0;

/* Trajectory calculation log array and counter.
 */
struct logTrajStruct	logTrajData[LOG_AZEL_MAX];
int			logTrajCounter = 0;

/* calculation log array and counter.
 */
struct logCalcStruct	logCalcData[LOG_AZEL_MAX];
int			logCalcCounter = 0;

void logSingle ();
void logWrap ();
void logTracking ();
void logAzEnable ();
void logElEnable ();
void logDisable ();
void logShow ();
void logTime (int, int, int, int);
void logTcs (double, double, double);
extern void logTrack (double[], double[], double[], double[], double[], double, double, double, int, long, long, long, long);

void dumpTimeLog ();
void dumpTcsLog ();
void dumpTrackLog ();
void dumpTimeLog ();
void dumpLogs();
void logClean ();

void logPmacEnable();
void logPmacAzEnable();
void logPmacElEnable();
void logPmacDisable();
void logPmacClean();
void dumpPmac();
extern void logPmac(double[], double[], double, double, double);

void logFitEnable();
void logFitAzEnable();
void logFitElEnable();
void logFitDisable();
void logFitClean();
void dumpFit();

extern void logFit(double, double, double, double, double, double, double, double, double, double, double, double, double);

void logCalcAzEnable();
void logCalcElEnable();
void logCalcDisable();
void logCalcClean();
void dumpCalc();
extern void logCalc(double, double, double, double, double, double, double, double, double, double, double, double);


void logTrackingEnd();

/* -----------------------------------------------------------------*/
#endif

#ifdef OLD_LOGGING
long   tcsjj = 0;
double tcsTT[LOG_LIMIT];
double tcsAz[LOG_LIMIT];
double tcsEl[LOG_LIMIT];
long   azjj = 0;
long   eljj = 0;
double faztt[96000];
double fazpos[96000];
double fazvel[96000];
double feltt[96000];
double felpos[96000];
double felvel[96000];
long   tcsjj = 0;
double tcsTT[96000];
double tcsAz[96000];
double tcsEl[96000];
long   azCounter = 0;
double azAA[96000];
double azBB[96000];
double azCC[96000];
long   afCounter = 0;
double azFit1[96000];
double azFit2[96000];
double azFit3[96000];
long logging = 0;
#endif
static int    firstAzFit = 1;	/* TRUE */
static int    firstElFit = 1;	/* TRUE */


/* initFollowA - Initialize the counter (offset) where to store the first
 * array coming from the TCS.
 */
long initFollowA (struct genSubRecord *pgsub)
{
    *(long *)pgsub->vale = 0;	/* first array at offset zero */
    return(0);
}
 

/*
 *  This routine is called whenever the
 *  20Hz data stream hits the MCS
 *
 *  Inputs:
 *  -------
 *  pgsub->a = TCS Logging switch		LONG
 *  pgsub->b = Az. lower limit			DOUBLE
 *  pgsub->c = Az. upper limit			DOUBLE
 *  pgsub->d = El. lower limit			DOUBLE
 *  pgsub->e = El. upper limit			DOUBLE
 *  pgsub->j = Array of 5 from TCS		DOUBLE
 *
 *  Outputs:
 *  --------
 *  pgsub->vala = 5 element array from TCS	DOUBLE
 *  pgsub->valb = 5 element array from TCS	DOUBLE
 *  pgsub->valc = 5 element array from TCS	DOUBLE
 *  pgsub->vald = Which of above is latest	LONG
 *  pgsub->vale = One more than above		LONG
 *  pgsub->valf = Azimuth demand from TCS	DOUBLE
 *  pgsub->valg = Elevation demand from TCS	DOUBLE
 *
 * This routine reads the input array from the TCS and writes it to one
 * of the three (least recent) output array. It also copies the azimuth
 * and elevation demands to independent output links.
*/

long FollowA (struct genSubRecord *pgsub)
{
    long   logSwitch;
    double azLower;
    double azUpper;
    double elLower;
    double elUpper;
    double *inptr;
    double *outptr = NULL;
    double *prevptr = NULL;
    int    i;

    /* Get input fields.
     */
    logSwitch = *(long *)   pgsub->a;	/* log flag */
    azLower   = *(double *) pgsub->b;	/* az lower limit */
    azUpper   = *(double *) pgsub->c;	/* az upper limit */
    elLower   = *(double *) pgsub->d;	/* el lower limit */
    elUpper   = *(double *) pgsub->e;	/* el upper limit */
    inptr     =  (double *) pgsub->j;	/* pointer to input array from TCS */

#ifdef OLD_LOGGING
    logging = logSwitch;
    if (logging)
    {
	tcsTT[tcsjj] = *(inptr+1);
	tcsAz[tcsjj] = *(inptr+3);
	tcsEl[tcsjj] = *(inptr+4);
	tcsjj++;
	tcsjj %= 96000;
    }
#endif

#if LOGGING_ON
    if (logEnableFlag != LOG_NONE)
	logTcs (*(inptr+1), *(inptr+3), *(inptr+4));
#endif

    /* Check for end-of-travel limits. Demands out range are set
     * to the limit values.
     */
    if (*(inptr+3) > azUpper)
	*(inptr+3) = azUpper;
    if (*(inptr+3) < azLower)
	*(inptr+3) = azLower;
    if (*(inptr+4) > elUpper)
	*(inptr+4) = elUpper;
    if (*(inptr+4) < elLower)
	*(inptr+4) = elLower;

#ifdef OLD_LOGGING
    if( logSwitch )
    {
	tcsTT[tcsjj] = *(inptr+1);
	tcsAz[tcsjj] = *(inptr+3);
	tcsEl[tcsjj] = *(inptr+4);
	tcsjj++;
	if( tcsjj == LOG_LIMIT )
	    *(long *) pgsub->a = 0;    /* Switch-Off logging */
    }
#endif

    /* Get pointer to the next output array.
     */
    *(long *) pgsub->vale %= 3;
    switch (*(long *)pgsub->vale)
    {
    case 0:
	outptr = (double *) pgsub->vala;
	prevptr = (double *) pgsub->valb;
	break;

    case 1:
	outptr = (double *) pgsub->valb;
	prevptr = (double *) pgsub->valc;
	break;

    case 2:
	outptr = (double *) pgsub->valc;
	prevptr = (double *) pgsub->vala;
	break;

    default:
	printf ("followA: Error in switch (%ld)\n", *(long *) pgsub->vale);
	break;
    }

    /* Output demand from TCS. The azimuth and elevation demands
     * are stored in the fourth and fifth elements of the array.
     */
    *(double *) pgsub->valf = *(prevptr+3);	/* az */
    *(double *) pgsub->valg = *(prevptr+4);	/* el */

    /* Copy input array into output array.
     */
    for (i=0; i<5; i++)
	*(outptr++) = *(inptr++);

    /* Update counters.
     */
    *(long *)  pgsub->vald = *(long *) pgsub->vale;
    (*(long *) pgsub->vale)++;

    /* Ok.
     */
    return (0);
}


/*
 * This routine is at the heart of tracking in the MCS.
 *
 * Inputs:
 * -------
 *    pgsub->a = Array of 5 doubles.
 *    pgsub->b = Array of 5 doubles.
 *    pgsub->c = Array of 5 doubles.
 *    pgsub->d = Which one of the above was most recent?    LONG.
 *    pgsub->e = Follow Flag                                LONG.
 *    pgsub->f = Current Track Id                           DOUBLE.
 *    pgsub->g = Azimuth Current Position                   DOUBLE.
 *    pgsub->h = Azimuth Current Velocity                   DOUBLE.
 *    pgsub->i = Internal use:				    LONG.
 *               Previous recent sample
 *    pgsub->j = Not used				    LONG.
 *    pgsub->k = Elevation Current Position                 DOUBLE.
 *    pgsub->l = Elevation Current Velocity                 DOUBLE.
 *    pgsub->m = Azimuth Current Max. Velocity              DOUBLE.
 *    pgsub->n = Elevation Current Max. Velocity            DOUBLE.
 *    pgsub->o = Azimuth Current Max. Acceleration          DOUBLE.
 *    pgsub->p = Elevation Current Max. Acceleration        DOUBLE.
 *    pgsub->q = Handshake bit from Az. PMAC motion program DOUBLE.
 *    pgsub->r = Handshake bit from El. PMAC motion program DOUBLE.
 *    pgsub->s = Trajectory Calculation Mode                LONG.
 *    pgsub->t = az Pmac Demand Position                    DOUBLE.
 *    pgsub->u = el Pmac Demand Position                    DOUBLE.
 *
 * Outputs:
 * --------
 *    pgsub->vala = Array of NUM_EXTRAP Azimuth Positions           DOUBLE.
 *    pgsub->valb = Array of NUM_EXTRAP Azimuth Velocities          DOUBLE.
 *    pgsub->valc = Array of NUM_EXTRAP Elevation Positions         DOUBLE.
 *    pgsub->vald = Array of NUM_EXTRAP Elevation Velocities        DOUBLE.
 *    pgsub->vale = Mask for fanout                         LONG.
 *                  1  (Link 1) = Set trackId.
 *                  2  (Link 2) = Fill Az. buffer.
 *                  4  (Link 3) = Fill El. buffer.  
 *                  8  (Link 4) = Set time interrupt.
 *                  16 (Link 5) = Set Car Error.
 *    pgsub->valf = Time for external Bancomm Trigger       DOUBLE.
 *    pgsub->valg = TrackId                                 DOUBLE.
 *    pgsub->valh = Current Network time delay (sec)        DOUBLE.
 *    pgsub->vali = Number of samples missed                LONG.
 *    pgsub->valj = Fill bottom/top of Az. buffer (1/2)     LONG.
 *    pgsub->valk = Fill bottom/top of El. buffer (1/2)     LONG.
 *    pgsub->vall = Error string                            LONG.
 *
*/

long Tracking (struct genSubRecord *pgsub)
{
    double arrayA[5];
    double arrayB[5];
    double arrayC[5];
    double AA[2];
    double BB[2];
    double CC[2];
    long   recent;
    long   precent;
    long   follow;
    double trackid;
    double azCurrentPos;
    double azCurrentVel;
    double elCurrentPos;
    double elCurrentVel;
    double azCurrentMaxVel;
    double elCurrentMaxVel;
    double azCurrentMaxAcc;
    double elCurrentMaxAcc;
    double azPmacDemandPos;
    double elPmacDemandPos;
    double azPos[NUM_EXTRAP];
    double azVel[NUM_EXTRAP];
    double elPos[NUM_EXTRAP];
    double elVel[NUM_EXTRAP];
    long   mask;
    double *dptrA;
    double *dptrB;
    double *dptrC;
    int    i;
    int    hmsf[4];
    double tnow;
    double applyT;
    double sentT;
    double newid;
    double latestAz;
    double latestEl;
    double least;
    long   azHandShake;
    long   elHandShake;
    long   trajectoryMode;
    long   error;
    double bufferT;
    static int    first      = 1;	/* TRUE */

    static long   prevAzHandShake = 0;	/* top half */
    static long   prevElHandShake = 0;	/* top half */
    static double prevAz;
    static double prevEl;
    static double azTicks;
    static double elTicks;
    static double startT;
    static double lastAzVel;
    static double lastElVel;

    /* Get input values.
     */
    recent          = *(long *) pgsub->d;
    follow          = *(long *) pgsub->e;
/*#if 0 */
    trackid         = *(double *) pgsub->f;
/*#endif */
    precent         = *(long *) pgsub->i;
    azHandShake     = (long)(*(double *) pgsub->q);
    elHandShake     = (long)(*(double *) pgsub->r);

    azCurrentPos    = *(double *) pgsub->g;
    azCurrentVel    = *(double *) pgsub->h;
    elCurrentPos    = *(double *) pgsub->k;
    elCurrentVel    = *(double *) pgsub->l;
    
    azCurrentMaxVel = *(double *) pgsub->m;
    elCurrentMaxVel = *(double *) pgsub->n;
    
    azCurrentMaxAcc = *(double *) pgsub->o;
    elCurrentMaxAcc = *(double *) pgsub->p;

    trajectoryMode  = *(long *) pgsub->s;
    azPmacDemandPos = *(double *) pgsub->t;
    elPmacDemandPos = *(double *) pgsub->u;

    /* Initialize variables.
     */
    error = 0;
    mask  = 0;

    /* Check if follow is enabled. Process only if that's the case.
     */
    if (follow) 
    {
	/* Process if there is a change in any of the handshake words
	 * coming from the PMAC side.
	 */
	if ((azHandShake != prevAzHandShake) ||
	    (elHandShake != prevElHandShake))
	{
	    /* Initialize pointers to input arrays.
	     */
	    dptrA = (double *) pgsub->a;
	    dptrB = (double *) pgsub->b;
	    dptrC = (double *) pgsub->c;

	    /* Copy input arrays into temporary arrays. They are
	     * not necesarily in the right order in time.
	     */
	    for (i=0; i<5; i++)
	    {
		arrayA[i] = *(dptrA++);
		arrayB[i] = *(dptrB++);
		arrayC[i] = *(dptrC++);
	    }

	    /* Get current time.
	     */
	    error = timeNow (&tnow);

	    /* Process the rest of the code only if there is no error
	     * in retrieving the time.
	     */
	    if (error)
	        sprintf (pgsub->vall, "timeNow returned %ld", error);
	    else
	    {
		/* Look for the most recent array in time and copy
		 * these values to temporary variables.
		 */
		switch (recent)
		{
		case 0:
		    sentT    = arrayA[0];
		    applyT   = arrayA[1];
		    newid    = arrayA[2];
		    latestAz = arrayA[3];
		    latestEl = arrayA[4];
		    break;

		case 1:
		    sentT    = arrayB[0];
		    applyT   = arrayB[1];
		    newid    = arrayB[2];
		    latestAz = arrayB[3];
		    latestEl = arrayB[4];
		    break;

		case 2:
		    sentT    = arrayC[0];
		    applyT   = arrayC[1];
		    newid    = arrayC[2];
		    latestAz = arrayC[3];
		    latestEl = arrayC[4];
		    break;

		default:
		    sprintf (pgsub->vall,
			     "Incorrect value of recent - %ld", recent);
		    error = 1;
		    break;
		}

		/* Continue processing if no errors were detected.
		 */
		if (!error)
		{
		    /* Check if this is the first time the routine
		     * is executed.
		     */
		    if (first)
		    {
			/* Set the other two temporary arrays to the
			 * same values of the first and only array.
			 * Reset tick counters.
			 */
			first     = 0;	/* FALSE */

			arrayA[3] = latestAz;
			arrayB[3] = latestAz;
			arrayC[3] = latestAz;
			prevAz    = latestAz;
			
			arrayA[4] = latestEl;
			arrayB[4] = latestEl;
			arrayC[4] = latestEl;
			prevEl    = latestEl;
			azTicks   = 0;
			elTicks   = 0;

			/* Calculate the time at which the motion program
			 * will begin on PMAC. It will be the time in the
			 * input array unless the latter is too close to
			 * the current time.
			 */
			if (applyT - tnow > TRIGGER_LATENCY)
			    startT = applyT;
			else
			    startT = tnow + TRIGGER_LATENCY;
                         
			 /*printf("startT = %.9f\n",startT); */
			/* Convert the PMAC starting raw time (seconds
			 * from an arbitrary origin) into hours, minutes,
			 * seconds, and fraction of seconds.
			 */
			error = timeThenT (startT, TAI, 5, hmsf);

			/* Check for error in time conversion. Calculate
			 * the time for the external bancomm trigger and
			 * the mask to indicate a time interrupt if no
			 * errors are found.
			 */
			if (error)
			    sprintf (pgsub->vall,
				"timeThenT returned %ld", error);
			else
			{
#if LOGGING_ON
			    logTime (hmsf[0], hmsf[1], hmsf[2], hmsf[3]);
#endif
#if 0
			    printf ("Tracking: Interrupt at: %d:%d:%d.%d\n",
				hmsf[0], hmsf[1], hmsf[2], hmsf[3]);
#endif
			    mask = mask | 8;
			    *(double *)pgsub->valf = 60.0*60.0*hmsf[0] +
						60.0 * hmsf[1] +
						hmsf[2] +
						(double) hmsf[3] / 100000;
			}

		    } /* if first */

		} /* if error */

		/* Continue processing if no errors were detected.
		 * This is the branch that will be executed every
		 * time (not just the first one).
		 */
		if (!error)
		{
		    /* Calculate the current network delay. This is done
		     * only when there is a change in the array index.
		     */
		    if (recent != precent)
			*(double *) pgsub->valh = tnow - sentT;

		    /* Increment the counter of samples missed if the
		     * time to apply the demand is already in the past.
		     */
		    if (applyT <= tnow)
			(*(long *)pgsub->vali)++;

		    /* Output the last track Id.
		     */
		    *(double *) pgsub->valg = newid;

		    /* Has track Id changed? If that's the case flag a
		     * track id change in the mask.
		     */
		    if ( trackid != newid)
                    {
			mask = mask | 1;
			*(long *)pgsub->valr = 1; 
                    }
		    else
		    {
			*(long *)pgsub->valr = 0; 
		    }

		    /* Calculate the minimum value of the time to apply the
		     * demands and substract this value from the three
		     * times, thus the times will be offsets relative to
		     * the smallest time.
		     */
		    least      = MIN (arrayA[1], arrayB[1]);
		    least      = MIN (least, arrayC[1]);
		    arrayA[1] -= least;
		    arrayB[1] -= least;
		    arrayC[1] -= least;

		    /* Az. Process only if there is a change in the
		     * handshake word coming from the PMAC so we don't
		     * write while the PMAC is still reading.
		     */
		    if (azHandShake != prevAzHandShake)
		    {
			/* Calculate starting time for buffer. This time
			 * is also relative to the minimum time in the
			 * three input buffers. azTicks is multiplied by
			 * 0.05 (20Hz) because that's the time between
			 * two subsequent demands from the TCS.
			 */
			if (NUM_EXTRAP > 10)
			    bufferT = startT + (double)(azTicks) * (double)(0.1) - least;
			else
			    bufferT = startT + azTicks * 0.05 - least;

			/* Copy time to apply the demands and the demands
			 * to temporary arrays.
			 */
			AA[0] = arrayA[1];	/* apply time */
			AA[1] = arrayA[3];	/* az demand */
			BB[0] = arrayB[1];	/* apply time */
			BB[1] = arrayB[3];	/* az demand */
			CC[0] = arrayC[1];	/* apply time */
			CC[1] = arrayC[3];	/* az demand */

#ifdef OLD_LOGGING
			azFit1[afCounter] = arrayA[3];
			azFit2[afCounter] = arrayB[3];
			azFit3[afCounter] = arrayC[3];
			afCounter++;
			afCounter %= 9600;
#endif

			/* Extrapolate demands starting just after the
			 * last demand from the TCS.
			 */
			error = fillBuffer (AA, BB, CC, azPos,
                                            azVel,  bufferT, 1,
					    &prevAz, AZ_JUMP, 
                                            azCurrentMaxVel, 
                                            azCurrentMaxAcc,
                                            azCurrentPos,
                                            azCurrentVel, 
                                            trajectoryMode,
                                            (int) recent);

#if LOGGING_ON
			if (logEnableFlag == LOG_AZIMUTH)
			{
			    logTrack (arrayA, arrayB, arrayC,
				      azPos, azVel,
				      bufferT, startT, least, azTicks,
				      azHandShake, elHandShake,
				      recent, mask);
			}
#endif

			/* Check the handshake flag coming from the PMAC
			 * and set the output link to write to the buffer
			 * that's not being read on the PMAC side.
			 */
			if (azHandShake == 0)
			    *(long *) pgsub->valj = 2; /* Fill top-half */
			else
			    *(long *) pgsub->valj = 1; /* Fill bottom-half */
		        /*if (!slewFlag)*/	
                         mask = mask | 2;

			/* Count number of times this routine has been
			 * called. This counter is used to calculate the
			 * absolute time since the start of the motion
			 * program in the PMAC.
			 */
			azTicks++;

			/* Save handshake from PMAC.
			 */
			prevAzHandShake = azHandShake;
		    }

		    /* El. Same as Az logic.
		     */
		    if (elHandShake != prevElHandShake)
		    {
			if (NUM_EXTRAP > 10)
			    bufferT = startT + (double)(elTicks) * (double)(0.1) - least;
			else
			    bufferT = startT + elTicks * 0.05 - least;

			AA[0] = arrayA[1];
			AA[1] = arrayA[4];
			BB[0] = arrayB[1];
			BB[1] = arrayB[4];
			CC[0] = arrayC[1];
			CC[1] = arrayC[4];

			error = fillBuffer (AA, BB, CC, elPos,
                                            elVel,  bufferT, 2,
					    &prevEl, EL_JUMP, 
                                            elCurrentMaxVel, 
                                            elCurrentMaxAcc, 
                                            elCurrentPos,
                                            elCurrentVel,
                                            trajectoryMode,
                                            (int) recent);
			
#if LOGGING_ON
			if (logEnableFlag == LOG_ELEVATION)
			{
			    logTrack (arrayA, arrayB, arrayC,
				      elPos, elVel,
				      bufferT, startT, least, elTicks,
				      azHandShake, elHandShake,
				      recent, mask);
			}
#endif

			if (elHandShake == 0)
			    *(long *)pgsub->valk = 2; /* Fill top-half */
			else
			    *(long *)pgsub->valk = 1; /* Fill bottom-half */

                        mask = mask | 4;

			elTicks++;
			prevElHandShake = elHandShake;
		    }

		    *(long *) pgsub->i = recent;
	        }
	    }
	}
    }
    else
    {
	first           = 1;
	firstAzFit      = 1;
	firstElFit      = 1;
	prevAzHandShake = 0;
	prevElHandShake = 0;
    }

    /* Flag an error condition if an error was detected.
     */
    if (error)
	mask = 16;

    /* Copy mask and buffers to output links.
     */
     
    *(long *) pgsub->vale = mask;
       memcpy (pgsub->vala, azPos, NUM_EXTRAP * sizeof(double));
       memcpy (pgsub->valb, azVel, NUM_EXTRAP * sizeof(double));
       memcpy (pgsub->valc, elPos, NUM_EXTRAP * sizeof(double));
       memcpy (pgsub->vald, elVel, NUM_EXTRAP * sizeof(double));
    *(double *)pgsub->valp = azPos[0];
    *(double *)pgsub->valq = elPos[0];

    lastAzVel = azVel[NUM_EXTRAP-1];
    lastElVel = elVel[NUM_EXTRAP-1];
    /* Ok.
     */
    return(0);
}


/* fillBuffer - Extrapolate demands
 */
long fillBuffer (double *AA,  double *BB,  double *CC, 
                 double *pos, double *vel, double offset, 
                 long axis,   double *lastPMACDemand, double jump,
                 double maxVel, double maxAcc, double currentPos,
                 double currentVel, long trajectoryMode, int recent)
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
    static double azA;
    static double azB;
    static double azC;
    static double elA;
    static double elB;
    static double elC;
    static double lastAzVelocity;
    static double lastElVelocity;
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
	    A = azA;
	    B = azB;
	    C = azC;
	}
	else
	{
	    A = elA;
	    B = elB;
	    C = elC;
	}
    }

#ifdef OLD_LOGGING
    if (logging)
    {
	if (axis == 1)
	{
	    azAA[azCounter] = A;
	    azBB[azCounter] = B;
	    azCC[azCounter] = C;
	    azCounter++;
	    azCounter %= 96000;
	}
    }
#endif

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

    if ( (axis == 1) && (logCalcFlag == LOG_AZ_CALC) )
    {
        logCalc(offset, timeA, timeB, timeC, posA, posB, posC, A, B, C,
                pos[0], vel[0]);
    } else if ( (axis == 2) && (logCalcFlag == LOG_EL_CALC) )
    {
        logCalc(offset, timeA, timeB, timeC, posA, posB, posC, A, B, C,
                pos[0], vel[0]);
    }

    /* Check whether the azimuth velocity changed its sign. Print a
     * warning message if that's the case.
     */
    if ( (axis == 1) && (logPmacFlag == LOG_AZ_PMAC) )
    {
        logPmac(pos, vel, lastPos, lastVel, acceleration);
    } else if ((axis == 2) && (logPmacFlag == LOG_EL_PMAC) )
    {
        logPmac(pos, vel, lastPos, lastVel, acceleration);
    }

#ifdef OLD_LOGGING
    if (logging)
    {
	for (i=0; i<NUM_EXTRAP; i++)
	{
	    if (axis == 1)
	    {
		fazpos[azjj] = pos[i];
		fazvel[azjj] = vel[i];
		faztt[azjj]  = tt;
		azjj++;
		azjj %= 96000;
	    }
	    else
	    {
		felpos[eljj] = pos[i];
		felvel[eljj] = vel[i];
		feltt[eljj]  = tt;
		eljj++;
		eljj %= 96000;
	    }
	}
    }

#endif

    /* Save coefficients for next call in case the fit fails.
     */
    if (axis == 1)
    {
	azA = A;
	azB = B;
	azC = C;
    }
    else
    {
	elA = A;
	elB = B;
	elC = C;
    }

    /* Put the last PMAC position demand in a separate parameter.
     */
    *lastPMACDemand = pos[NUM_EXTRAP-1];
    if (axis == 1)
	lastAzVelocity = vel[NUM_EXTRAP-1];
    else
        lastElVelocity = vel[NUM_EXTRAP-1];

    return (0);
}


/* calc_coeffs - Not used anymore.
 */
long calc_coeffs (double *aa, double *bb, double *cc, double *A,
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


/* trackingFO - Split an array coming in through B into NUM_EXTRAP single
 * elements. These will be written to PMAC DPRAM further down the
 * chain of processing.
 */
long trackingFO (struct genSubRecord *pgsub)
{
    double *dptr;
    int    i;

    dptr = (double *)pgsub->b;

    for (i=0; i<NUM_EXTRAP; i++)
    {
	switch(i)
	{
	case 0:
	    *(double *)pgsub->vala = *(dptr++);
	    break;

	case 1:
	    *(double *)pgsub->valb = *(dptr++);
	    break;

	case 2:
	    *(double *)pgsub->valc = *(dptr++);
	    break;

	case 3:
	    *(double *)pgsub->vald = *(dptr++);
	    break;

	case 4:
	    *(double *)pgsub->vale = *(dptr++);
	    break;

	case 5:
	    *(double *)pgsub->valf = *(dptr++);
	    break;

	case 6:
	    *(double *)pgsub->valg = *(dptr++);
	    break;

	case 7:
	    *(double *)pgsub->valh = *(dptr++);
	    break;

	case 8:
	    *(double *)pgsub->vali = *(dptr++);
	    break;

	case 9:
	    *(double *)pgsub->valj = *(dptr++);
	    break;

#if NUM_EXTRAP>10
	case 10:
	    *(double *)pgsub->valk = *(dptr++);
	    break;

	case 11:
	    *(double *)pgsub->vall = *(dptr++);
	    break;

	case 12:
	    *(double *)pgsub->valm = *(dptr++);
	    break;

	case 13:
	    *(double *)pgsub->valn = *(dptr++);
	    break;

	case 14:
	    *(double *)pgsub->valo = *(dptr++);
	    break;

	case 15:
	    *(double *)pgsub->valp = *(dptr++);
	    break;

	case 16:
	    *(double *)pgsub->valq = *(dptr++);
	    break;

	case 17:
	    *(double *)pgsub->valr = *(dptr++);
	    break;

	case 18:
	    *(double *)pgsub->vals = *(dptr++);
	    break;

	case 19:
	    *(double *)pgsub->valt = *(dptr++);
	    break;
#endif
	default:
	    printf ("trackingFO: Should never see this\n");
	    break;
        }
    }

    return(0);
}


/* readTime -- Get the current time and output each of its components
 (hour, minutes, seconds and fraction of a second to separate links.
 */
long readTime (struct subRecord *psub)
{
    long error;
    int  hmsf[4];

    error = timeNowT (TAI, 5, hmsf);
    printf ("Time after setting interrupt is:  %d:%d:%d.%d\n",
	    hmsf[0], hmsf[1], hmsf[2], hmsf[3]);
    return (0);
}


#ifdef OLD_LOGGING
void dump_file ()
{
    FILE *fd;
    long i;

    fd = fopen ("azDemands.dat", "w+");
    for (i=0; i<azjj; i++)
        fprintf (fd, "%f  %f,  %f\n", faztt[i], fazpos[i], fazvel[i]);
    fclose (fd);
    printf ("Number of PMAC demands = %d\n", azjj);
    azjj = 0;

    fd = fopen ("elDemands.dat", "w+");
    for (i=0; i<eljj; i++)
        fprintf (fd, "%f  %f,  %f\n", feltt[i], felpos[i], felvel[i]);
    fclose (fd);
    eljj = 0;

    fd = fopen ("tcs.dat", "w+");
    for (i=0; i<tcsjj; i++)
        fprintf (fd, "%f,  %f,  %f\n", tcsTT[i], tcsAz[i], tcsEl[i]);
    fclose (fd);
    printf ("Number of TCS samples = %d\n", tcsjj);
    tcsjj = 0;

    fd = fopen ("azCoeffs.dat", "w+");
    for (i=0; i<azCounter; i++)
        fprintf (fd, "%f,  %f,  %f\n", azAA[i], azBB[i], azCC[i]);
    fclose (fd);
    azCounter = 0;

    fd = fopen ("azFit.dat", "w+");
    for (i=0; i<afCounter; i++)
        fprintf (fd, "%f,  %f,  %f\n", azFit1[i], azFit2[i], azFit3[i]);
    fclose (fd);
    afCounter = 0;
}
#endif


#ifdef OLD_LOGGING
void dump_file ()
{
    FILE *fd;
    int  i;

    fd = fopen ("tcs.dat", "w+");
    for (i=0; i<tcsjj; i++)
        fprintf (fd, "%f,  %f,  %f\n", tcsTT[i], tcsAz[i], tcsEl[i]);
    fclose (fd);
    printf ("Number of TCS samples = %d\n", tcsjj);
    tcsjj = 0;
}
#endif

int calc_linear (double dpmax,
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


int calc_quadratic( double dpmax,
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

/*
 * This routine perform the trajectory calculation for
 * the demands from the TCS.
 *
 * Inputs:
 * -------
 *    pgsub->a = Array of 5 doubles.
 *    pgsub->b = Array of 5 doubles.
 *    pgsub->c = Array of 5 doubles.
 *    pgsub->d = Which one of the above was most recent?    LONG.
 *    pgsub->e = Follow Flag                                LONG.
 *    pgsub->f = Trajectory Calculation Mode                LONG.
 *    pgsub->g = Azimuth Current Position                   DOUBLE.
 *    pgsub->h = Elevation Current Position                 DOUBLE.
 *    pgsub->i = Azimuth Current Max. Velocity              DOUBLE.
 *    pgsub->j = Elevation Current Max. Velocity            DOUBLE.
 *    pgsub->k = Azimuth Current Max. Acceleration          DOUBLE.
 *    pgsub->l = Elevation Current Max. Acceleration        DOUBLE.
 *
 * Outputs:
 * --------
 *    pgsub->vala = Array of 5 doubles.                     DOUBLE.
 *    pgsub->valb = Array of 5 doubles.                     DOUBLE.
 *    pgsub->valc = Array of 5 doubles.                     DOUBLE.
 *    pgsub->vald = Which of above is latest	            LONG
 *
 */

long TrajecCalc (struct genSubRecord *pgsub)
{
    double arrayA[5];
    double arrayB[5];
    double arrayC[5];
    long   recent;
    long   follow;
    double azCurrentPos;
    double elCurrentPos;
    double azCurrentMaxVel;
    double elCurrentMaxVel;
    double azCurrentMaxAcc;
    double elCurrentMaxAcc;
    double *dptrA;
    double *dptrB;
    double *dptrC;
    double *outptr; 
    int    i;
    double applyT;
    double sentT;
    double newid;
    double latestAz;
    double latestEl;
    double least;
    long   trajectoryMode;
    long   error;

    static double prevAz;
    static double prevEl;
    static double posAzA, posElA;
    static double posAzB, posElB;
    static double posAzC, posElC;
    double timeA, timeB, timeC;
    static int firstTraj = 1;

    /* Get input values.
     */
    recent          = *(long *) pgsub->d;
    follow          = *(long *) pgsub->e;
    trajectoryMode  = *(long *) pgsub->f;

    azCurrentPos    = *(double *) pgsub->g;
    elCurrentPos    = *(double *) pgsub->h;
    
    azCurrentMaxVel = *(double *) pgsub->i;
    elCurrentMaxVel = *(double *) pgsub->j;
    
    azCurrentMaxAcc = *(double *) pgsub->k;
    elCurrentMaxAcc = *(double *) pgsub->l;


    /* Initialize variables.
     */
    error = 0;

    /* Check if follow is enabled. Process only if that's the case.
     */
    /*if (follow) */
	    /* Initialize pointers to input arrays.
	     */
	    dptrA = (double *) pgsub->a;
	    dptrB = (double *) pgsub->b;
	    dptrC = (double *) pgsub->c;

	    /* Copy input arrays into temporary arrays. They are
	     * not necesarily in the right order in time.
	     */
	    for (i=0; i<3; i++)
	    {
		arrayA[i] = *(dptrA++);
		arrayB[i] = *(dptrB++);
		arrayC[i] = *(dptrC++);
	    }

	    /* Look for the most recent array in time and copy
	     * these values to temporary variables.
	     */
	    switch (recent)
	    {
	    case 0:
	        sentT    = arrayA[0];
	        applyT   = arrayA[1];
	        newid    = arrayA[2];
	        for (i=3; i<5; i++)
		    arrayA[i] = *(dptrA++);
	        posAzA   = latestAz = arrayA[3];
	        posElA   = latestEl = arrayA[4];
	        break;

	    case 1:
	        sentT    = arrayB[0];
	        applyT   = arrayB[1];
	        newid    = arrayB[2];
	        for (i=3; i<5; i++)
		    arrayB[i] = *(dptrB++);
	        posAzB   = latestAz = arrayB[3];
	        posElB   = latestEl = arrayB[4];
	        break;

	    case 2:
	        sentT    = arrayC[0];
	        applyT   = arrayC[1];
	        newid    = arrayC[2];
	        for (i=3; i<5; i++)
		    arrayC[i] = *(dptrC++);
	        posAzC   = latestAz = arrayC[3];
	        posElC   = latestEl = arrayC[4];
	        break;

	    default:
	        sprintf (pgsub->vall,
		         "Incorrect value of recent - %ld", recent);
	        error = 1;
	        break;
	    }

	    timeA = arrayA[1];
	    timeB = arrayB[1];
	    timeC = arrayC[1];


		/* Continue processing if no errors were detected.
		 * This is the branch that will be executed every
		 * time (not just the first one).
		 */
/*    if (trajectoryMode != 0) */
    if (follow) 
    {

		    /* Calculate the minimum value of the time to apply the
		     * demands and substract this value from the three
		     * times, thus the times will be offsets relative to
		     * the smallest time.
		     */
		    least      = MIN (arrayA[1], arrayB[1]);
		    least      = MIN (least, arrayC[1]);
		    timeA     -= least;
		    timeB     -= least;
		    timeC     -= least;

/*printf("BEFORE timeA = %f  posAzA = %f  posAzB = %f posAzC = %f\n",timeA, posAzA, posAzB, posAzC);*/

        if ( trajectoryMode != 0)
        {
            fit_new_AZ_demand(timeA, &posAzA, timeB, &posAzB, timeC, &posAzC, azCurrentMaxVel, azCurrentMaxAcc, azCurrentPos, trajectoryMode, recent); 
/*printf("AFTER timeA = %f  posAzA = %f  posAzB = %f posAzC = %f\n",timeA, posAzA, posAzB, posAzC);*/
            fit_new_EL_demand(timeA, &posElA, timeB, &posElB, timeC, &posElC, elCurrentMaxVel, elCurrentMaxAcc, elCurrentPos, trajectoryMode, recent); 
        }

        *(long *) pgsub->i = recent;
    } /* if follow */
    else
    {
        posAzA = azCurrentPos;
        posAzB = azCurrentPos;
        posAzC = azCurrentPos;
        posElA = elCurrentPos;
        posElB = elCurrentPos;
        posElC = elCurrentPos;
	prevAz = azCurrentPos;
	prevEl = elCurrentPos;
	firstTraj = 0;

    }

    switch (recent)
    {
    case 0:
        arrayA[3] = posAzA;     /* az demand */
        arrayA[4] = posElA;     /* el demand */
	outptr    = (double *) pgsub->vala;
	for (i=0; i<5; i++)
	{
	    *(outptr++) = arrayA[i];
	}
        break;

    case 1:
        arrayB[3] = posAzB;     /* az demand */
        arrayB[4] = posElB;     /* el demand */
	outptr    = (double *) pgsub->valb;
	for (i=0; i<5; i++)
	{
	    *(outptr++) = arrayB[i];
	}
        break;

    case 2:
        arrayC[3] = posAzC;     /* az demand */
        arrayC[4] = posElC;     /* el demand */
	outptr    = (double *) pgsub->valc;
	for (i=0; i<5; i++)
	{
	    *(outptr++) = arrayC[i];
	}
        break;

    default:
        sprintf (pgsub->vall,
	         "Incorrect value of recent - %ld", recent);
        error = 1;
        break;
    }

    *(long *)pgsub->vald = recent;

    /* Ok.
     */
    return(0);
}


int fit_new_AZ_demand( double timeA, double *posA,
                       double timeB, double *posB,
                       double timeC, double *posC,
                       double maxVel, double maxAcc,
                       double currentPos, 
                       long trajectoryMode,
                       int recent)
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
   static double prevAzVel;
   int    flag  = 0;
   int    index = 0;
   static double prevAzDemand[3];

   pospa = *posA; 
   pospb = *posB; 
   pospc = *posC; 
   
   ta = timeA;
   tb = timeB;
   tc = timeC;

   /* in FIT_NEW */ 
   if (firstAzFit)
   {
       prevAzDemand[0] = pa = currentPos;
       prevAzDemand[1] = pb = currentPos;
       prevAzDemand[2] = pc = currentPos;
       prevAzVel       = 0.0;
       firstAzFit = 0;
       printf("firstAzFit = 0\n");
   } else {
       pa = prevAzDemand[0];
       pb = prevAzDemand[1];
       pc = prevAzDemand[2];
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

   accel = (vel - prevAzVel)/d;

   /* Apply Acceleration Limit */
   if ( fabs(accel) > maxAcc)
   {
       accel = sign( maxAcc, accel);
       vel   = prevAzVel + (double)d * accel; 
       flag = 1;
   }

/* Apply Velocity Limit (yes again!!!)*/
   if ( fabs(vel) > maxVel)
   {
      vel = sign( maxVel, vel);
      accel = (vel - prevAzVel)/(double)d;
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
          accel = (vel - prevAzVel)/(double)d; 
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
          accel = (vel - prevAzVel)/(double)d;
          flag = 1; 
      } 
   } 

/* Adjust new position */
 
   if (flag)
   {
      newpos =  prevAzVel * d + (double)0.5*accel*d*d + pb;
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

    
    if (logFitFlag == LOG_AZ_FIT) 
        {
        logFit(prevAzVel, vel, velPos, accel,
               pospa, pospb, pospc,
               pa, pb, pc, 
               currentPos, newpos, targetPos);
        }

   prevAzDemand[0] = pa;
   prevAzDemand[1] = pb;
   prevAzDemand[2] = pc;
   prevAzVel       = vel; 

/* Normal exit. */
   return 0;
}

/*
**  - - - - - - - - - - - -
**   fit_new_EL_demand
**  - - - - - - - - - - - -
**
*/
int fit_new_EL_demand( double timeA, double *posA,
                       double timeB, double *posB,
                       double timeC, double *posC,
                       double maxVel, double maxAcc,
                       double currentPos, 
                       long trajectoryMode,
                       int recent)

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
   static double prevElVel;
   int    flag  = 0;
   int    index = 0;
   static double prevElDemand[3];

   pospa = *posA; 
   pospb = *posB; 
   pospc = *posC; 
   
   ta = timeA;
   tb = timeB;
   tc = timeC;

   /* in FIT_NEW */ 
   if (firstElFit)
   {
       prevElDemand[0] = pa = currentPos;
       prevElDemand[1] = pb = currentPos;
       prevElDemand[2] = pc = currentPos;
       prevElVel       = 0.0;
       firstElFit = 0;
       printf("firstElFit = 0\n");
   } else {
       pa = prevElDemand[0];
       pb = prevElDemand[1];
       pc = prevElDemand[2];
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

   accel = (vel - prevElVel)/d;

   /* Apply Acceleration Limit */
   if ( fabs(accel) > maxAcc)
   {
       accel = sign( maxAcc, accel);
       vel   = prevElVel + (double)d * accel; 
       flag = 1;
   }

/* Apply Velocity Limit (yes again!!!)*/
   if ( fabs(vel) > maxVel)
   {
      vel = sign( maxVel, vel);
      accel = (vel - prevElVel)/(double)d;
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
          accel = (vel - prevElVel)/(double)d; 
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
          accel = (vel - prevElVel)/(double)d;
          flag = 1; 
      } 
   } 

/* Adjust new position */
 
   if (flag)
   {
      newpos =  prevElVel * d + (double)0.5*accel*d*d + pb;
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

    
    if (logFitFlag == LOG_EL_FIT) 
        {
        logFit(prevElVel, vel, velPos, accel,
               pospa, pospb, pospc,
               pa, pb, pc, 
               currentPos, newpos, targetPos);
        }

   prevElDemand[0] = pa;
   prevElDemand[1] = pb;
   prevElDemand[2] = pc;
   prevElVel       = vel; 

/* Normal exit. */
   return 0;
}

#if LOGGING_ON
/* -----------------------------------------------------------------*/

/* logSingle -- Set single shot flag.
 */
void logSingle ()
{
    logSingleFlag = TRUE;
    printf ("Single shot logging selected\n");
}

/* logWrap -- Reset single shot flag.
 */
void logWrap ()
{
    logSingleFlag = FALSE;
    printf ("Wrap around logging selected\n");
}

/* logTracking -- Start tracking logging.
 */
void logTracking (struct genSubRecord *pgsub)
{
    long axis;

    axis  = *(long *) pgsub->a;

    printf ("Start tracking logging\n");
    logClean();
    logFitClean();
    logPmacClean();
    if ( axis == 0 ) 
    {
	logAzEnable();
	logFitAzEnable();
	logPmacAzEnable();
	logCalcAzEnable();
    } else {
	logElEnable();
	logFitElEnable();
	logPmacElEnable();
	logCalcElEnable();
    }
}

/* logAzEnable -- Reset log counters and enable azimuth logging.
 */
void logAzEnable ()
{
    logTcsCounter   = 0;
    logTrackCounter = 0;
    logEnableFlag   = LOG_AZIMUTH;
    printf (
	"Az logging enabled: TcsCounter=%d, TrackCounter=%d, SingleFlag=%d\n",
	logTcsCounter, logTrackCounter, logSingleFlag);
}

/* logElEnable -- Reset log counters and enable elevation logging.
 */
void logElEnable ()
{
    logTcsCounter   = 0;
    logTrackCounter = 0;
    logEnableFlag   = LOG_ELEVATION;
    printf (
	"El logging enabled: TcsCounter=%d, TrackCounter=%d, SingleFlag=%d\n",
	logTcsCounter, logTrackCounter, logSingleFlag);
}

/* logDisable -- Disable logging.
 */
void logDisable ()
{
    logEnableFlag = 0;
    printf ("Logging disabled: TcsCounter=%d, TrackCounter=%d, SingleFlag=%d\n",
	    logTcsCounter, logTrackCounter, logSingleFlag);
}

void logShow ()
{
    printf ("logSingleFlag   = %d\n", logSingleFlag);
    printf ("logEnableFlag   = %d\n", logEnableFlag);
    printf ("logTcsCounter   = %d\n", logTcsCounter);
    printf ("logTrackCounter = %d\n", logTrackCounter);
}

/* logTime -- Log time interrupt to the bancom card.
 */
void logTime (int hours, int minutes, int seconds, int fraction)
{
    logTimeData.hour = hours;
    logTimeData.min  = minutes;
    logTimeData.sec  = seconds;
    logTimeData.frac = fraction;
}

/* logTcs -- Log the demands received from the TCS.
 */
void logTcs (double time, double az, double el)
{
    logTcsData[logTcsCounter].t  = time;
    logTcsData[logTcsCounter].az = az;
    logTcsData[logTcsCounter].el = el;
    logTcsCounter++;
    logTcsCounter %= LOG_TCS_MAX;
}

/* logTrack -- Log the last three demands received from the TCS and the
 * demands sent to the PMAC.
 */
void logTrack (double arrayA[], double arrayB[], double arrayC[],
	 double pos[], double vel[], double toffset, 
	 double startT, double least, int ticks,
	 long azHs, long elHs, long recent, long mask)
{
    int	size;

    logTrackData[logTrackCounter].word = ((azHs & 0xf) |
				    ((elHs << 4)   & 0xf0) |
				    ((recent << 8) & 0xf00) |
				    ((mask << 12)  & 0x1f000));

    logTrackData[logTrackCounter].toff  = toffset;
    logTrackData[logTrackCounter].startT = startT;
    logTrackData[logTrackCounter].least = least;
    logTrackData[logTrackCounter].ticks = ticks;

    logTrackData[logTrackCounter].t1  = arrayA[1];
    logTrackData[logTrackCounter].t2  = arrayB[1];
    logTrackData[logTrackCounter].t3  = arrayC[1];


    if (logEnableFlag == LOG_AZIMUTH)
    {
	logTrackData[logTrackCounter].v1 = arrayA[3];
	logTrackData[logTrackCounter].v2 = arrayB[3];
	logTrackData[logTrackCounter].v3 = arrayC[3];
    }
    else if (logEnableFlag == LOG_ELEVATION)
    {
	logTrackData[logTrackCounter].v1 = arrayA[4];
	logTrackData[logTrackCounter].v2 = arrayB[4];
	logTrackData[logTrackCounter].v3 = arrayC[4];
    }

    size = (LOG_EXTRAP_MAX - 2) * sizeof(double);
    memcpy (logTrackData[logTrackCounter].pos, pos, size);
    memcpy (logTrackData[logTrackCounter].vel, vel, size);
    logTrackData[logTrackCounter].pos[LOG_EXTRAP_MAX-2] = pos[NUM_EXTRAP-2];
    logTrackData[logTrackCounter].vel[LOG_EXTRAP_MAX-2] = vel[NUM_EXTRAP-2];
    logTrackData[logTrackCounter].pos[LOG_EXTRAP_MAX-1] = pos[NUM_EXTRAP-1];
    logTrackData[logTrackCounter].vel[LOG_EXTRAP_MAX-1] = vel[NUM_EXTRAP-1];

    logTrackCounter++;
    logTrackCounter %= LOG_AZEL_MAX;

    if ((logTrackCounter == 0) && (logSingleFlag)) {
	logEnableFlag = FALSE;
	printf ("Logging disabled\n");
	/*dumpLogs(); */
    }
}

/* waitLogTrackingEnd -- Waits for end of tracking logging.
 *
 *    pgsub->a    = logging flag                        LONG.
 *    pgsub->vala = Mask for fanout                     LONG.
 */
void waitLogTrackingEnd (struct genSubRecord *pgsub)
{

    loggingFlag = *(long *)   pgsub->a;	/* log flag */

    if ( loggingFlag == 1 )
    { 
	if ( (logEnableFlag ==  LOG_NONE) && (logFitFlag == LOG_NONE) &&
	     (logCalcFlag ==  LOG_NONE) && (logPmacFlag == LOG_NONE) )
        {
	    /* Logs finished, start Log dumping */

            *(long *) pgsub->vala = 15;
            *(long *) pgsub->a = 0;
	}
    } else {
        *(long *) pgsub->vala = 0;
    }
}

/* dumpTimeLog -- Dump time interrupt log.
 */
void dumpTimeLog ()
{
    FILE	*fp;

    if ((fp = fopen ("logtime.dat", "w")) == NULL) {
	printf ("Cannot open log file\n");
	return;
    }

    printf ("Time interrupt (h:m:s.s) = %d:%d:%d.%d\n",
	    logTimeData.hour, logTimeData.min, logTimeData.sec,
	    logTimeData.frac);

    printf ("Time interrupt (seconds) = %g\n",
	    3600.0 * logTimeData.hour + 60.0 * logTimeData.min +
	    logTimeData.sec + (double) logTimeData.frac / 100000);

    fprintf (fp, "Time interrupt (h:m:s.s) = %d:%d:%d.%d\n",
	     logTimeData.hour, logTimeData.min, logTimeData.sec,
	     logTimeData.frac);

    fprintf (fp, "Time interrupt (seconds) = %g\n",
	     3600.0 * logTimeData.hour + 60.0 * logTimeData.min +
	     logTimeData.sec + (double) logTimeData.frac / 100000);

    fclose (fp);
}

/* dumpTcsLog -- Dump TCS log.
 */
void dumpTcsLog ()
{
    int		n, m, nmin;
    double	tmin;
    FILE	*fp;

    nmin = 1;
    tmin = logTcsData[nmin].t;

    if ((fp = fopen ("logtcs.dat", "w")) == NULL) {
	printf ("Cannot open log file\n");
	return;
    }

    /* Look for the minimum time value.
     */
    for (n = 1; n < LOG_TCS_MAX; n++)
    {
	if (logTcsData[n].t < tmin)
	{
	    nmin = n;
	    tmin = logTcsData[n].t;
	}
    }

    /* Print title.
     */
    fprintf (fp, "# index t az el\n");

    /* Print values starting from the minimum value.
     */
    for (n = nmin, m = nmin; n < nmin + LOG_TCS_MAX; n++)
    {
	fprintf (fp, "%d %.15e %g %g\n",
		 m, logTcsData[m].t, logTcsData[m].az, logTcsData[m].el);
	m++;
	m %= LOG_TCS_MAX;
    }

    fclose (fp);
}

/* dumpTrackLog -- Dump Az/El log.
 */
void dumpTrackLog ()
{
    int		i, n, m, nmin;
    double	t, tmin;
    FILE	*fp;

    nmin = 1;
    tmin = MIN3(logTrackData[nmin].t1, logTrackData[nmin].t2,
	   logTrackData[nmin].t3);

    if ((fp = fopen ("logazel.dat", "w")) == NULL) {
	printf ("Cannot open log file\n");
	return;
    }

    /* Look for the minimum time value.
     */
    for (n = 1; n < LOG_AZEL_MAX; n++)
    {
	t = MIN3 (logTrackData[n].t1, logTrackData[n].t2, logTrackData[n].t3);
	if (t < tmin)
	{
	    nmin = n;
	    tmin = t;
	}
    }

    /* Print title.
     */
    fprintf (fp,
	"# index azHs elHs recent mask toff startT least ticks t1 t2 t3 v1 v2 v3 pos vel pos vel..\n");
    fprintf (fp,
	"# %d\n", NUM_EXTRAP);

    /* Print values starting from the minimum value.
     */
    for (n = nmin, m = nmin; n < nmin + LOG_AZEL_MAX; n++)
    {
	fprintf (fp, "%d %d %d %d 0x%x %.8f %.8f %.8f %i %g %g %g %g %g %g",
		m,
		( logTrackData[m].word        & 0x0f),
		((logTrackData[m].word >> 4)  & 0x0f),
		((logTrackData[m].word >> 8)  & 0x0f),
		((logTrackData[m].word >> 12) & 0x1f),
		logTrackData[m].toff,
		logTrackData[m].startT,
		logTrackData[m].least,
		logTrackData[m].ticks,
		logTrackData[m].t1, logTrackData[m].t2, logTrackData[m].t3,
		logTrackData[m].v1, logTrackData[m].v2, logTrackData[m].v3);

	for (i = 0; i < LOG_EXTRAP_MAX; i++) {
	    fprintf (fp, " %g", logTrackData[m].pos[i]);
	    fprintf (fp, " %g", logTrackData[m].vel[i]);
	}
	fprintf (fp, "\n");

	m++;
	m %= LOG_AZEL_MAX;
    }

    fclose (fp);
}

/* dumpLogs -- Dump all logs.
 */
void dumpLogs ()
{
    printf("Starting dump Logs\n");
    dumpTcsLog ();
    dumpTrackLog ();
    printf("Dump Logs done\n");
}

/* logClean -- Clean all logging buffers.
 */
void logClean ()
{
    int	i, j;

    for (i = 0; i < LOG_TCS_MAX; i++) {
	logTcsData[i].t  = 0;
	logTcsData[i].az = 0;
	logTcsData[i].el = 0;
    }

    for (i = 0; i < LOG_AZEL_MAX; i++) {
	logTrackData[i].word = 0;
	logTrackData[i].t1   = 0;
	logTrackData[i].t2   = 0;
	logTrackData[i].t3   = 0;
	logTrackData[i].v1   = 0;
	logTrackData[i].v2   = 0;
	logTrackData[i].v3   = 0;
	for (j = 0; j < LOG_EXTRAP_MAX; j++) {
	    logTrackData[i].pos[j] = 0;
	    logTrackData[i].vel[j] = 0;
	}
    }
}


/* logFitEnable -- Reset log counters and enable trajectory calculation
 * logging.
 */
void logFitAzEnable ()
{
    logTrajCounter = 0;
    logFitFlag     = LOG_AZ_FIT;
    printf ("Trajectory calculation AZ logging enable\n");
}
void logFitElEnable ()
{
    logTrajCounter = 0;
    logFitFlag     = LOG_EL_FIT;
    printf ("Trajectory calculation EL logging enable\n");
}
void logFitEnable ()
{
    logTrajCounter = 0;
    logFitFlag     = LOG_FIT;
    printf ("Trajectory calculation logging enable\n");
}

/* logFitDisable -- Disable trajectory calculation logging.
 */
void logFitDisable ()
{
    logFitFlag = 0;
    printf ("Trajectory calculation logging disable\n");
}

/* logFitClean -- Clean all trajectory calculation logging buffers.
 */
void logFitClean ()
{
    int i;

    for (i = 0; i < LOG_AZEL_MAX; i++) 
    {
        logTrajData[i].prevVel    = 0.0;
        logTrajData[i].vel        = 0.0;
        logTrajData[i].velPos     = 0.0;
        logTrajData[i].accel      = 0.0;
        logTrajData[i].prevpa     = 0.0;
        logTrajData[i].prevpb     = 0.0;
        logTrajData[i].prevpc     = 0.0;
        logTrajData[i].pa         = 0.0;
        logTrajData[i].pb         = 0.0;
        logTrajData[i].pc         = 0.0;
        logTrajData[i].currentPos = 0.0;
        logTrajData[i].newpos     = 0.0;
        logTrajData[i].targetPos  = 0.0;
    }
}

/* dumpFit -- Dump trajectory calculation log.
 */
void dumpFit ()
{
    int         n;
    FILE        *fp;

    printf ("Starting dump of Fit logging\n");
    if ((fp = fopen ("logfit.dat", "w")) == NULL) {
        printf ("Cannot open log file\n");
        return;
    }

    /* Print title.
     */
    fprintf (fp,
        "# prevVel vel   velPos accel  prevpa prevpb prevpc pa    pb    pc   currentPos newPos targetPos \n");

    /* Print values starting from the minimum value.
     */
    for (n = 0;  n < LOG_AZEL_MAX; n++)
    {
        fprintf (fp, "%d %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f \n",
                n,
                logTrajData[n].prevVel,
                logTrajData[n].vel,
                logTrajData[n].velPos,
                logTrajData[n].accel,
                logTrajData[n].prevpa,
                logTrajData[n].prevpb,
                logTrajData[n].prevpc,
                logTrajData[n].pa,
                logTrajData[n].pb,
                logTrajData[n].pc,
                logTrajData[n].currentPos,
                logTrajData[n].newpos,
                logTrajData[n].targetPos);
    }

    printf ("Dump of Fit logging done\n");
    fclose (fp);
 
}


/* logFit -- Log the last three demands received from the TCS and the
 * demands sent to the PMAC.
 */
void logFit(double prevVel, double vel,
            double velPos, double accel,
            double prevpa, double prevpb, double prevpc,
            double pa, double pb, double pc,
            double currentPos, double newPos, double targetPos)
{
    logTrajData[logTrajCounter].prevVel     = prevVel;
    logTrajData[logTrajCounter].vel         = vel;
    logTrajData[logTrajCounter].velPos      = velPos;
    logTrajData[logTrajCounter].accel       = accel;
    logTrajData[logTrajCounter].prevpa      = prevpa;
    logTrajData[logTrajCounter].prevpb      = prevpb;
    logTrajData[logTrajCounter].prevpc      = prevpc;
    logTrajData[logTrajCounter].pa          = pa;
    logTrajData[logTrajCounter].pb          = pb;
    logTrajData[logTrajCounter].pc          = pc;
    logTrajData[logTrajCounter].currentPos  = currentPos;
    logTrajData[logTrajCounter].newpos      = newPos;
    logTrajData[logTrajCounter].targetPos   = targetPos;

    logTrajCounter++;
    logTrajCounter %= LOG_AZEL_MAX;

    if ((logTrajCounter == 0) && (logSingleFlag)) {
        logFitFlag = FALSE;
        printf ("Trajectory Calculation Logging disabled\n");
	/*dumpFit(); */
    }
}


/* pmac log methods */

/* logPmacEnable -- Reset log counters and enable trajectory calculation
 * logging.
 */
void logPmacAzEnable ()
{
    logPmacCounter = 0;
    logPmacFlag     = LOG_AZ_PMAC;
    printf ("Pmac AZ logging enable\n");
}
void logPmacElEnable ()
{
    logPmacCounter = 0;
    logPmacFlag     = LOG_EL_PMAC;
    printf ("Pmac EL logging enable\n");
}
void logPmacEnable ()
{
    logPmacCounter = 0;
    logPmacFlag     = LOG_PMAC;
    printf ("Pmac logging enable\n");
}

/* logPmacDisable -- Disable trajectory calculation logging.
 */
void logPmacDisable ()
{
    logPmacFlag = 0;
    printf ("Pmac logging disable\n");
}

/* logPmacClean -- Clean all trajectory calculation logging buffers.
 */
void logPmacClean ()
{
    int i,j;

    for (i = 0; i < LOG_AZEL_MAX; i++) 
    {
        for (j = 0; j < NUM_EXTRAP; j++) {
            logPmacData[i].pos[j] = 0.0;
            logPmacData[i].vel[j] = 0.0;
        }
        logPmacData[i].lastPos      = 0.0;
        logPmacData[i].lastVel      = 0.0;
        logPmacData[i].acceleration = 0.0;
    }
}

/* dumpPmac -- Dump Pmac log.
 */
void dumpPmac ()
{
    int         i, n;
    FILE        *fp;
    FILE        *fpvel;
    FILE        *fpfit;

    printf ("Starting dump of Pmac logging\n");
    if ((fp    = fopen ("logPmacDemand.dat", "w")) == NULL) {
        printf ("Cannot open PmaclogDemand file\n");
        return;
    }
    if ((fpvel = fopen ("logPmacVel.dat", "w")) == NULL) {
        printf ("Cannot open PmaclogVel file\n");
        return;
    }
    if ((fpfit = fopen ("logPmacFit.dat", "w")) == NULL) {
        printf ("Cannot open PmaclogFit file\n");
        return;
    }

    /* Print values starting from the minimum value.
     */

    fprintf (fpfit, "lastPos lastVel acceleration \n"); 
    for (n = 0;  n < LOG_AZEL_MAX; n++)
    {   n %= LOG_AZEL_MAX;

        fprintf (fpfit, "%.8f %.8f %.8f \n ",logPmacData[n].lastPos, logPmacData[n].lastVel, logPmacData[n].acceleration); 

        for (i = 0; i < NUM_EXTRAP; i++) {
            fprintf (fp, "%.8f\n  ", logPmacData[n].pos[i]);
            fprintf (fpvel, "%.8f  ", logPmacData[n].vel[i]);
        }
        /*fprintf (fp, "\n ");*/
        fprintf (fpvel, "\n ");
    }

    fclose (fp);
    printf ("Dump of Pmac logging done\n");
}


/* logPmac -- Log the  demands and velocity sent to the PMAC.
 */
void logPmac (double pos[], double vel[], double lastPos, double lastVel,
              double acceleration)
{
int i;

    for ( i = 0; i<NUM_EXTRAP; i++)
    { 
    logPmacData[logPmacCounter].pos[i] = pos[i];
    logPmacData[logPmacCounter].vel[i] = vel[i];
    }
    logPmacData[logPmacCounter].lastPos      = lastPos;
    logPmacData[logPmacCounter].lastVel      = lastVel;
    logPmacData[logPmacCounter].acceleration = acceleration;
    logPmacCounter = logPmacCounter + 1;
    logPmacCounter %= LOG_AZEL_MAX;

    if ((logPmacCounter == 0) && (logSingleFlag)) {
        logPmacFlag = FALSE;
        printf ("Pmac Logging disabled\n");
	/*dumpPmac();*/
    }
}

/***********************************************************************/

/* logCalcEnable -- Reset log counters and enable Calcectory calculation
 * logging.
 */
void logCalcAzEnable ()
{
    logCalcCounter = 0;
    logCalcFlag     = LOG_AZ_CALC;
    printf ("Calc AZ logging enable\n");
}
void logCalcElEnable ()
{
    logCalcCounter = 0;
    logCalcFlag     = LOG_EL_CALC;
    printf ("Calc EL logging enable\n");
}

/* logCalcDisable -- Disable Calcectory calculation logging.
 */
void logCalcDisable ()
{
    logCalcFlag = 0;
    printf ("Calc logging disable\n");
}

/* logCalcClean -- Clean all Calc logging buffers.
 */
void logCalcClean ()
{
    int i;

    for (i = 0; i < LOG_AZEL_MAX; i++) 
    {
        logCalcData[i].toff = 0.0;
        logCalcData[i].ta   = 0.0;
        logCalcData[i].tb   = 0.0;
        logCalcData[i].tc   = 0.0;
        logCalcData[i].pa   = 0.0;
        logCalcData[i].pb   = 0.0;
        logCalcData[i].pc   = 0.0;
        logCalcData[i].A    = 0.0;
        logCalcData[i].B    = 0.0;
        logCalcData[i].C    = 0.0;
        logCalcData[i].pos  = 0.0;
    }
}

/* dumpCalc -- Dump trajectory calculation log.
 */
void dumpCalc ()
{
    int         n;
    FILE        *fp;

    printf ("Starting dump of Trajectory Calculation logging\n");
    if ((fp = fopen ("logCalc.dat", "w")) == NULL) {
        printf ("Cannot open log file\n");
        return;
    }

    /* Print title.
     */
    fprintf (fp,
        "# timeoffset ta tb tc pa    pb    pc   Afactor Bfactor  Cfactor  Pos Vel \n");

    /* Print values starting from the minimum value.
     */
    for (n = 0;  n < LOG_AZEL_MAX; n++)
    {
        fprintf (fp, "%d %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f %.8f \n",
                n,
                logCalcData[n].toff,
                logCalcData[n].ta, 
                logCalcData[n].tb,
                logCalcData[n].tc,
                logCalcData[n].pa,
                logCalcData[n].pb,
                logCalcData[n].pc,
                logCalcData[n].A, 
                logCalcData[n].B, 
                logCalcData[n].C,
                logCalcData[n].pos,
                logCalcData[n].vel);
    }

    fclose (fp);
    printf ("Dump of Trajectory Calculation logging done\n");
 
}


/* logCalc -- Log the last three demands received from the TCS and the
 * demands sent to the PMAC.
 */
void logCalc(double toff, double ta, double tb, double tc,
            double pa, double pb, double pc, double A,
            double B, double C, double pos, double vel)
{
    logCalcData[logCalcCounter].toff = toff;
    logCalcData[logCalcCounter].ta   = ta;
    logCalcData[logCalcCounter].tb   = tb;
    logCalcData[logCalcCounter].tc   = tc;
    logCalcData[logCalcCounter].pa   = pa;
    logCalcData[logCalcCounter].pb   = pb;
    logCalcData[logCalcCounter].pc   = pc;
    logCalcData[logCalcCounter].A    = A;
    logCalcData[logCalcCounter].B    = B;
    logCalcData[logCalcCounter].C    = C;
    logCalcData[logCalcCounter].pos  = pos;
    logCalcData[logCalcCounter].vel  = vel;

    logCalcCounter++;
    logCalcCounter %= LOG_AZEL_MAX;

    if ((logCalcCounter == 0) && (logSingleFlag)) {
        logCalcFlag = FALSE;
        printf ("Calc Logging disabled\n");
	/*dumpCalc(); */
    }
}
/* -----------------------------------------------------------------*/
#endif
epicsRegisterFunction(dumpPmac);
epicsRegisterFunction(dumpFit);
epicsRegisterFunction(dumpCalc);
epicsRegisterFunction(dumpLogs);
epicsRegisterFunction(logTracking);
epicsRegisterFunction(waitLogTrackingEnd);
epicsRegisterFunction(Tracking);
epicsRegisterFunction(readTime);
epicsRegisterFunction(trackingFO);
epicsRegisterFunction(initFollowA);
epicsRegisterFunction(FollowA);
epicsRegisterFunction(TrajecCalc);
