#include "kBlack.h"
#include "kSolver.h"
#include "kFd1d.h"

class kBlackObj : public kSolverObjective
{
public:

	//	constructor
	kBlackObj(
		double	expiry,
		double	strike,
		double	price,
		double	forward)
		: kSolverObjective(),
		myExpiry(expiry),
		myStrike(strike),
		myPrice(price),
		myForward(forward)
	{}

	//	value
	virtual double	value(double x)
	{
		double res = kBlack::call(myExpiry, myStrike, myForward, x) - myPrice;

		//	done
		return res;
	}

	//	deriv
	virtual double	deriv(double x)
	{
		double res = kBlack::vega(myExpiry, myStrike, myForward, x);

		//	done
		return res;
	}

	//	private parts
private:

	//	expiry
	double	myExpiry;
	double	myStrike;
	double	myPrice;
	double	myForward;

};

// implied vol
double
kBlack::implied(
	double	expiry,
	double	strike,
	double	price,
	double	forward)
{
	//	calc intrinsic
	double intrinc = max(forward - strike,0.0);
	if (price <= intrinc) return 0.0;

	//	objective
	kBlackObj obj(expiry, strike, price, forward);

	//	start guess
	double volatility = 0.1;
	int    numIter = 10;
	double epsilon = (price - intrinc) * kConstants::epsilon();

	//	solve
	kSolver::newtonRapson(obj, volatility, numIter, epsilon, nullptr);

	//	bound
	volatility = max(0.0, volatility);

	//	done
	return volatility;
}

//	fd runner
bool
kBlack::fdRunner(
	const double		s0,
	const double		r,
	const double		mu,
	const double		sigma,
	const double		expiry,
	const double		strike,
	const bool			dig,
	const int			pc,			//	put (-1) call (1)
	const int			ea,			//	european (0), american (1)
	const int			smooth,		//	smoothing
	const double		theta,
	const int			wind,
	const double		numStd,
	const int			numT,
	const int			numS,
	const bool			update,
	const int			numPr,
	double& res0,
	kVector<double>& s,
	kVector<double>& res,
	string& error)
{
	//	helps
	int h, i, p;

	//	construct s axis
	double t = max(0.0, expiry);
	double std = sqrt(exp(2*mu*t)) * sqrt(exp(2*sigma*t)-1); //sigma * sqrt(t); // check this if weird prices
	double sl = s0 * exp(-numStd * std); // s0 - numStd * std;
	double su = s0 * exp(numStd * std); //s0 + numStd * std;
	int    nums = 2 * (max(0, numS) / 2 + 1);
	if (numS <= 1 || sl == su)
	{
		nums = 1;
	}
	double ds = (su - sl) / nums;
	s.resize(nums);
	s(0) = sl;
	for (i = 1; i < nums; ++i)
	{
		// this
		s(i) = s0 * exp((i - nums / 2.0) * ds); // s(i - 1) + ds; // ; //s(i - 1) + ds;
	}

	//	construct fd grid
	kFd1d<double> fd;
	fd.init(1, s, false);

	//	set terminal result
	double xl, xu;
	res.resize(nums);
	for (i = 0; i < nums; ++i)
	{
		if (smooth == 0 || i == 0 || i == nums - 1)
		{
			if (dig) res(i) = 0.5 * (kInlines::sign(s(i) - strike) + 1.0);
			else    res(i) = max(0.0, s(i) - strike);
		}
		else
		{
			xl = 0.5 * (s(i - 1) + s(i)); // lower bound
			xu = 0.5 * (s(i) + s(i + 1)); // upper bound
			if (dig) res(i) = kFiniteDifference::smoothDigital(xl, xu, strike);
			else	 res(i) = kFiniteDifference::smoothCall(xl, xu, strike);
		}

		if (pc < 0)
		{
			if (dig) res(i) = 1.0 - res(i);
			else    res(i) -= (s(i) - strike);
		}
	}

	//	time steps
	int    numt = max(0, numT);
	double dt = t / max(1, numt);

	//	repeat
	int nump = max(1, numPr);
	for (p = 0; p < nump; ++p)
	{
		//	set parameters
		for (i = 0; i < nums; ++i)
		{
			// this
			fd.r()(i) = r; 
			fd.mu()(i) = mu * s(i); //mu;
			fd.var()(i) = sigma * sigma * s(i); //sigma * sigma;
		}

		//	roll
		fd.res()(0) = res;
		for (h = numt - 1; h >= 0; --h)
		{
			fd.rollBwd(dt, update || h == (numt - 1), theta, wind, fd.res());
			if (ea > 0)
			{
				for (i = 0; i < nums; ++i) fd.res()(0)(i) = max(res(i), fd.res()(0)(i));
			}
		}
	}

	//	set result
	res = fd.res()(0);
	res0 = fd.res()(0)(nums / 2);

	//	done
	return true;
}