#ifndef __FOLLOW_H__
#define __FOLLOW_H__

#define NUM_EXTRAP	20	/* number of points to extrapolate    */

typedef struct {
	int    firstAzFit;
	int    firstElFit;
	double azA;
	double azB;
	double azC;
	double elA;
	double elB;
	double elC;
	double lastAzVelocity;
	double lastElVelocity;
	double prevAzVel;
	double prevAzDemand[3];
	double prevElVel;
	double prevElDemand[3];
} mcs_parameters;

long fillBuffer		(double *, double *, double *, double *, double *,
			 double, long, double *, double, double, double,
			 double, double, long, int, mcs_parameters *);
long calc_coeffs	(double *, double *, double *, double *, double *,
			 double *);
int calc_linear		(double, double, double, double, double, double,
			 double, double *, double *, double *);
int calc_quadratic	(double, double, double, double, double, double,
			 double, double *, double *, double * );
int fit_new_AZ_demand   (double, double *, double, double *, double,
			 double *, double, double, double, long, int,
			 mcs_parameters *);
int fit_new_EL_demand   (double, double *, double, double *, double,
			 double *, double, double, double, long, int,
			 mcs_parameters *);

#endif // __FOLLOW_H__
